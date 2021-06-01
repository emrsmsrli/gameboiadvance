/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/apu/apu_types.h>

namespace gba::apu {

noise_channel::noise_channel(scheduler* scheduler) noexcept
  : scheduler_{scheduler}
{
    timer_event_id = scheduler_->ADD_EVENT(calculate_sample_rate(), noise_channel::generate_output_sample);
}

void noise_channel::generate_output_sample(const u64 late_cycles) noexcept
{
    timer_event_id = scheduler_->ADD_EVENT(calculate_sample_rate() - late_cycles, noise_channel::generate_output_sample);

    const u16 first_bit_reverse = bit::extract(~lfsr, 0_u8);
    const u16 result = bit::extract(lfsr, 0_u8) ^ bit::extract(lfsr, 1_u8);
    lfsr >>= 1u;
    lfsr |= result << 14u;

    if(polynomial_cnt.has_7_bit_counter_width) {
        lfsr = (lfsr & 0xBF_u16) | (result << 6_u16);
    }

    if(enabled && dac_enabled) {
        output = (16_i8 * narrow<u8>(first_bit_reverse) - 8_i8) * volume;
    } else {
        output = 0_i8;
    }
}

void noise_channel::length_click() noexcept
{
    if(length_counter > 0_u32 && freq_control.use_counter) {
        --length_counter;
        if(length_counter == 0_u32) {
            enabled = false;
        }
    }
}

void noise_channel::envelope_click() noexcept
{
    --env.timer;
    if(env.timer <= 0_i32) {
        env.timer = env.period;
        if(env.timer == 0_i32) {
            env.timer = 8_i32;
        }

        if(env.period > 0_u8) {
            switch(env.direction) {
                case envelope::mode::increase:
                    if(volume < 15_u8) {
                        ++volume;
                    }
                    break;
                case envelope::mode::decrease:
                    if(volume > 0_u8) {
                        --volume;
                    }
                    break;
            }
        }
    }
}

void noise_channel::restart() noexcept
{
    scheduler_->remove_event(timer_event_id);
    timer_event_id = scheduler_->ADD_EVENT(calculate_sample_rate(), noise_channel::generate_output_sample);

    enabled = true;
    length_counter = sound_length;

    env.timer = env.period;
    if(env.timer == 0_i32) {
        env.timer = 8_i32;
    }

    volume = env.initial_volume;
    lfsr = 0x7FFF_u16;
}

void noise_channel::disable() noexcept
{
    length_counter = 0_u32;
    enabled = false;
    output = 0_i8;
}

void noise_channel::write(const register_index index, const u8 data) noexcept
{
    switch(index) {
        case register_index::sound_length:
            sound_length = 0x40_u8 - (data & 0x3F_u8);
            break;
        case register_index::envelope:
            dac_enabled = (data & 0xF8_u8) != 0x00_u32;
            env.period = data & 0x7_u8;
            env.direction = to_enum<envelope::mode>(bit::extract(data, 3_u8));
            env.initial_volume = data >> 4_u8;
            break;
        case register_index::polynomial_counter:
            polynomial_cnt.dividing_ratio = data & 0x7_u8;
            polynomial_cnt.has_7_bit_counter_width = bit::test(data, 3_u8);
            polynomial_cnt.shift_clock_frequency = data >> 4_u8;
            break;
        case register_index::freq_control:
            freq_control.use_counter = bit::test(data, 6_u8);
            if(bit::test(data, 7_u8)) {
                restart();
            }
            break;
    }
}

} // namespace gba::apu
