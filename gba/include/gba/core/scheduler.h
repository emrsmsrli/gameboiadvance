/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_SCHEDULER_H
#define GAMEBOIADVANCE_SCHEDULER_H

#include <algorithm>
#include <functional>   // std::greater
#include <type_traits>  // std::forward

#if WITH_DEBUGGER
  #include <string>
#endif // WITH_DEBUGGER

#include <gba/core/container.h>
#include <gba/core/event/delegate.h>

#if WITH_DEBUGGER
  #define ADD_HW_EVENT_NAMED(delay, callback, name) add_hw_event(delay, {connect_arg<&callback>, this}, name)
#else
  #define ADD_HW_EVENT_NAMED(delay, callback, name) add_hw_event(delay, {connect_arg<&callback>, this})
#endif // WITH_DEBUGGER

#define ADD_HW_EVENT(delay, callback) ADD_HW_EVENT_NAMED(delay, callback, #callback)

namespace gba {

class scheduler {
public:
    // represents an hardware event
    struct hw_event {
        using handle = u64;

        delegate<void(u32 /*late_cycles*/)> callback;
        u64 timestamp;
        handle h;

#if WITH_DEBUGGER
        std::string name;
#endif // WITH_DEBUGGER

        bool operator>(const hw_event& other) const noexcept { return timestamp > other.timestamp; }
    };

private:
    using predicate = std::greater<scheduler::hw_event>;

    vector<hw_event> heap_;
    u64 now_;
    u64 next_event_handle;

public:
    scheduler()
    {
        heap_.reserve(64_usize);
    }

#if WITH_DEBUGGER
    hw_event::handle add_hw_event(const u32 delay, const delegate<void(u32)> callback, std::string name)
    {
        heap_.push_back(hw_event{callback, now_ + delay, ++next_event_handle, std::move(name)});
        std::push_heap(heap_.begin(), heap_.end(), predicate{});
        return next_event_handle;
    }
#else
    hw_event::handle add_hw_event(const u32 delay, const delegate<void(u32)> callback)
    {
        heap_.push_back(hw_event{callback, now_ + delay, ++next_event_handle});
        std::push_heap(heap_.begin(), heap_.end(), predicate{});
        return next_event_handle;
    }
#endif // WITH_DEBUGGER

    [[nodiscard]] bool has_event(const hw_event::handle handle)
    {
        const auto it = std::find_if(heap_.begin(), heap_.end(), [handle](const hw_event& e) {
            return e.h == handle;
        });

        return it != heap_.end();
    }

    void remove_event(const hw_event::handle handle)
    {
        const auto it = std::remove_if(heap_.begin(), heap_.end(), [handle](const hw_event& e) {
            return e.h == handle;
        });

        if(it != heap_.end()) {
            heap_.erase(it, heap_.end());
            std::make_heap(heap_.begin(), heap_.end(), predicate{});
        }
    }

    void add_cycles(const u32 cycles) noexcept
    {
        now_ += cycles;
        if(const u64 next_event = timestamp_of_next_event(); UNLIKELY(next_event <= now_)) {
            while(!heap_.empty() && timestamp_of_next_event() <= now_) {
#if WITH_DEBUGGER
                const auto [callback, timestamp, handle, name] = heap_[0_usize];
                LOG_TRACE(scheduler, "executing event {}:{}", handle, name);
#else
                const auto [callback, timestamp, handle] = heap_[0_usize];
#endif // WITH_DEBUGGER

                std::pop_heap(heap_.begin(), heap_.end(), predicate{});
                heap_.pop_back();

                // call with how much cycles the event has been late
                callback(narrow<u32>(now_ - timestamp));
            }
        }
    }

    [[nodiscard]] u64 now() const noexcept { return now_; }
    [[nodiscard]] u64 timestamp_of_next_event() const noexcept { ASSERT(!heap_.empty()); return heap_.front().timestamp; }
    [[nodiscard]] u32 remaining_cycles_to_next_event() const noexcept { return narrow<u32>(timestamp_of_next_event() - now()); }
};

} // namespace gba

#endif //GAMEBOIADVANCE_SCHEDULER_H
