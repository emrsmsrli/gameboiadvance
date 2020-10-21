#ifndef GAMEBOIADVANCE_MATH_H
#define GAMEBOIADVANCE_MATH_H

#include <gba/core/integer.h>
#include <gba/helper/macros.h>

namespace gba {

namespace math {

template<u32::type B, typename T>
FORCEINLINE constexpr make_signed_t<T> sign_extend(const T x) noexcept
{
    struct { typename make_signed_t<T>::type x : B; } extender; // NOLINT
    extender.x = x.get();
    return extender.x;
}

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T min(T f, T s) noexcept { return f < s ? f : s; }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr T max(T f, T s) noexcept { return f > s ? f : s; }

} // namespace math

namespace bit {

[[nodiscard]] FORCEINLINE constexpr u32 from_bool(const bool b) noexcept { return static_cast<uint32_t>(b); }

template<typename T>
[[nodiscard]] FORCEINLINE constexpr u32 bit(const T b) noexcept { return 1_u32 << b; }

template<typename T, typename TBit>
[[nodiscard]] FORCEINLINE constexpr bool test(const T t, const TBit b) noexcept { return (t & bit(b)) != 0u; }

template<typename T, typename TBit>
[[nodiscard]] FORCEINLINE constexpr bool set(const T t, const TBit b) noexcept { return t | bit(b); }

template<typename T, typename TBit>
[[nodiscard]] FORCEINLINE constexpr bool clear(const T t, const TBit b) noexcept { return t & ~bit(b); }

template<typename T, typename TBit>
[[nodiscard]] FORCEINLINE constexpr u32 extract(const T value, const TBit bit) noexcept
{
    return bit::from_bool(bit::test(value, bit));
}

} // namespace bit

} // namespace gba

#endif //GAMEBOIADVANCE_MATH_H
