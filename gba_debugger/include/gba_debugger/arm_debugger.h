/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM_DEBUGGER_H
#define GAMEBOIADVANCE_ARM_DEBUGGER_H

namespace gba {

namespace arm {
class arm7tdmi;
} // namespace arm

namespace debugger {

struct arm_debugger {
    arm::arm7tdmi* arm;

    void draw() const noexcept;
};

} // namespace debugger

} // namespace gba

#endif //GAMEBOIADVANCE_ARM_DEBUGGER_H
