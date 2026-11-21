#pragma once

#include "base/basic_types.h"

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
} // namespace Ui

namespace UiSandbox {

void EditConfigBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);

} // namespace UiSandbox
