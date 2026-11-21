#include "ui_sandbox/ui_sandbox_config.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <crl/crl_time.h>
#include <rpl/distinct_until_changed.h>
#include <rpl/map.h>

#include "settings.h"

#include <algorithm>

namespace UiSandbox {
namespace {

constexpr auto kOptionKeys = std::array{
	"hideGifts",
	"overrideStarsBalance",
	"glassHub",
	"enterToSend",
	"characterCount",
	"seconds",
	"exactViews",
	"noVoiceAutoplay",
	"hidePhone",
	"hideTyping",
	"keepUnread",
	"readOnTyping",
	"hideOnline",
	"hideSponsored",
	"peerId",
	"creationDate",
	"hideDevices",
	"hideFolders",
	"hidePrivacy",
	"hideLanguage",
	"hideFaq",
	"squareAvatars",
	"glassProfile",
	"snowProfile",
	"blurProfileAvatar",
	"confirmCall",
	"filterZalgo",
	"revealSpoilers",
	"quickFormatting",
	"reactionGlow",
	"glassGifts",
};
static_assert(kOptionKeys.size() == static_cast<size_t>(Option::Count));

QString ConfigPath() {
	return QDir(cWorkingDir()).filePath(u"ui_sandbox.json"_q);
}

uint64 ReadId(const QJsonValue &value) {
	return value.isString()
		? value.toString().toULongLong()
		: static_cast<uint64>(std::max(qint64(0), value.toInteger()));
}

bool WriteConfig(const QJsonObject &object) {
	const auto bytes = QJsonDocument(object).toJson();
	auto file = QSaveFile(ConfigPath());
	return file.open(QIODevice::WriteOnly)
		&& file.write(bytes) == static_cast<qint64>(bytes.size())
		&& file.commit();
}

Values ReadValues(const QJsonObject &object) {
	auto result = Values{
		.premium = object.value(u"premium"_q).toBool(),
		.verified = object.value(u"verified"_q).toBool(),
		.emojiStatusId = ReadId(object.value(u"emojiStatusId"_q)),
		.phone = object.value(u"phone"_q).toString(),
		.username = object.value(u"username"_q).toString(),
		.collectibleUsernames = {},
		.rating = object.value(u"rating"_q).toInt(),
		.virtualPurchases = object.value(u"virtualPurchases"_q).toBool(),
		.virtualStars = std::max(qint64(0), object.value(u"virtualStars"_q).toInteger(kDefaultVirtualStars)),
		.gifts = {},
		.options = {},
		.verifiedBadgeId = ReadId(object.value(u"verifiedBadgeId"_q)),
		.registrationDate = object.value(u"registrationDate"_q).toString(),
		.activity = object.value(u"activity"_q).toString(),
		.notes = object.value(u"notes"_q).toString(),
		.profileBanner = object.value(u"profileBanner"_q).toString(),
		.stickerScale = std::clamp(object.value(u"stickerScale"_q).toInt(100), 50, 150),
	};
	for (const auto entry : object.value(u"collectibleUsernames"_q).toArray()) {
		const auto username = entry.toString();
		if (!username.isEmpty()) {
			result.collectibleUsernames.push_back(username);
		}
	}
	for (const auto item : object.value(u"gifts"_q).toArray()) {
		const auto gift = item.toObject();
		auto preset = GiftPreset{
			.documentId = ReadId(gift.value(u"documentId"_q)),
			.title = gift.value(u"title"_q).toString().trimmed(),
			.num = std::max(0, gift.value(u"num"_q).toInt()),
			.centerColor = gift.value(u"centerColor"_q).toString(),
			.edgeColor = gift.value(u"edgeColor"_q).toString(),
			.patternDocumentId = ReadId(gift.value(u"patternDocumentId"_q)),
			.text = gift.value(u"text"_q).toString(),
			.attributes = {},
			.patternColor = gift.value(u"patternColor"_q).toString(),
			.textColor = gift.value(u"textColor"_q).toString(),
			.sourceSlug = gift.value(u"sourceSlug"_q).toString(),
		};
		for (const auto attribute : gift.value(u"attributes"_q).toArray()) {
			if (attribute.isString()) {
				preset.attributes.push_back(attribute.toString());
			}
		}
		if (preset.documentId && !preset.title.isEmpty()
			&& IsGiftColor(preset.centerColor)
			&& IsGiftColor(preset.edgeColor)) {
			result.gifts.push_back(std::move(preset));
		}
	}
	const auto options = object.value(u"options"_q).toObject();
	for (auto i = size_t(0); i != kOptionKeys.size(); ++i) {
		result.options[i] = options.value(
			QString::fromLatin1(kOptionKeys[i])).toBool();
	}
	if (!options.contains(u"overrideStarsBalance"_q)) {
		result.options[static_cast<size_t>(Option::OverrideStarsBalance)]
			= result.virtualPurchases;
	}
	return result;
}

} // namespace

bool IsGiftColor(const QString &value) {
	static const auto hex = QRegularExpression(u"^#[0-9A-Fa-f]{6}$"_q);
	return value.size() == 7 && hex.match(value).hasMatch();
}

Config &Config::Instance() {
	static auto instance = Config();
	return instance;
}

Config::Config() {
	if (!reload()) {
		_values = {};
	}
}

const Values &Config::current() const {
	return _values;
}

rpl::producer<> Config::changes() const {
	return _changes.events();
}

rpl::producer<DocumentUpdate> Config::documentChanges() const {
	return _documentChanges.events();
}

void Config::notifyDocumentChanged(DocumentUpdate update) {
	_documentChanges.fire(std::move(update));
}

bool Config::reload() {
	auto file = QFile(ConfigPath());
	if (!file.exists()) {
		_values = {};
	} else {
		if (!file.open(QIODevice::ReadOnly)) {
			return false;
		}
		const auto document = QJsonDocument::fromJson(file.readAll());
		if (!document.isObject()) {
			return false;
		}
		_values = ReadValues(document.object());
	}
	_changes.fire({});
	return true;
}

namespace {

QJsonObject EncodeValues(const Values &values) {
	auto gifts = QJsonArray();
	for (const auto &gift : values.gifts) {
		gifts.append(QJsonObject{
			{ u"documentId"_q, QString::number(gift.documentId) },
			{ u"title"_q, gift.title },
			{ u"num"_q, gift.num },
			{ u"centerColor"_q, gift.centerColor },
			{ u"edgeColor"_q, gift.edgeColor },
			{ u"patternDocumentId"_q, QString::number(gift.patternDocumentId) },
			{ u"text"_q, gift.text },
			{ u"attributes"_q, QJsonArray::fromStringList(gift.attributes) },
			{ u"patternColor"_q, gift.patternColor },
			{ u"textColor"_q, gift.textColor },
			{ u"sourceSlug"_q, gift.sourceSlug },
		});
	}
	auto options = QJsonObject();
	for (auto i = size_t(0); i != kOptionKeys.size(); ++i) {
		options.insert(QString::fromLatin1(kOptionKeys[i]), values.options[i]);
	}
	return {
		{ u"sandboxFormat"_q, 1 },
		{ u"premium"_q, values.premium },
		{ u"verified"_q, values.verified },
		{ u"emojiStatusId"_q, QString::number(values.emojiStatusId) },
		{ u"phone"_q, values.phone },
		{ u"username"_q, values.username },
		{ u"collectibleUsernames"_q, QJsonArray::fromStringList(values.collectibleUsernames) },
		{ u"rating"_q, values.rating },
		{ u"virtualPurchases"_q, values.virtualPurchases },
		{ u"virtualStars"_q, static_cast<qint64>(values.virtualStars) },
		{ u"gifts"_q, gifts },
		{ u"options"_q, options },
		{ u"verifiedBadgeId"_q, QString::number(values.verifiedBadgeId) },
		{ u"registrationDate"_q, values.registrationDate },
		{ u"activity"_q, values.activity },
		{ u"notes"_q, values.notes },
		{ u"profileBanner"_q, values.profileBanner },
		{ u"stickerScale"_q, values.stickerScale },
	};
}

} // namespace

bool Config::save(Values values) {
	values.collectibleUsernames.removeAll(QString());
	if (!WriteConfig(EncodeValues(values))) {
		return false;
	}
	_values = std::move(values);
	_changes.fire({});
	return true;
}

bool Config::reset() {
	if (!WriteConfig({})) {
		return false;
	}
	_values = {};
	_changes.fire({});
	return true;
}

QByteArray Config::exportJson() const {
	return QJsonDocument(EncodeValues(_values)).toJson();
}

bool Config::importJson(const QByteArray &bytes) {
	if (bytes.isEmpty() || bytes.size() > 4 * 1024 * 1024) {
		return false;
	}
	const auto doc = QJsonDocument::fromJson(bytes);
	if (!doc.isObject()) {
		return false;
	}
	const auto object = doc.object();
	const auto format = object.value(u"sandboxFormat"_q);
	if (format.isUndefined()) {
		if (!object.contains(u"premium"_q)
			|| !object.contains(u"gifts"_q)) {
			return false;
		}
	} else if (format.toInt() != 1) {
		return false;
	}
	return save(ReadValues(object));
}

bool Enabled(Option option) {
	return Config::Instance().current().options[static_cast<size_t>(option)];
}

rpl::producer<bool> OptionValue(Option option) {
	return rpl::single() | rpl::then(
		Config::Instance().changes()
	) | rpl::map([=] {
		return Enabled(option);
	}) | rpl::distinct_until_changed();
}

} // namespace UiSandbox
