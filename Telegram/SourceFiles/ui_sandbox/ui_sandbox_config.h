#pragma once

#include "base/basic_types.h"

#include <QtCore/QStringList>
#include <rpl/event_stream.h>

#include <vector>

namespace Main {
class Session;
} // namespace Main

namespace UiSandbox {

struct DocumentUpdate {
	Main::Session *session = nullptr;
	uint64 id = 0;
};

struct GiftPreset {
	uint64 documentId = 0;
	QString title;
	int num = 0;
	QString centerColor;
	QString edgeColor;
	uint64 patternDocumentId = 0;

	friend bool operator==(const GiftPreset &, const GiftPreset &) = default;
};

[[nodiscard]] bool IsGiftColor(const QString &value);

struct Values {
	bool premium = false;
	bool verified = false;
	uint64 emojiStatusId = 0;
	QString phone;
	QString username;
	QStringList collectibleUsernames;
	int rating = 0;
	std::vector<GiftPreset> gifts;
};

class Config final {
public:
	[[nodiscard]] static Config &Instance();

	[[nodiscard]] const Values &current() const;
	[[nodiscard]] rpl::producer<> changes() const;
	[[nodiscard]] rpl::producer<DocumentUpdate> documentChanges() const;
	void notifyDocumentChanged(DocumentUpdate update);
	[[nodiscard]] bool reload();
	[[nodiscard]] bool save(Values values);

private:
	Config();
	Config(const Config &) = delete;
	Config &operator=(const Config &) = delete;

	Values _values;
	rpl::event_stream<> _changes;
	rpl::event_stream<DocumentUpdate> _documentChanges;
};

} // namespace UiSandbox
