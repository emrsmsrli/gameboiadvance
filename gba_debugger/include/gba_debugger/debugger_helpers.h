/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DEBUGGER_HELPERS_H
#define GAMEBOIADVANCE_DEBUGGER_HELPERS_H

#include <gba/helper/macros.h>

namespace gba::debugger {

FORCEINLINE constexpr const char* fmt_bool(const bool val) noexcept
{
    return val ? "true" : "false";
}

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DEBUGGER_HELPERS_H
