/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_BITFLAGS_H
#define GAMEBOIADVANCE_BITFLAGS_H

#include <type_traits>

namespace gba {

template<typename T>
struct enable_bitflag_ops { static constexpr bool value = false; };

#define ENABLE_BITFLAG_OPS(T) \
  template<> struct enable_bitflag_ops<T> { static constexpr bool value = true; }

template<typename T>
std::enable_if_t<enable_bitflag_ops<T>::value, T> operator|(const T l, const T r) noexcept
{
    using underlying_type = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<underlying_type>(l) | static_cast<underlying_type>(r));
}

template<typename T>
std::enable_if_t<enable_bitflag_ops<T>::value, T> operator&(const T l, const T r) noexcept
{
    using underlying_type = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<underlying_type>(l) & static_cast<underlying_type>(r));
}

template<typename T>
std::enable_if_t<enable_bitflag_ops<T>::value, T> operator^(const T l, const T r) noexcept
{
    using underlying_type = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<underlying_type>(l) ^ static_cast<underlying_type>(r));
}

template<typename T>
std::enable_if_t<enable_bitflag_ops<T>::value, T> operator~(const T t) noexcept
{
    return static_cast<T>(~static_cast<std::underlying_type_t<T>>(t));
}

template<typename T>
std::enable_if_t<enable_bitflag_ops<T>::value, T&> operator|=(T& l, const T r) noexcept
{
    l = l | r;
    return l;
}

template<typename T>
std::enable_if_t<enable_bitflag_ops<T>::value, T&> operator&=(T& l, const T r) noexcept
{
    l = l & r;
    return l;
}

template<typename T>
std::enable_if_t<enable_bitflag_ops<T>::value, T&> operator^=(T& l, const T r) noexcept
{
    l = l ^ r;
    return l;
}

} // namespace gba

#endif //GAMEBOIADVANCE_BITFLAGS_H
