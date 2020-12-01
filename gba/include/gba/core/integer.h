/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_INTEGER_H
#define GAMEBOIADVANCE_INTEGER_H

// https://github.com/foonathan/type_safe/blob/master/include/type_safe/integer.hpp

#include <cstdint>
#include <type_traits>
#include <limits>

#include <fmt/core.h>

#include <gba/helper/macros.h>

namespace gba {

namespace detail {

template<typename T> inline constexpr bool is_integer_v =
  std::is_integral_v<T> &&
    !std::is_same_v<T, bool> &&
    !std::is_same_v<T, char>;

template<typename T>
using enable_integer = std::enable_if_t<is_integer_v<T>>;

template<typename From, typename To>
inline constexpr bool is_safe_integer_conversion_v =
  is_integer_v<From> && is_integer_v<To> &&
    ((sizeof(From) <= sizeof(To) && std::is_signed_v<From> == std::is_signed_v<To>) ||
      (sizeof(From) < sizeof(To) && std::is_unsigned_v<From>));

template<typename From, typename To>
using enable_safe_integer_conversion = std::enable_if_t<is_safe_integer_conversion_v<From, To>>;

template<typename From, typename To>
inline constexpr bool is_safe_integer_comparison_v =
  is_safe_integer_conversion_v<From, To> || is_safe_integer_conversion_v<To, From>;

template<typename A, typename B>
using enable_safe_integer_comparison = std::enable_if_t<is_safe_integer_comparison_v<A, B>>;

template<typename From, typename To>
inline constexpr bool is_safe_integer_operation_v =
  is_integer_v<From> && is_integer_v<To> &&
    std::is_signed_v<From> == std::is_signed_v<To>;

template<typename A, typename B>
using greater_sized_t = std::conditional_t<sizeof(A) < sizeof(B), B, A>;

template<typename A, typename B>
using integer_result_t = std::enable_if_t<is_safe_integer_operation_v<A, B>, greater_sized_t<A, B>>;

template<typename A, typename B>
using enable_safe_unsigned_operation = std::enable_if_t<
  std::is_unsigned_v<A> && std::is_unsigned_v<B> && sizeof(A) >= sizeof(B)>;

} // namespace detail

template<typename Integer>
class integer {
    static_assert(detail::is_integer_v<Integer>, "Integer must be an integer type");

    Integer value_{0};

public:
    using type = Integer;

    FORCEINLINE constexpr integer() noexcept = default;

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE constexpr integer(const T value) noexcept  // NOLINT
      : value_{value} {}

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE constexpr integer(const integer<T> value) noexcept  // NOLINT
      : value_(static_cast<T>(value)) {}

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE constexpr integer& operator=(const T value) noexcept
    {
        value_ = value;
        return *this;
    }

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE constexpr integer& operator=(const integer<T> value) noexcept
    {
        value_ = static_cast<T>(value);
        return *this;
    }

    FORCEINLINE explicit constexpr operator Integer() const noexcept { return value_; }
    [[nodiscard]] FORCEINLINE constexpr Integer get() const noexcept { return value_; }

    // arithmetic ops

    FORCEINLINE constexpr integer operator+() const noexcept { return *this; }
    FORCEINLINE constexpr integer operator-() const noexcept
    {
        static_assert(std::is_signed_v<Integer>, "T must be signed");
        return integer{value_ * Integer{-1}};
    }

    FORCEINLINE constexpr integer& operator++() noexcept
    {
        value_ = value_ + Integer{1};
        return *this;
    }

    FORCEINLINE constexpr integer operator++(int) noexcept  // NOLINT
    {
        auto res = *this;
        ++*this;
        return res;
    }

    FORCEINLINE constexpr integer& operator--() noexcept
    {
        value_ = value_ - Integer{1};
        return *this;
    }

    FORCEINLINE constexpr integer operator--(int) noexcept  // NOLINT
    {
        auto res = *this;
        --*this;
        return res;
    }

    template<typename T, typename = detail::enable_integer<T>>
    FORCEINLINE constexpr integer& operator+=(const integer<T> other) noexcept
    {
        value_ += static_cast<T>(other);
        return *this;
    }

    template<typename T, typename = detail::enable_integer<T>>
    FORCEINLINE constexpr integer& operator+=(const T other) noexcept
    {
        value_ += other;
        return *this;
    }

#define MAKE_OP(Op)                                                                                 \
    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>             \
    FORCEINLINE constexpr integer& operator Op(const integer<T> other) noexcept                     \
    {                                                                                               \
        value_ Op static_cast<T>(other);                                                            \
        return *this;                                                                               \
    }                                                                                               \
    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>             \
    FORCEINLINE constexpr integer& operator Op(const T other) noexcept                              \
    {                                                                                               \
        *this Op integer<T>(other);                                                                 \
        return *this;                                                                               \
    }

