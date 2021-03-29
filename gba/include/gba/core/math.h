/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_MATH_H
#define GAMEBOIADVANCE_MATH_H

#include <gba/core/integer.h>
#include <gba/helper/macros.h>

namespace gba {

namespace bit {

template<typename T = u32>
[[nodiscard]] FORCEINLINE constexpr T from_bool(const bool b) noexcept { return static_cast<typename T::type>(b); }

template<typename T = u32>
[[nodiscard]] FORCEINLINE constexpr T bit(const u8 b) noexcept { return narrow<T>(1_u32 << b); }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T extract(const T t, const u8 b) noexcept { return (t >> b) & 0x1_u8; }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr bool test(const T t, const u8 b) noexcept { return extract(t, b) != 0_u32; }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T set(const T t, const u8 b) noexcept { return t | bit<T>(b); }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T clear(const T t, const u8 b) noexcept { return t & ~bit<T>(b); }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T set_byte(const T t, const u8 n, const u8 byte) noexcept
{
    ASSERT(sizeof(T) > n.get());
    constexpr u8 byte_mask = 0xFF_u8;
    return (t & ~(widen<T>(byte_mask) << (8_u8 * n))) | (widen<T>(byte) << (8_u8 * n));
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr u8 extract_byte(const T t, const u8 n) noexcept
{
    ASSERT(sizeof(T) > n.get());
    return narrow<u8>((t >> (n * 8_u8)) & 0xFF_u8);
}

} // namespace bit

namespace mask {

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T set(const T t, const T m) noexcept { return t | m; }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T clear(const T t, const T m) noexcept { return t & ~m; }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T set(const T t, const traits::underlying_int_type<T> m) noexcept
{
    return mask::set(t, integer<traits::underlying_int_type<T>>{m});
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T clear(const T t, const typename T::type m) noexcept
{
    return mask::clear(t, integer<traits::underlying_int_type<T>>{m});
}

} // namespace mask

namespace math {

template<typename T>
struct logical_op_result {
    T result;
    T carry;
};

template<u8::type B, typename T>
[[nodiscard]] FORCEINLINE constexpr integer<traits::make_signed_t<T>> sign_extend(const integer<T> x) noexcept
{
    struct { traits::make_signed_t<T> x: B; } extender; // NOLINT
    extender.x = x.get();
    return extender.x;
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr logical_op_result<T> logical_shift_left(const T t, const u8 shift) noexcept
{
    return logical_op_result<T>{
      t << shift,
      bit::extract(t, narrow<u8>(make_unsigned(numeric_limits<T>::digits)) - shift)
    };
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr logical_op_result<T> logical_shift_right(const T t, const u8 shift) noexcept
{
    return logical_op_result<T>{t >> shift, bit::extract(t, shift - 1_u8)};
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr logical_op_result<T> arithmetic_shift_right(const T t, const u8 shift) noexcept
{
    return logical_op_result<T>{
      make_unsigned(make_signed(t) >> shift),
      bit::extract(t, shift - 1_u8)
    };
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr logical_op_result<T> logical_rotate_right(const T t, const u8 rotate) noexcept
{
    return logical_op_result<T>{
      (t >> rotate) | (t << narrow<T>(make_unsigned(numeric_limits<T>::digits) - rotate)),
      bit::extract(t, rotate - 1_u8)
    };
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr logical_op_result<T> logical_rotate_right_extended(const T t, const T in_carry) noexcept
{
    return logical_op_result<T>{
      (t >> 1_u8) | (in_carry << narrow<T>(make_unsigned(numeric_limits<T>::digits - 1_i32))),
      bit::extract(t, 0_u8)
    };
}

} // namespace math

} // namespace gba

#endif //GAMEBOIADVANCE_MATH_H
