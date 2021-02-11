/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_GAMEPAK_DEBUGGER_H
#define GAMEBOIADVANCE_GAMEPAK_DEBUGGER_H

#include <gba/core/fwd.h>

namespace gba::debugger {

struct gamepak_debugger {
    cartridge::gamepak* pak = nullptr;

    void draw() const noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_GAMEPAK_DEBUGGER_H
