/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_GAMEPAK_DEBUGGER_H
#define GAMEBOIADVANCE_GAMEPAK_DEBUGGER_H

namespace gba {

namespace cartridge {
class gamepak;
} // namespace cartridge

namespace debugger {

struct gamepak_debugger {
    cartridge::gamepak* pak = nullptr;

    void draw() const noexcept;
};

} // namespace debugger

} // namespace gba

#endif //GAMEBOIADVANCE_GAMEPAK_DEBUGGER_H
