/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_GBA_H
#define GAMEBOIADVANCE_GBA_H

#include <gba/cartridge/gamepak.h>
#include <gba/keypad.h>

namespace gba {

struct gba {
    gamepak pak;
    keypad keypad;

    void tick(u8 cycles = 1_u8) noexcept {}
    void tick_one_frame() noexcept {}

    void press_key(const keypad::key key) noexcept { keypad.press(key); }
    void release_key(const keypad::key key) noexcept { keypad.release(key); }

    void load_pak(const fs::path& path) { pak.load(path); }
};

} // namespace gba

#endif //GAMEBOIADVANCE_GBA_H
