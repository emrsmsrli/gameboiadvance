/*
/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_KEYPAD_DEBUGGER_H
#define GAMEBOIADVANCE_KEYPAD_DEBUGGER_H

#include <gba/core/fwd.h>

namespace gba::debugger {

struct keypad_debugger {
    keypad::keypad* kp;

    void draw() const noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_KEYPAD_DEBUGGER_H
