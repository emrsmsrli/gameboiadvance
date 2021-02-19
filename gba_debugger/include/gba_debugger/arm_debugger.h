/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM_DEBUGGER_H
#define GAMEBOIADVANCE_ARM_DEBUGGER_H

#include <gba/core/fwd.h>

namespace gba::debugger {

struct arm_debugger {
    arm::arm7tdmi* arm;

    void draw() const noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_ARM_DEBUGGER_H