    MAKE_OP(-=)
    MAKE_OP(*=)
    MAKE_OP(/=)
    MAKE_OP(%=)

#undef MAKE_OP

    // bitwise ops

    FORCEINLINE constexpr integer operator~() const noexcept
    {
        static_assert(std::is_unsigned_v<Integer>, "Integer must be unsigned");
        return integer(static_cast<Integer>(~value_));
    }

#define MAKE_OP(Op)                                                                                 \
    template<typename T, typename = detail::enable_safe_unsigned_operation<Integer, T>>             \
    FORCEINLINE constexpr integer& operator Op(const integer<T> other) noexcept                     \
    {                                                                                               \
        value_ Op static_cast<T>(other);                                                            \
        return *this;                                                                               \
    }                                                                                               \
    template<typename T, typename = detail::enable_safe_unsigned_operation<Integer, T>>             \
    FORCEINLINE constexpr integer& operator Op(const T other) noexcept                              \
    {                                                                                               \
        *this Op integer<T>(other);                                                                 \
        return *this;                                                                               \
    }

    MAKE_OP(|=)
    MAKE_OP(&=)
    MAKE_OP(^=)
    MAKE_OP(<<=)
    MAKE_OP(>>=)

#undef MAKE_OP
};

namespace detail {

template<typename Integer>
using underlying_int_type = typename Integer::type;

template<typename T>
struct make_signed {
    using type = std::make_signed_t<T>;
};

template<typename T>
struct make_signed<integer<T>> {
    using type = integer<std::make_signed_t<T>>;
};

template<typename T>
struct make_unsigned {
    using type = std::make_unsigned_t<T>;
};

template<typename T>
struct make_unsigned<integer<T>> {
    using type = integer<std::make_unsigned_t<T>>;
};

} // namespace detail

template<typename To, typename From, typename = std::enable_if_t<
  detail::is_safe_integer_operation_v<detail::underlying_int_type<From>, detail::underlying_int_type<To>>>>
FORCEINLINE constexpr To narrow(const From from) noexcept
{
    if constexpr(sizeof(To) < sizeof(From)) {
        return static_cast<typename To::type>(from.get());
    } else {
        static_assert(sizeof(To) == sizeof(From), "narrow() shouldn't widen integers");
        return from;
    }
}

// sign ops

template<class Integer> using make_signed_t = typename detail::make_signed<Integer>::type;
template<class Integer> using make_unsigned_t = typename detail::make_unsigned<Integer>::type;

template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_signed_t<integer<T>> make_signed(const T i) noexcept { return static_cast<make_signed_t<T>>(i); }
template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_signed_t<integer<T>> make_signed(const integer<T> i) noexcept
{
    return make_signed(static_cast<T>(i));
}

template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_unsigned_t<integer<T>> make_unsigned(const T i) noexcept { return static_cast<make_unsigned_t<T>>(i); }
template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_unsigned_t<integer<T>> make_unsigned(const integer<T> i) noexcept
{
    return make_unsigned(static_cast<T>(i));
}

// arithmetic ops

template<typename A, typename B>
FORCEINLINE constexpr integer<detail::greater_sized_t<A, B>> operator+(const integer<A> a, const integer<B> b) noexcept
{
    using result_type = detail::greater_sized_t<A, B>;
    return static_cast<result_type>(static_cast<A>(a) + static_cast<B>(b));
}

template<typename A, typename B>
FORCEINLINE constexpr integer<detail::greater_sized_t<A, B>> operator+(const A a, const integer<B> b) noexcept
{
    return integer<A>(a) + b;
}

template<typename A, typename B>
FORCEINLINE constexpr integer<detail::greater_sized_t<A, B>> operator+(const integer<A> a, const B b) noexcept
{
    return a + integer<B>(b);
}

#define MAKE_OP(Op)                                                                                                             \
    template<typename A, typename B>                                                                                            \
    FORCEINLINE constexpr integer<detail::integer_result_t<A, B>> operator Op(const integer<A> a, const integer<B> b) noexcept  \
    {                                                                                                                           \
        using result_type = detail::integer_result_t<A, B>;                                                                     \
        return integer<result_type>(static_cast<result_type>(static_cast<A>(a) Op static_cast<B>(b)));                          \
    }                                                                                                                           \
    template<typename A, typename B>                                                                                            \
    FORCEINLINE constexpr integer<detail::integer_result_t<A, B>> operator Op(const A a, const integer<B> b) noexcept           \
    {                                                                                                                           \
        return integer<A>(a) Op b;                                                                                              \
    }                                                                                                                           \
    template<typename A, typename B>                                                                                            \
    FORCEINLINE constexpr integer<detail::integer_result_t<A, B>> operator Op(const integer<A> a, const B b) noexcept           \
    {                                                                                                                           \
        return a Op integer<B>(b);                                                                                              \
    }

MAKE_OP(-)
MAKE_OP(*)
MAKE_OP(/)
MAKE_OP(%)

#undef MAKE_OP

// comparison ops

#define MAKE_OP(Op)                                                                                 \
    template<typename A, typename B, typename = detail::enable_safe_integer_comparison<A, B>>       \
    FORCEINLINE constexpr bool operator Op(const integer<A> a, const integer<B> b) noexcept         \
    {                                                                                               \
        return static_cast<A>(a) Op static_cast<B>(b);                                              \
    }                                                                                               \
    template<typename A, typename B, typename = detail::enable_safe_integer_comparison<A, B>>       \
    FORCEINLINE constexpr bool operator Op(const A a, const integer<B> b) noexcept                  \
    {                                                                                               \
        return integer<A>(a) Op b;                                                                  \
    }                                                                                               \
    template<typename A, typename B, typename = detail::enable_safe_integer_comparison<A, B>>       \
    FORCEINLINE constexpr bool operator Op(const integer<A> a, const B b) noexcept                  \
    {                                                                                               \
        return a Op integer<B>(b);                                                                  \
    }

MAKE_OP(==)
MAKE_OP(!=)
MAKE_OP(<)
MAKE_OP(<=)
MAKE_OP(>)
MAKE_OP(>=)

#undef MAKE_OP

// bitwise ops

#define MAKE_OP(Op)                                                                                 \
    template<typename A, typename B, typename = detail::enable_safe_unsigned_operation<A, B>>       \
    FORCEINLINE constexpr integer<A> operator Op(const integer<A> a, const integer<B> b) noexcept   \
    {                                                                                               \
        return static_cast<A>(static_cast<A>(a) Op static_cast<B>(b));                              \
    }                                                                                               \
    template<typename A, typename B, typename = detail::enable_safe_unsigned_operation<A, B>>       \
    FORCEINLINE constexpr integer<A> operator Op(const A a, const integer<B> b) noexcept            \
    {                                                                                               \
        return integer<A>(a) Op b;                                                                  \
    }                                                                                               \
    template<typename A, typename B, typename = detail::enable_safe_unsigned_operation<A, B>>       \
    FORCEINLINE constexpr integer<A> operator Op(const integer<A> a, const B b) noexcept            \
    {                                                                                               \
        return a Op integer<B>(b);                                                                  \
    }

MAKE_OP(&)
MAKE_OP(|)
MAKE_OP(^)
MAKE_OP(<<)
MAKE_OP(>>)

#undef MAKE_OP

using i8 = integer<int8_t>;
using u8 = integer<uint8_t>;

using i16 = integer<int16_t>;
using u16 = integer<uint16_t>;

using i32 = integer<int32_t>;
using u32 = integer<uint32_t>;

using i64 = integer<int64_t>;
using u64 = integer<uint64_t>;

using usize = integer<std::size_t>;

template<typename T>
using numeric_limits = std::numeric_limits<typename T::type>;

inline namespace integer_literals {

FORCEINLINE constexpr i8::type operator ""_i8(unsigned long long v) noexcept { return static_cast<i8::type>(v); }
FORCEINLINE constexpr u8::type operator ""_u8(unsigned long long v) noexcept { return static_cast<u8::type>(v); }
FORCEINLINE constexpr i16::type operator ""_i16(unsigned long long v) noexcept { return static_cast<i16::type>(v); }
FORCEINLINE constexpr u16::type operator ""_u16(unsigned long long v) noexcept { return static_cast<u16::type>(v); }
FORCEINLINE constexpr i32::type operator ""_i32(unsigned long long v) noexcept { return static_cast<i32::type>(v); }
FORCEINLINE constexpr u32::type operator ""_u32(unsigned long long v) noexcept { return static_cast<u32::type>(v); }
FORCEINLINE constexpr i64::type operator ""_i64(unsigned long long v) noexcept { return static_cast<i64::type>(v); }
FORCEINLINE constexpr u64::type operator ""_u64(unsigned long long v) noexcept { return v; }
FORCEINLINE constexpr usize::type operator ""_usize(
  unsigned long long v) noexcept { return static_cast<usize::type>(v); }

FORCEINLINE constexpr usize operator ""_kb(unsigned long long v) noexcept
{
    return static_cast<usize::type>(v) * 1024_u32;
}

} // inline namespace integer_literals

} // namespace gba

template<typename Integer>
struct std::hash<gba::integer<Integer>> {
    std::size_t operator()(const gba::integer<Integer>& i) const noexcept
    {
        return std::hash<Integer>()(static_cast<Integer>(i));
    }
};

template<typename T>
struct fmt::formatter<gba::integer<T>> : formatter<T> {
    template<typename FormatContext>
    auto format(gba::integer<T> i, FormatContext& ctx)
    {
        return formatter<T>::format(i.get(), ctx);
    }
};

#endif //GAMEBOIADVANCE_INTEGER_H
