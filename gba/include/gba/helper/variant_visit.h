/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_VARIANT_VISIT_H
#define GAMEBOIADVANCE_VARIANT_VISIT_H

#include <variant>

namespace gba {

template<class... Ts> struct overload : Ts ... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

template<size_t N, typename R, typename Variant, typename Visitor>
[[nodiscard]] constexpr R visit_nt(Variant&& var, Visitor&& vis)
{
    if constexpr(N == 0) {
        if(N == var.index()) {
            // If this check isnt there the compiler will generate
            // exception code, this stops that
            return std::forward<Visitor>(vis)(std::get<N>(std::forward<Variant>(var)));
        }
    } else {
        if(var.index() == N) {
            return std::forward<Visitor>(vis)(std::get<N>(std::forward<Variant>(var)));
        }
        return visit_nt<N - 1, R>(std::forward<Variant>(var), std::forward<Visitor>(vis));
    }
    while(true) {} // unreachable
}

template<class... Args, typename Visitor, typename... Visitors>
[[nodiscard]] constexpr decltype(auto) visit_nt(
  const std::variant<Args...>& var,
  Visitor&& vis, Visitors&& ... visitors)
{
    auto ol = overload{std::forward<Visitor>(vis), std::forward<Visitors>(visitors)...};
    using result_t = decltype(std::invoke(std::move(ol), std::get<0>(var)));

    static_assert(sizeof...(Args) > 0);
    return visit_nt<sizeof...(Args) - 1, result_t>(var, std::move(ol));
}

template<class... Args, typename Visitor, typename... Visitors>
[[nodiscard]] constexpr decltype(auto) visit_nt(std::variant<Args...>& var, Visitor&& vis, Visitors&& ... visitors)
{
    auto ol = overload{std::forward<Visitor>(vis), std::forward<Visitors>(visitors)...};
    using result_t = decltype(std::invoke(std::move(ol), std::get<0>(var)));

    static_assert(sizeof...(Args) > 0);
    return visit_nt<sizeof...(Args) - 1, result_t>(var, std::move(ol));
}

template<class... Args, typename Visitor, typename... Visitors>
[[nodiscard]] constexpr decltype(auto) visit_nt(std::variant<Args...>&& var, Visitor&& vis, Visitors&& ... visitors)
{
    auto ol = overload{std::forward<Visitor>(vis), std::forward<Visitors>(visitors)...};
    using result_t = decltype(std::invoke(std::move(ol), std::move(std::get<0>(var))));

    static_assert(sizeof...(Args) > 0);
    return visit_nt<sizeof...(Args) - 1, result_t>(std::move(var), std::move(ol));
}

template<typename Value, typename... Visitors>
inline constexpr bool is_visitable_v = (std::is_invocable_v<Visitors, Value> or ...);

} // namespace gba

#endif //GAMEBOIADVANCE_VARIANT_VISIT_H
