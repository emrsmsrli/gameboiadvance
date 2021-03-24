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
                        delay -= 2_u64;
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
    last_scheduled_timestamp_ = scheduler_->now() - cycles_late;
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
        arm_->request_interrupt(to_enum<interrupt_source>(
          from_enum<u32>(interrupt_source::timer_0_overflow) << id_));
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

} // namespace gba::arm
