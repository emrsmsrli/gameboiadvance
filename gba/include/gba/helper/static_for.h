/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */


#ifndef GAMEBOIADVANCE_STATIC_FOR_H
#define GAMEBOIADVANCE_STATIC_FOR_H

#include <type_traits>

namespace gba {

template<typename T, T First, T Last, typename F>
constexpr void static_for(F&& f) noexcept
{
    if constexpr(First < Last) {
        f(std::integral_constant<T, First>{});
        static_for<T, First + 1, Last>(std::forward<F>(f));
    }
}

} // namespace gba

#endif //GAMEBOIADVANCE_STATIC_FOR_H
