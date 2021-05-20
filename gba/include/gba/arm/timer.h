/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_TIMER_H
#define GAMEBOIADVANCE_TIMER_H

#include <gba/core/fwd.h>
#include <gba/core/math.h>
#include <gba/core/event/event.h>
#include <gba/core/scheduler.h>

namespace gba::arm {

struct timer_control {
    u8 prescalar;
    bool cascaded = false;
    bool irq_enabled = false;
    bool enabled = false;
};

class timer {
    arm7tdmi* arm_;
    scheduler* scheduler_;

    scheduler::event::handle handle_;

    u32 id_;
    u64 last_scheduled_timestamp_;

    u32 counter_;
    u16 reload_;
    timer_control control_;

public:
    enum class register_type {
        cnt_l_lsb, // TMxCNT_L
        cnt_l_msb, // TMxCNT_L
        cnt_h_lsb, // TMxCNT_H
    };

    event<timer*> on_overflow;

    timer(const u32 id, arm7tdmi* arm, scheduler* scheduler) noexcept
      : arm_{arm},
        id_{id},
        scheduler_{scheduler} {}

    [[nodiscard]] u8 read(register_type reg) const noexcept;
    void write(register_type reg, u8 data) noexcept;

private:
    [[nodiscard]] u32 calculate_counter_delta() const noexcept;

    void schedule_overflow(u64 late_cycles = 0_u64) noexcept;
    void overflow(u64 late_cycles) noexcept;
    void overflow_internal() noexcept;
    void tick_internal() noexcept;
};

} // namespace gba::arm

#endif //GAMEBOIADVANCE_TIMER_H
