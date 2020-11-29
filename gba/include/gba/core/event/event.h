/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_EVENT_H
#define GAMEBOIADVANCE_EVENT_H

#include <vector>

#include <gba/core/event/delegate.h>

namespace gba {

template<typename... Args>
class event {
    std::vector<delegate<void(Args...)>> delegates;

public:
    event() = default;

    void add_delegate(delegate<void(Args...)> d) { delegates.push_back(d); }
    void clear_delegates() noexcept { delegates.clear(); }

    template<typename... TArgs>
    void operator()(TArgs&&... args) const
    {
        for(auto& d : delegates) {
            d(std::forward<TArgs>(args)...);
        }
    }
};

} // namespace gba

#endif //GAMEBOIADVANCE_EVENT_H
