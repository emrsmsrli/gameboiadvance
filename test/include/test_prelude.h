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
