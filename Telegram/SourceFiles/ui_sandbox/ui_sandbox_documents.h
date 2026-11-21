#pragma once

#include "base/basic_types.h"

namespace Main {
class Session;
} // namespace Main

namespace UiSandbox {

void PreloadDocuments(not_null<Main::Session*> session);

} // namespace UiSandbox
