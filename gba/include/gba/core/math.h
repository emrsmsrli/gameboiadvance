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

} // namespace bit

namespace math {

template<typename T>
struct logical_op_result {
    T result;
    T carry;
};

template<u32::type B, typename T>
[[nodiscard]] FORCEINLINE constexpr make_signed_t<T> sign_extend(const T x) noexcept
{
    struct { typename make_signed_t<T>::type x: B; } extender; // NOLINT
    extender.x = x.get();
    return extender.x;
}

template<typename T>
[[nodiscard]] constexpr logical_op_result<T> logical_shift_left(const T t, const u8 shift) noexcept
{
    return logical_op_result<T>{
      t << shift,
      bit::extract(t, narrow<u8>(make_unsigned(numeric_limits<T>::digits)) - shift)
    };
}

template<typename T>
[[nodiscard]] constexpr logical_op_result<T> logical_shift_right(const T t, const u8 shift) noexcept
{
    return logical_op_result<T>{t >> shift, bit::extract(t, shift - 1_u8)};
}

template<typename T>
[[nodiscard]] constexpr logical_op_result<T> arithmetic_shift_right(const T t, const u8 shift) noexcept
{
    using underlying_type = typename T::type;
    using underlying_stype = make_signed_t<underlying_type>;

    // not portable, GCC produces correct code though
    return logical_op_result<T>{
      make_unsigned(static_cast<underlying_stype>(make_signed(t)) >> static_cast<underlying_type>(shift)),
      bit::extract(t, shift - 1_u8)
    };
}

template<typename T>
[[nodiscard]] constexpr logical_op_result<T> logical_rotate_right(const T t, const u8 rotate) noexcept
{
    return logical_op_result<T>{
      (t >> rotate) | (t << (make_unsigned(numeric_limits<T>::digits) - rotate)),
      bit::extract(t, rotate - 1_u8)
    };
}

template<typename T>
[[nodiscard]] constexpr logical_op_result<T> logical_rotate_right_extended(const T t, const T in_carry) noexcept
{
    return logical_op_result<T>{
      (t >> 1_u32) | (in_carry << make_unsigned(numeric_limits<T>::digits - 1_i32)),
      bit::extract(t, 0_u8)
    };
}

} // namespace math

} // namespace gba

#endif //GAMEBOIADVANCE_MATH_H
