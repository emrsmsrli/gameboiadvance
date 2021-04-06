/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_VERSION_H
#define GAMEBOIADVANCE_VERSION_H

#include <string_view>

namespace gba {

[[maybe_unused]] constexpr auto version_major = 0;
[[maybe_unused]] constexpr auto version_minor = 7;
[[maybe_unused]] constexpr auto version_patch = 1;

[[maybe_unused]] constexpr std::string_view version = "0.7.1";

} // namespace gba

#endif //GAMEBOIADVANCE_VERSION_H
