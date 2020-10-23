#ifndef GAMEBOIADVANCE_INTEGER_H
#define GAMEBOIADVANCE_INTEGER_H

// https://github.com/foonathan/type_safe/blob/master/include/type_safe/integer.hpp

#include <cstdint>
#include <type_traits>

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
using enable_safe_unsigned_operation = std::enable_if_t<std::is_unsigned_v<A> && std::is_unsigned_v<B> && sizeof(A) >= sizeof(B)>;

} // namespace detail

template<typename Integer>
class integer {
    static_assert(detail::is_integer_v<Integer>, "Integer must be an integer type");

    Integer value_{0};

public:
    using type = Integer;

    FORCEINLINE constexpr integer() noexcept = default;

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE constexpr integer(const T value) noexcept : value_{value} {} // NOLINT

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE constexpr integer(const integer<T> value) noexcept  // NOLINT
        : value_(static_cast<T>(value)) {}

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE integer& operator=(const T value) noexcept
    {
        value_ = value;
        return *this;
    }

    template<typename T, typename = detail::enable_safe_integer_conversion<T, Integer>>
    FORCEINLINE integer& operator=(const integer<T> value) noexcept
    {
        value_ = static_cast<Integer>(value);
        return *this;
    }

    FORCEINLINE explicit constexpr operator Integer() const noexcept { return value_; }
    FORCEINLINE constexpr Integer get() const noexcept { return value_; }

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

    FORCEINLINE constexpr integer operator++(int) const noexcept
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

    FORCEINLINE constexpr integer operator--(int) const noexcept
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
        return integer(~value_);
    }

#define MAKE_OP(Op)                                                                                 \
    template<typename T, typename = detail::enable_safe_unsigned_operation<T, Integer>>             \
    FORCEINLINE constexpr integer& operator Op(const integer<T> other) noexcept                     \
    {                                                                                               \
        value_ Op static_cast<T>(other);                                                            \
        return *this;                                                                               \
    }                                                                                               \
    template<typename T, typename = detail::enable_safe_unsigned_operation<T, Integer>>             \
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
    using type = typename std::make_signed<T>::type;
};

template<typename T>
struct make_signed<integer<T>>
{
    using type = integer<std::make_signed_t<T>>;
};

template<typename T>
struct make_unsigned
{
    using type = std::make_unsigned_t<T>;
};

template<typename T>
struct make_unsigned<integer<T>>
{
    using type = integer<std::make_unsigned_t<T>>;
};

} // namespace detail

template<typename To, typename From, typename = std::enable_if_t<
        detail::is_safe_integer_operation_v<detail::underlying_int_type<From>, detail::underlying_int_type<To>>
        && sizeof(To) < sizeof(From)>>
FORCEINLINE constexpr To narrow(const From from) noexcept { return static_cast<typename To::type>(from.get()); }

// sign ops

template<class Integer> using make_signed_t = typename detail::make_signed<Integer>::type;
template<class Integer> using make_unsigned_t = typename detail::make_unsigned<Integer>::type;

template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_signed_t<T> make_signed(const T i) noexcept { return static_cast<make_signed_t<T>>(i); }
template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_signed_t<integer<T>> make_signed(const integer<T> i) noexcept { return make_signed(static_cast<T>(i)); }

template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_unsigned_t<T> make_unsigned(const T i) noexcept { return static_cast<make_unsigned_t<T>>(i); }
template<typename T, typename = detail::enable_integer<T>>
FORCEINLINE constexpr make_unsigned_t<integer<T>> make_unsigned(const integer<T> i) noexcept { return make_unsigned(static_cast<T>(i)); }

// arithmetic ops

template<typename A, typename B>
FORCEINLINE constexpr integer<detail::greater_sized_t<A, B>> operator+(const integer<A> a, const integer<B> b) noexcept
{
    return integer<detail::greater_sized_t<A, B>>(static_cast<A>(a) + static_cast<B>(b));
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
        return integer<detail::integer_result_t<A, B>>(static_cast<A>(a) Op static_cast<B>(b));                                 \
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
        return static_cast<A>(a) Op static_cast<B>(b);                                              \
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

} // namespace gba

// make sure these are in the global namespace
FORCEINLINE constexpr int8_t operator""_i8(unsigned long long v) noexcept { return static_cast<int8_t>(v); }
FORCEINLINE constexpr uint8_t operator""_u8(unsigned long long v) noexcept { return static_cast<uint8_t>(v); }
FORCEINLINE constexpr int16_t operator""_i16(unsigned long long v) noexcept { return static_cast<int16_t>(v); }
FORCEINLINE constexpr uint16_t operator""_u16(unsigned long long v) noexcept { return static_cast<uint16_t>(v); }
FORCEINLINE constexpr int32_t operator""_i32(unsigned long long v) noexcept { return static_cast<int32_t>(v); }
FORCEINLINE constexpr uint32_t operator""_u32(unsigned long long v) noexcept { return static_cast<uint32_t>(v); }
FORCEINLINE constexpr int64_t operator""_i64(unsigned long long v) noexcept { return static_cast<int64_t>(v); }
FORCEINLINE constexpr uint64_t operator""_u64(unsigned long long v) noexcept { return v; }

FORCEINLINE constexpr auto operator""_kb(unsigned long long v) noexcept
{
    return gba::integer<std::size_t>(static_cast<std::size_t>(v) * 1024u);
}

template<typename Integer>
struct std::hash<gba::integer<Integer>> {
    std::size_t operator()(const gba::integer<Integer>& i) const noexcept
    {
        return std::hash<Integer>()(static_cast<Integer>(i));
    }
};

#endif //GAMEBOIADVANCE_INTEGER_H
