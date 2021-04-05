/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_SORT_H
#define GAMEBOIADVANCE_SORT_H

#include <algorithm>

namespace gba {

template<typename Container>
FORCEINLINE void insertion_sort(Container& container) noexcept
{
    for(auto i = container.begin(); i != container.end(); ++i) {
        std::rotate(std::upper_bound(container.begin(), i, *i), i, i + 1);
    }
}

} // namespace gba

#endif //GAMEBOIADVANCE_SORT_H
