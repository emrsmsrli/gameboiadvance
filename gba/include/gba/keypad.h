/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_KEYPAD_H
#define GAMEBOIADVANCE_KEYPAD_H

#include <gba/core/math.h>

namespace gba {

class keypad {
public:
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
    };

    static inline constexpr auto addr_state = 0x0400'0130_u32;
    static inline constexpr auto addr_control = 0x0400'0132_u32;

private:
    u16 pressed_ = 0x003F_u16;
    struct {
        u16 irq_enabled_buttons;
        bool irq_enabled = false;
        bool logical_and_mode = false;
    } control_;

public:
    void press(key k) noexcept { pressed_ = bit::clear(pressed_, static_cast<u8::type>(k)); }
    void release(key k) noexcept { pressed_ = bit::set(pressed_, static_cast<u8::type>(k)); }

    void write(const u32 address, const u8 data) noexcept
    {
        // todo
    }

    [[nodiscard]] u8 read(const u32 address) const noexcept
    {
        // todo
    }
};

} // namespace gba

#endif //GAMEBOIADVANCE_KEYPAD_H
