/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cpu/irq_controller_handle.h>

#include <gba/cpu/arm7tdmi.h>

namespace gba::cpu {

void irq_controller_handle::request_interrupt(const interrupt_source irq) noexcept
{
    arm_->request_interrupt(irq);
}

} // namespace gba::cpu
