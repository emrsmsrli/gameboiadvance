/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DEBUGGER_HELPERS_H
#define GAMEBOIADVANCE_DEBUGGER_HELPERS_H

#include <fmt/format.h>

#include <gba/helper/macros.h>

namespace gba::debugger {

FORCEINLINE constexpr const char* fmt_bool(const bool val) noexcept
{
    return val ? "true" : "false";
}

template<typename T>
FORCEINLINE std::string fmt_bin(const T val) noexcept
{
    if constexpr(sizeof(T) == 8) {
        return fmt::format("{:08B}", val);
    } else if constexpr(sizeof(T) == 4) {
        return fmt::format("{:04B}", val);
    } else if constexpr(sizeof(T) == 2) {
        return fmt::format("{:02B}", val);
    } else {
        return fmt::format("{:01B}", val);
    }
}

template<typename T>
FORCEINLINE std::string fmt_hex(const T val) noexcept
{
    if constexpr(sizeof(T) == 8) {
        return fmt::format("{:08X}", val);
    } else if constexpr(sizeof(T) == 4) {
        return fmt::format("{:04X}", val);
    } else if constexpr(sizeof(T) == 2) {
        return fmt::format("{:02X}", val);
    } else {
        return fmt::format("{:01X}", val);
    }
}

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DEBUGGER_HELPERS_H
