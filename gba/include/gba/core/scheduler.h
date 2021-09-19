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
#include <string_view>
#include <type_traits>  // std::forward

#include <gba/core/container.h>
#include <gba/core/event/delegate.h>

#define MAKE_HW_EVENT_V(callback, instance) {connect_arg<&callback>, instance}
#define MAKE_HW_EVENT(callback) MAKE_HW_EVENT_V(callback, this)

namespace gba {

class hw_event_registry {
public:
    struct entry {
        delegate<void(u32)> callback;
        std::string_view name;

        entry(const delegate<void(u32)> cb, const std::string_view n) noexcept
          : callback(cb), name(n) {}
    };

private:
    vector<entry> entries_;

public:
    [[nodiscard]] static hw_event_registry& get() noexcept
    {
        static hw_event_registry registry;
        return registry;
    }

    void register_entry(const delegate<void(u32)> callback,
      const std::string_view name) noexcept
    {
        const entry* e = find_by_name(name);
        if(!e) {
            entries_.emplace_back(callback, name);
            LOG_DEBUG(hw_event_registry, "event registered: {}", name);
        }
    }

    [[nodiscard]] const entry* find_by_name(const std::string_view name) const noexcept
    {
        const auto it = std::find_if(entries_.begin(),  entries_.end(), [&](const entry& entry) {
            return entry.name == name;
        });
        return it == entries_.end() ? nullptr : &*it;
    }

    [[nodiscard]] const entry* find_by_callback(const delegate<void(u32)> cb) const noexcept
    {
        const auto it = std::find_if(entries_.begin(),  entries_.end(), [&](const entry& entry) {
            return entry.callback == cb;
        });
        return it == entries_.end() ? nullptr : &*it;
    }
};

class scheduler {
public:
    // represents an hardware event
    struct hw_event {
        using handle = u64;

        delegate<void(u32 /*late_cycles*/)> callback;
        u64 timestamp;
        handle h;

        bool operator>(const hw_event& other) const noexcept { return timestamp > other.timestamp; }

        template<typename Ar>
        void serialize(Ar& archive) const noexcept
        {
            const hw_event_registry& event_registry = hw_event_registry::get();
            const hw_event_registry::entry* event_entry = event_registry.find_by_callback(callback);
            if(!event_entry) {
                LOG_CRITICAL(scheduler, "unencountered event callback");
                PANIC();
            }

            archive.serialize(event_entry->name);
            archive.serialize(timestamp);
            archive.serialize(h);
        }

        template<typename Ar>
        void deserialize(const Ar& archive) noexcept
        {
            const std::string_view event_name = archive.template deserialize<std::string_view>();

            const hw_event_registry& event_registry = hw_event_registry::get();
            const hw_event_registry::entry* event_entry = event_registry.find_by_name(event_name);
            if(!event_entry) {
                LOG_CRITICAL(scheduler, "corrupted serialized event data {}", event_name);
                PANIC();
            }

            callback = event_entry->callback;
            archive.deserialize(timestamp);
            archive.deserialize(h);
        }
    };

private:
    using predicate = std::greater<scheduler::hw_event>;

    vector<hw_event> heap_;
    u64 now_;
    u64 next_event_handle_;

public:
    scheduler()
    {
        heap_.reserve(64_usize);
    }

    hw_event::handle add_hw_event(const u32 delay, const delegate<void(u32)> callback)
    {
        heap_.push_back(hw_event{callback, now_ + delay, ++next_event_handle_});
        std::push_heap(heap_.begin(), heap_.end(), predicate{});
        return next_event_handle_;
    }

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
                const auto [callback, timestamp, handle] = heap_.front();

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

    template<typename Ar>
    void serialize(Ar& archive) const noexcept
    {
        archive.serialize(heap_);
        archive.serialize(now_);
        archive.serialize(next_event_handle_);
    }

    template<typename Ar>
    void deserialize(const Ar& archive) noexcept
    {
        archive.deserialize(heap_);
        archive.deserialize(now_);
        archive.deserialize(next_event_handle_);
    }
};

} // namespace gba

#endif //GAMEBOIADVANCE_SCHEDULER_H
