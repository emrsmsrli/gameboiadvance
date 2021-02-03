/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_GBA_H
#define GAMEBOIADVANCE_GBA_H

#include <gba/core/scheduler.h>
#include <gba/arm/arm7tdmi.h>
#include <gba/cartridge/gamepak.h>
#include <gba/ppu/ppu.h>
#include <gba/keypad.h>

namespace gba {

struct gba {
    scheduler schdlr;
    arm::arm7tdmi arm{this};
    cartridge::gamepak pak;
    ppu::engine ppu;
    keypad::keypad keypad;

    void tick(u8 cycles = 1_u8) noexcept {}
    void tick_one_frame() noexcept {}

    void release_key(const keypad::keypad::key key) noexcept { keypad.release(key); }
    void press_key(const keypad::keypad::key key) noexcept {
        keypad.press(key);
        if(keypad.interrupt_available()) {
            arm.request_interrupt(arm::interrupt_source::keypad);
        }
    }

    void load_pak(const fs::path& path) { pak.load(path); }
};

} // namespace gba

#endif //GAMEBOIADVANCE_GBA_H
