/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_IRQ_CONTROLLER_HANDLE_H
#define GAMEBOIADVANCE_IRQ_CONTROLLER_HANDLE_H

#include <gba/core/integer.h>

namespace gba::arm {

enum class interrupt_source : u16::type {
    vblank = 1 << 0,
    hblank = 1 << 1,
    vcounter_match = 1 << 2,
    timer_0_overflow = 1 << 3,
    timer_1_overflow [[maybe_unused]] = 1 << 4,
    timer_2_overflow [[maybe_unused]] = 1 << 5,
    timer_3_overflow [[maybe_unused]] = 1 << 6,
    serial_io = 1 << 7,
    dma_0 = 1 << 8,
    dma_1 [[maybe_unused]] = 1 << 9,
    dma_2 [[maybe_unused]] = 1 << 10,
    dma_3 [[maybe_unused]] = 1 << 11,
    keypad = 1 << 12,
    gamepak = 1 << 13,
};

class irq_controller_handle {
    u16* if_;

public:
    irq_controller_handle() = default;
    explicit irq_controller_handle(u16* if_handle) noexcept : if_{if_handle} {}
    void request_interrupt(const interrupt_source irq) noexcept { *if_ |= from_enum<u16>(irq); }
};

} // namespace gba::arm

#endif //GAMEBOIADVANCE_IRQ_CONTROLLER_HANDLE_H
