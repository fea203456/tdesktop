#pragma once

#include "data/data_types.h"

namespace Ui {
class GenericBox;
class PopupMenu;
} // namespace Ui

namespace UiSandbox {

void EditConfigBox(not_null<Ui::GenericBox*> box);
void AddCopyDocumentIdAction(
	not_null<Ui::PopupMenu*> menu,
	DocumentId id,
	bool showId = false);

} // namespace UiSandbox
