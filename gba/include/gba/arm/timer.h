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
#include <gba/arm/irq_controller_handle.h>
#include <gba/helper/range.h>

namespace gba::timer {

enum class register_type {
    cnt_l_lsb, // TMxCNT_L
    cnt_l_msb, // TMxCNT_L
    cnt_h_lsb, // TMxCNT_H
};

struct timer_cnt {
    u8 prescalar;
    bool cascaded = false;
    bool irq_enabled = false;
    bool enabled = false;
};

class timer {
    friend controller;

    scheduler* scheduler_;
    arm::irq_controller_handle irq_handle_;
    timer* cascade_instance = nullptr;

    scheduler::hw_event::handle handle_;

    u32 id_;
    u64 last_scheduled_timestamp_;

    u32 counter_;
    u16 reload_;
    timer_cnt control_;

public:
    event<timer*> on_overflow;

    timer(const u32 id, scheduler* scheduler) noexcept
      : scheduler_{scheduler},
        id_{id} {}

    [[nodiscard]] u8 read(register_type reg) const noexcept;
    void write(register_type reg, u8 data) noexcept;

    [[nodiscard]] FORCEINLINE u32 id() const noexcept { return id_; }

private:
    [[nodiscard]] u32 calculate_counter_delta() const noexcept;

    void schedule_overflow(u64 late_cycles = 0_u64) noexcept;
    void overflow(u64 late_cycles) noexcept;
    void overflow_internal() noexcept;
    void tick_internal() noexcept;
};

class controller {
    array<timer, 4> timers_;

public:
    controller(scheduler* scheduler) noexcept
      : timers_{
          timer{0_u32, scheduler},
          timer{1_u32, scheduler},
          timer{2_u32, scheduler},
          timer{3_u32, scheduler}
        }
    {
        for(u32 id : range(1_u32, 4_u32)) {
            timers_[id].cascade_instance = &timers_[id - 1_u32];
        }
    }

    void set_irq_controller_handle(const arm::irq_controller_handle irq_handle) noexcept
    {
        for(timer& t : timers_) {
            t.irq_handle_ = irq_handle;
        }
    }

    [[nodiscard]] timer& operator[](usize idx) noexcept { return timers_[idx]; }
    [[nodiscard]] const timer& operator[](usize idx) const noexcept { return timers_[idx]; }
};

} // namespace gba::arm

#endif //GAMEBOIADVANCE_TIMER_H
