#include "ui_sandbox/ui_sandbox_config.h"

#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QRegularExpression>

#include <algorithm>

namespace UiSandbox {
namespace {

QString ConfigPath() {
	return QDir(cWorkingDir()).filePath(u"ui_sandbox.json"_q);
}

uint64 ReadId(const QJsonValue &value) {
	return value.isString()
		? value.toString().toULongLong()
		: static_cast<uint64>(std::max(qint64(0), value.toInteger()));
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
		.gifts = {},
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
		};
		if (preset.documentId && !preset.title.isEmpty()
			&& IsGiftColor(preset.centerColor)
			&& IsGiftColor(preset.edgeColor)) {
			result.gifts.push_back(std::move(preset));
		}
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

bool Config::save(Values values) {
	values.collectibleUsernames.removeAll(QString());
	auto gifts = QJsonArray();
	for (const auto &gift : values.gifts) {
		gifts.append(QJsonObject{
			{ u"documentId"_q, QString::number(gift.documentId) },
			{ u"title"_q, gift.title },
			{ u"num"_q, gift.num },
			{ u"centerColor"_q, gift.centerColor },
			{ u"edgeColor"_q, gift.edgeColor },
			{ u"patternDocumentId"_q, QString::number(gift.patternDocumentId) },
		});
	}
	const auto object = QJsonObject{
		{ u"premium"_q, values.premium },
		{ u"verified"_q, values.verified },
		{ u"emojiStatusId"_q, QString::number(values.emojiStatusId) },
		{ u"phone"_q, values.phone },
		{ u"username"_q, values.username },
		{ u"collectibleUsernames"_q, QJsonArray::fromStringList(values.collectibleUsernames) },
		{ u"rating"_q, values.rating },
		{ u"gifts"_q, gifts },
	};
	const auto bytes = QJsonDocument(object).toJson();
	auto file = QSaveFile(ConfigPath());
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(bytes) != static_cast<qint64>(bytes.size())
		|| !file.commit()) {
		return false;
	}
	_values = std::move(values);
	_changes.fire({});
	return true;
}

} // namespace UiSandbox
