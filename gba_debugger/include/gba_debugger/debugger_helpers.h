/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DEBUGGER_HELPERS_H
#define GAMEBOIADVANCE_DEBUGGER_HELPERS_H

#include <optional>

#include <fmt/format.h>
#include <imgui.h>

namespace ImGui {

template <typename S, typename... Args>
void Text(const S& format_str, Args&&... args)
{
    TextUnformatted(fmt::format(format_str, std::forward<Args>(args)...).c_str());
}

} // namespace ImGui

template<typename T>
struct fmt::formatter<std::optional<T>> : formatter<T> {
    template<typename FormatContext>
    auto format(const std::optional<T>& o, FormatContext& ctx) -> decltype(ctx.out())
    {
        if(o.has_value()) {
            return formatter<T>::format(o.value(), ctx);
        }
        return format_to(ctx.out(), "{}", "<none>");
    }
};

#endif //GAMEBOIADVANCE_DEBUGGER_HELPERS_H
