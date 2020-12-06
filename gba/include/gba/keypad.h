/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_KEYPAD_H
#define GAMEBOIADVANCE_KEYPAD_H

#include <gba/core/integer.h>

namespace gba {

class keypad {
public:
    enum class key : u16::type {
        a = 1_u16 << 0_u16,
        b = 1_u16 << 1_u16,
        select = 1_u16 << 2_u16,
        start = 1_u16 << 3_u16,
        right = 1_u16 << 4_u16,
        left = 1_u16 << 5_u16,
        up = 1_u16 << 6_u16,
        down = 1_u16 << 7_u16,
        right_shoulder = 1_u16 << 8_u16,
        left_shoulder = 1_u16 << 9_u16,
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
    void press(key k) noexcept { pressed_ &= ~static_cast<u16::type>(k); }
    void release(key k) noexcept { pressed_ |= static_cast<u16::type>(k); }

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
