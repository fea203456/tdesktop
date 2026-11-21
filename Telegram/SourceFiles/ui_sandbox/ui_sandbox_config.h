#pragma once

#include "base/basic_types.h"

#include <QtCore/QStringList>
#include <rpl/event_stream.h>

namespace UiSandbox {

struct Values {
	bool premium = false;
	bool verified = false;
	uint64 emojiStatusId = 0;
	QString phone;
	QString username;
	QStringList collectibleUsernames;
	int rating = 0;
};

class Config final {
public:
	[[nodiscard]] static Config &Instance();

	[[nodiscard]] const Values &current() const;
	[[nodiscard]] rpl::producer<> changes() const;
	[[nodiscard]] bool reload();
	[[nodiscard]] bool save(Values values);

private:
	Config();
	Config(const Config &) = delete;
	Config &operator=(const Config &) = delete;

	Values _values;
	rpl::event_stream<> _changes;
};

} // namespace UiSandbox
