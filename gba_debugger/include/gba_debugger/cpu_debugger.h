/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_CPU_DEBUGGER_H
#define GAMEBOIADVANCE_CPU_DEBUGGER_H

#include <gba/core/fwd.h>
#include <gba/core/event/event.h>

namespace gba::debugger {

class breakpoint_database;

class cpu_debugger {
public:
    enum class execution_request { none, instruction, scanline, frame };

private:
    dma::controller* dma_controller_;
    timer::controller* timer_controller_;
    arm::arm7tdmi* arm_;
    breakpoint_database* bp_db_;

public:
    event<execution_request> on_execution_requested;

    cpu_debugger(timer::controller* timer_controller, dma::controller* dma_controller,
      arm::arm7tdmi* arm, breakpoint_database* bp_db) noexcept
      : dma_controller_{dma_controller},
        timer_controller_{timer_controller},
        arm_{arm},
        bp_db_{bp_db} {}

    void draw() noexcept;

private:
    void draw_breakpoints() noexcept;
    void draw_execution_breakpoints() noexcept;
    void draw_access_breakpoints() noexcept;
    void draw_disassembly() noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_CPU_DEBUGGER_H
