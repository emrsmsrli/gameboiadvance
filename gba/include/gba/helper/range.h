/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_RANGE_H
#define GAMEBOIADVANCE_RANGE_H

#include <gba/core/integer.h>

namespace gba {

template<typename T>
struct range {
    T min_;
    T max_;

public:
    class iterator {
        T current_;

    public:
        FORCEINLINE constexpr explicit iterator(T current) noexcept
            : current_{current} {}

        FORCEINLINE constexpr iterator& operator++() noexcept
        {
            if constexpr(std::is_enum_v<T>) {
                current_ = static_cast<T>(static_cast<std::underlying_type_t<T>>(current_) + 1);
            } else {
                ++current_;
            }
            return *this;
        }

        FORCEINLINE constexpr T operator*() const { return current_; }

        FORCEINLINE constexpr bool operator==(const iterator& other) const noexcept { return other.current_ == current_; }
        FORCEINLINE constexpr bool operator!=(const iterator& other) const noexcept { return !(*this == other); }
    };

    FORCEINLINE constexpr explicit range(const T max) noexcept
        : range(T{}, max) {}

    FORCEINLINE constexpr range(const T min, const T max) noexcept
        : min_(min), max_(max) {  ASSERT(min < max); }

    [[nodiscard]] FORCEINLINE constexpr iterator begin() const noexcept { return iterator(min_); }
    [[nodiscard]] FORCEINLINE constexpr iterator end() const noexcept { return iterator(max_); }

    [[nodiscard]] FORCEINLINE constexpr T size() const noexcept { return max_ - min_; }
    [[nodiscard]] FORCEINLINE constexpr T min() const noexcept { return min_; }
    [[nodiscard]] FORCEINLINE constexpr T max() const noexcept { return max_; }

    [[nodiscard]] FORCEINLINE constexpr bool contains(const T v) { return min_ <= v && v < max_; }
};

template<typename Container, typename F>
FORCEINLINE void enumerate(Container&& container, F&& f) noexcept
{
    usize idx;
    for(auto&& e : std::forward<Container>(container)) {
        if constexpr(!std::is_lvalue_reference<Container>::value) {
            f(idx++, std::move(e));
        } else {
            f(idx++, e);
        }
    }
}

} // namespace gba

#endif //GAMEBOIADVANCE_RANGE_H
