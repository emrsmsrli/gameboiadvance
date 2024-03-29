/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_KEYPAD_H
#define GAMEBOIADVANCE_KEYPAD_H

#include <gba/core/math.h>

namespace gba::keypad {

enum class key : u8::type {
    a = 0_u8,
    b = 1_u8,
    select = 2_u8,
    start = 3_u8,
    right = 4_u8,
    left = 5_u8,
    up = 6_u8,
    down = 7_u8,
    right_shoulder = 8_u8,
    left_shoulder = 9_u8,

#if WITH_DEBUGGER
    max = 10_u8
#endif // WITH_DEBUGGER
};

struct irq_control {
    enum class condition_strategy : u8::type { any, all };

    u16 select;
    bool enabled = false;
    condition_strategy cond_strategy = condition_strategy::any;
};

struct keypad {
    u16 keyinput_ = 0x03FF_u16;
    irq_control keycnt_;

    void release(const key k) noexcept { keyinput_ = bit::set(keyinput_, from_enum<u8>(k)); }
    void press(const key k) noexcept { keyinput_ = bit::clear(keyinput_, from_enum<u8>(k)); }

    [[nodiscard]] bool interrupt_available() const noexcept
    {
        if(!keycnt_.enabled) {
            return false;
        }

        const u16 input_mask = ~keyinput_ & 0x03FF_u16;
        switch(keycnt_.cond_strategy) {
            case irq_control::condition_strategy::any: return (keycnt_.select & input_mask) != 0_u16;
            case irq_control::condition_strategy::all: return keycnt_.select == input_mask;
            default:
                UNREACHABLE();
        }
    }

    template<typename Ar>
    void serialize(Ar& archive) const noexcept
    {
        archive.serialize(keyinput_);
        archive.serialize(keycnt_.select);
        archive.serialize(keycnt_.enabled);
        archive.serialize(keycnt_.cond_strategy);
    }

    template<typename Ar>
    void deserialize(const Ar& archive) noexcept
    {
        archive.deserialize(keyinput_);
        archive.deserialize(keycnt_.select);
        archive.deserialize(keycnt_.enabled);
        archive.deserialize(keycnt_.cond_strategy);
    }
};

} // namespace gba::keypad

#endif //GAMEBOIADVANCE_KEYPAD_H
