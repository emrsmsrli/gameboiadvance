/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_TEST_PRELUDE_H
#define GAMEBOIADVANCE_TEST_PRELUDE_H

#include <doctest.h>

#include <gba/core/integer.h>

namespace doctest {

template<typename T>
struct StringMaker<gba::integer<T>> {
static String convert(const gba::integer<T>& value) {
    return std::to_string(value.get()).c_str();
}
};

}

#endif //GAMEBOIADVANCE_TEST_PRELUDE_H
