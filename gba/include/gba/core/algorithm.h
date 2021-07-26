/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */


#ifndef GAMEBOIADVANCE_ALGORITHM_H
#define GAMEBOIADVANCE_ALGORITHM_H

#include <algorithm>

namespace gba::algo {

template<typename Container>
FORCEINLINE void insertion_sort(Container& container) noexcept
{
    for(auto i = container.begin(); i != container.end(); ++i) {
        std::rotate(std::upper_bound(container.begin(), i, *i), i, i + 1);
    }
}

} // namespace gba::algo

#endif //GAMEBOIADVANCE_ALGORITHM_H
