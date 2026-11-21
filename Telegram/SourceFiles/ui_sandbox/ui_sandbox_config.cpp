#include "ui_sandbox/ui_sandbox_config.h"

#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

#include <algorithm>

namespace UiSandbox {
namespace {

QString ConfigPath() {
	return QDir(cWorkingDir()).filePath(u"ui_sandbox.json"_q);
}

Values ReadValues(const QJsonObject &object) {
	const auto emoji = object.value(u"emojiStatusId"_q);
	const auto numericId = emoji.toInteger();
	auto result = Values{
		.premium = object.value(u"premium"_q).toBool(),
		.verified = object.value(u"verified"_q).toBool(),
		.emojiStatusId = emoji.isString()
			? emoji.toString().toULongLong()
			: static_cast<uint64>(std::max(qint64(0), numericId)),
		.phone = object.value(u"phone"_q).toString(),
		.username = object.value(u"username"_q).toString(),
		.collectibleUsernames = {},
		.rating = object.value(u"rating"_q).toInt(),
	};
	for (const auto entry : object.value(u"collectibleUsernames"_q).toArray()) {
		const auto username = entry.toString();
		if (!username.isEmpty()) {
			result.collectibleUsernames.push_back(username);
		}
	}
	return result;
}

} // namespace

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
	const auto object = QJsonObject{
		{ u"premium"_q, values.premium },
		{ u"verified"_q, values.verified },
		{ u"emojiStatusId"_q, QString::number(values.emojiStatusId) },
		{ u"phone"_q, values.phone },
		{ u"username"_q, values.username },
		{ u"collectibleUsernames"_q, QJsonArray::fromStringList(values.collectibleUsernames) },
		{ u"rating"_q, values.rating },
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
