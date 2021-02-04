/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_SCHEDULER_H
#define GAMEBOIADVANCE_SCHEDULER_H

#include <algorithm>
#include <functional> // std::greater
#include <type_traits> // std::forward

#include <gba/core/container.h>
#include <gba/core/event/delegate.h>

namespace gba {

class scheduler {
public:
    struct event {
        delegate<void(u64 /*late_cycles*/)> callback;
        u64 timestamp;

        bool operator>(const event& other) const noexcept { return timestamp > other.timestamp; }
        bool operator==(const event& other) const noexcept { return timestamp == other.timestamp && callback == other.callback; }
    };

private:
    using predicate = std::greater<scheduler::event>;

    vector<event> heap_;
    u64 now_;

public:
    scheduler()
    {
        heap_.reserve(64_usize);
    }

    void add_event(const u64 delay, const delegate<void(u64)> callback)
    {
        heap_.push_back(event{callback, now_ + delay});
        std::push_heap(heap_.begin(), heap_.end(), predicate{});
    }

    void remove_event(const event& event)
    {
        if(const auto it = std::find(heap_.begin(), heap_.end(), event); it != heap_.end()) {
            heap_.erase(it);
            std::make_heap(heap_.begin(), heap_.end(), predicate{});
        }
    }

    void add_cycles(const u32 cycles) noexcept
    {
        now_ += cycles;
        if(const u64 next_event = timestamp_of_next_event(); UNLIKELY(next_event <= now_)) {
            while(!heap_.empty() && next_event <= now_) {
                const auto [callback, timestamp] = heap_[0_usize];
                std::pop_heap(heap_.begin(), heap_.end(), predicate{});
                heap_.pop_back();

                // call with how much cycles the event has been later
                callback(now_ - timestamp);
            }
        }
    }

    [[nodiscard]] u64 now() const noexcept { return now_; }
    [[nodiscard]] u64 timestamp_of_next_event() const noexcept { ASSERT(!heap_.empty()); return heap_[0_usize].timestamp; }
    [[nodiscard]] u64 remaining_cycles_to_next_event() const noexcept { return timestamp_of_next_event() - now(); }
};

} // namespace gba

#endif //GAMEBOIADVANCE_SCHEDULER_H
