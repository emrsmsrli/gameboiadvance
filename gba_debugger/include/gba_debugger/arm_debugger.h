/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM_DEBUGGER_H
#define GAMEBOIADVANCE_ARM_DEBUGGER_H

#include <gba/core/fwd.h>
#include <gba/core/event/event.h>

namespace gba::debugger {

class breakpoint_database;

class arm_debugger {
public:
    enum class execution_request { none, instruction, scanline, frame };

private:
    arm::arm7tdmi* arm_;
    breakpoint_database* bp_db_;

public:
    event<execution_request> on_execution_requested;

    arm_debugger(arm::arm7tdmi* arm, breakpoint_database* bp_db) noexcept
      : arm_{arm}, bp_db_{bp_db} {}

    void draw() noexcept;

private:
    void draw_breakpoints() noexcept;
    void draw_execution_breakpoints() noexcept;
    void draw_access_breakpoints() noexcept;
    void draw_disassembly() noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_ARM_DEBUGGER_H
