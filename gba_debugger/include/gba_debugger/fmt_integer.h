#ifndef GAMEBOIADVANCE_FMT_INTEGER_H
#define GAMEBOIADVANCE_FMT_INTEGER_H

#include <fmt/format.h>

#include <gba/core/integer.h>

template<typename T>
struct fmt::formatter<gba::integer<T>> : formatter<T> {
    template<typename FormatContext>
    auto format(gba::integer<T> i, FormatContext& ctx)
    {
        return formatter<T>::format(i.get(), ctx);
    }
};

#endif //GAMEBOIADVANCE_FMT_INTEGER_H
