/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DEBUGGER_HELPERS_H
#define GAMEBOIADVANCE_DEBUGGER_HELPERS_H

#include <string>
#include <optional>

#include <fmt/format.h>
#include <imgui.h>

#include <gba/helper/macros.h>

namespace ImGui {

template <typename S, typename... Args>
void Text(const S& format_str, Args&&... args)
{
    TextUnformatted(fmt::format(format_str, std::forward<Args>(args)...).c_str());
}

} // namespace ImGui

namespace gba::debugger {

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

template<typename T>
struct fmt::formatter<std::optional<T>> : formatter<T> {
    template<typename FormatContext>
    auto format(const std::optional<T>& o, FormatContext& ctx) -> decltype(ctx.out())
    {
        if(o.has_value()) {
            return formatter<T>::format(o.value(), ctx);
        }
        return format_to(ctx.out(), "{}", "<nullopt>");
    }
};

#endif //GAMEBOIADVANCE_DEBUGGER_HELPERS_H
