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

using namespace gba;

template<typename T>
struct StringMaker<integer<T>> {
static String convert(const integer<T>& value) {
    return std::to_string(value.get()).c_str();
}
};

}

#endif //GAMEBOIADVANCE_TEST_PRELUDE_H
