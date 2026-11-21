#pragma once

#include "data/data_types.h"

namespace Data {
struct StarGiftBackground;
struct UniqueGift;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
class PopupMenu;
} // namespace Ui

namespace UiSandbox {

struct GiftPreset;

[[nodiscard]] GiftPreset MakeGiftPreset(
	DocumentId documentId,
	QString title,
	QString text,
	const Data::UniqueGift *unique,
	const Data::StarGiftBackground *background = nullptr);

enum class GiftStoreResult {
	Saved,
	AlreadyAdded,
	InsufficientBalance,
	Failed,
};
[[nodiscard]] GiftStoreResult StoreGift(GiftPreset preset, int64 cost = 0);
[[nodiscard]] QString GiftStoreMessage(GiftStoreResult result);


void EditConfigBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);
void AddCopyDocumentIdAction(
	not_null<Ui::PopupMenu*> menu,
	DocumentId id,
	bool showId = false);

} // namespace UiSandbox
