#ifndef GAMEBOIADVANCE_MATH_H
#define GAMEBOIADVANCE_MATH_H

namespace gba::math {

template<typename T>
T min(T f, T s) noexcept
{
    return f < s ? f : s;
}

template<typename T>
T max(T f, T s) noexcept
{
    return f > s ? f : s;
}

} // namespace gba::math

#endif //GAMEBOIADVANCE_MATH_H
