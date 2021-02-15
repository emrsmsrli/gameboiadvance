/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/timer.h>
#include <gba/arm/arm7tdmi.h>

namespace gba::arm {

namespace {

constexpr array prescalar_shifts{0_u8, 6_u8, 8_u8, 10_u8};
constexpr array start_delay_masks{0_u8, 0x3F_u8, 0xFF_u8, 0x3FF_u8};
constexpr u32 overflow_value = 0x1'0000_u32;

} // namespace

u8 timer::read(const timer::register_type reg) const noexcept
{
    switch(reg) {
        case register_type::cnt_l_lsb: return narrow<u8>(counter_);
        case register_type::cnt_l_msb: return narrow<u8>(counter_ >> 8_u32);
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
            reload_ = (reload_ & 0xFF00_u16) | data;
            break;
        case register_type::cnt_l_msb:
            reload_ = (reload_ & 0x00FF_u16) | (widen<u16>(data) << 8_u16);
            break;
        case register_type::cnt_h_lsb: {
            const bool was_enabled = control_.enabled;

            control_.enabled = bit::test(data, 7_u8);
            control_.irq_enabled = bit::test(data, 6_u8);
            control_.cascaded = (id_ > 0_u32) && bit::test(data, 2_u8);
            control_.prescalar = data & 0b11_u8;

            if(scheduler_->has_event(handle_)) {
                scheduler_->remove_event(handle_);
                counter_ += (scheduler_->now() - last_scheduled_timestamp_) >> prescalar_shifts[control_.prescalar];
                if(counter_ >= overflow_value) {
                    overflow_internal();
                }
            }

            if(id_ > 0_u32) {
                arm_->timers_[id_ - 1_u32]
                  .on_overflow
                  .remove_delegate({connect_arg<&timer::tick_internal>, this});
            }

            if(control_.enabled) {
                if(!was_enabled) {
                    counter_ = reload_;
                }

                if(control_.cascaded) {
                    arm_->timers_[id_ - 1_u32]
                      .on_overflow
                      .add_delegate({connect_arg<&timer::tick_internal>, this});
                } else {
                    u64 delay = scheduler_->now() & start_delay_masks[control_.prescalar];
                    if(!was_enabled) {
                        delay - 2_u64;
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

void timer::schedule_overflow(const u64 cycles_late) noexcept
{
    last_scheduled_timestamp_ = scheduler_->now();
    handle_ = scheduler_->add_event(
      ((overflow_value - counter_) << prescalar_shifts[control_.prescalar]) - cycles_late,
      {connect_arg<&timer::overflow>, this});
}

void timer::overflow(const u64 cycles_late) noexcept
{
    overflow_internal();
    schedule_overflow(cycles_late);
}

void timer::overflow_internal() noexcept
{
    counter_ = reload_;

    if(control_.irq_enabled) {
        arm_->request_interrupt(
          static_cast<interrupt_source>(
            static_cast<u32::type>(interrupt_source::timer_0_overflow) << id_.get()));
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

} // namespace gba::arm