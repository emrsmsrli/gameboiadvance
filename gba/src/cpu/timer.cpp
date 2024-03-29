/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cpu/timer.h>

#include <gba/archive.h>

namespace gba::timer {

namespace {

constexpr array prescalar_shifts{0_u8, 6_u8, 8_u8, 10_u8};
constexpr array start_delay_masks{0_u8, 0x3F_u8, 0xFF_u8, 0x3FF_u8};
constexpr u32 overflow_value = 0x1'0000_u32;

} // namespace

u8 timer::read(const register_type reg) const noexcept
{
    u32 counter = counter_;
    if(scheduler_->has_event(handle_)) {
        counter += calculate_counter_delta();
    }

    switch(reg) {
        case register_type::cnt_l_lsb: return narrow<u8>(counter);
        case register_type::cnt_l_msb: return narrow<u8>(counter >> 8_u32);
        case register_type::cnt_h_lsb:
            return control_.prescalar
              | bit::from_bool<u8>(control_.cascaded) << 2_u8
              | bit::from_bool<u8>(control_.irq_enabled) << 6_u8
              | bit::from_bool<u8>(control_.enabled) << 7_u8;
        default:
            UNREACHABLE();
    }
}

void timer::write(const register_type reg, const u8 data) noexcept
{
    switch(reg) {
        case register_type::cnt_l_lsb:
            reload_ = bit::set_byte(reload_, 0_u8, data);
            break;
        case register_type::cnt_l_msb:
            reload_ = bit::set_byte(reload_, 1_u8, data);
            break;
        case register_type::cnt_h_lsb: {
            const bool was_enabled = control_.enabled;

            control_.enabled = bit::test(data, 7_u8);
            control_.irq_enabled = bit::test(data, 6_u8);
            control_.cascaded = (id_ > 0_u32) && bit::test(data, 2_u8);
            control_.prescalar = data & 0b11_u8;

            if(scheduler_->has_event(handle_)) {
                scheduler_->remove_event(handle_);
                counter_ += calculate_counter_delta();
                if(counter_ >= overflow_value) {
                    overflow_internal();
                }
            }

            if(cascade_instance) {
                cascade_instance->on_overflow.remove_delegate({connect_arg<&timer::tick_internal>, this});
            }

            if(control_.enabled) {
                if(!was_enabled) {
                    counter_ = reload_;
                }

                if(control_.cascaded) {
                    cascade_instance->on_overflow.add_delegate({connect_arg<&timer::tick_internal>, this});
                } else {
                    u32 delay = narrow<u32>(scheduler_->now() & start_delay_masks[control_.prescalar]);
                    if(!was_enabled) {
                        delay -= 2_u32;
                    }

                    schedule_overflow(delay);
                }
            }
            break;
        }
        default:
            UNREACHABLE();
    }
}

void timer::schedule_overflow(const u32 late_cycles) noexcept
{
    last_scheduled_timestamp_ = scheduler_->now() - late_cycles;
    handle_ = scheduler_->add_hw_event(
      ((overflow_value - counter_) << prescalar_shifts[control_.prescalar]) - late_cycles,
      MAKE_HW_EVENT(timer::overflow));
}

void timer::overflow(const u32 late_cycles) noexcept
{
    overflow_internal();
    schedule_overflow(late_cycles);
}

void timer::overflow_internal() noexcept
{
    counter_ = reload_;

    if(control_.irq_enabled) {
        irq_handle_.request_interrupt(to_enum<cpu::interrupt_source>(
          from_enum<u32>(cpu::interrupt_source::timer_0_overflow) << id_));
    }

    on_overflow(this);
}

void timer::tick_internal() noexcept
{
    ++counter_;

    if(counter_ == overflow_value) {
        overflow_internal();
    }
}

u32 timer::calculate_counter_delta() const noexcept
{
    return narrow<u32>(scheduler_->now() - last_scheduled_timestamp_) >> prescalar_shifts[control_.prescalar];
}

void timer::serialize(archive& archive) const noexcept
{
    archive.serialize(handle_);
    archive.serialize(last_scheduled_timestamp_);
    archive.serialize(counter_);
    archive.serialize(reload_);
    archive.serialize(control_.prescalar);
    archive.serialize(control_.enabled);
    archive.serialize(control_.cascaded);
    archive.serialize(control_.irq_enabled);
}

void timer::deserialize(const archive& archive) noexcept
{
    archive.deserialize(handle_);
    archive.deserialize(last_scheduled_timestamp_);
    archive.deserialize(counter_);
    archive.deserialize(reload_);
    archive.deserialize(control_.prescalar);
    archive.deserialize(control_.enabled);
    archive.deserialize(control_.cascaded);
    archive.deserialize(control_.irq_enabled);
}

controller::controller(scheduler* scheduler, cpu::irq_controller_handle irq) noexcept
  : timers_{
      timer{0_u32, scheduler, irq},
      timer{1_u32, scheduler, irq},
      timer{2_u32, scheduler, irq},
      timer{3_u32, scheduler, irq}
    }
{
    hw_event_registry::get().register_entry(MAKE_HW_EVENT_V(timer::overflow, timers_.ptr(0_usize)), "timer0::overflow");
    hw_event_registry::get().register_entry(MAKE_HW_EVENT_V(timer::overflow, timers_.ptr(1_usize)), "timer1::overflow");
    hw_event_registry::get().register_entry(MAKE_HW_EVENT_V(timer::overflow, timers_.ptr(2_usize)), "timer2::overflow");
    hw_event_registry::get().register_entry(MAKE_HW_EVENT_V(timer::overflow, timers_.ptr(3_usize)), "timer3::overflow");

    for(u32 id : range(1_u32, 4_u32)) {
        timers_[id].cascade_instance = &timers_[id - 1_u32];
    }
}

void controller::serialize(archive& archive) const noexcept
{
    for(const timer& t : timers_) {
        archive.serialize(t);
    }
}

void controller::deserialize(const archive& archive) noexcept
{
    for(timer& t : timers_) {
        archive.deserialize(t);
    }
}

} // namespace gba::timer
