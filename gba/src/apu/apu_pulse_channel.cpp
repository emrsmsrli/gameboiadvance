/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/apu/apu_types.h>

namespace gba::apu {

namespace {

constexpr array<i32, 32> wave_duty{
  8, -8, -8, -8, -8, -8, -8, -8,
  8,  8, -8, -8, -8, -8, -8, -8,
  8,  8,  8,  8, -8, -8, -8, -8,
  8,  8,  8,  8,  8,  8, -8, -8
};

} // namespace

pulse_channel::pulse_channel(scheduler* scheduler) noexcept
  : scheduler_{scheduler}
{
    timer_event_id = scheduler_->ADD_HW_EVENT(calculate_sample_rate(), pulse_channel::generate_output_sample);
}

void pulse_channel::generate_output_sample(const u32 late_cycles) noexcept
{
    waveform_phase = (waveform_phase + 1_u8) & 0x07_u8;
    adjust_waveform_duty_index();
    adjust_output_volume();

    timer_event_id = scheduler_->ADD_HW_EVENT(calculate_sample_rate() - late_cycles, pulse_channel::generate_output_sample);
}

i8 pulse_channel::get_output() const noexcept
{
    return make_signed(output) * narrow<i8>(wave_duty[waveform_duty_index]);
}

void pulse_channel::length_click() noexcept
{
    if(length_counter > 0_u32 && freq_data.freq_control.use_counter) {
        --length_counter;
        if(length_counter == 0_u32) {
            enabled = false;
            output = 0_u8;
        }
    }
}

void pulse_channel::sweep_click() noexcept
{
    --swp.timer;
    if(swp.timer <= 0_i16) {
        swp.timer = swp.period;
        if(swp.timer == 0_i16) {
            swp.timer = 8_i16;
        }

        if(swp.enabled && swp.period > 0_u8) {
            if(const u16 new_freq = sweep_calculation(); new_freq < 2048_u16 && swp.shift_count > 0_u8) {
                swp.shadow = new_freq;
                freq_data.sample_rate = new_freq;

                sweep_calculation();
            }

            sweep_calculation();
        }
    }
}

void pulse_channel::envelope_click() noexcept
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

void pulse_channel::restart() noexcept
{
    scheduler_->remove_event(timer_event_id);
    timer_event_id = scheduler_->ADD_HW_EVENT(calculate_sample_rate(), pulse_channel::generate_output_sample);

    enabled = true;
    length_counter = 64_u32 - wav_data.sound_length;

    volume = env.initial_volume;
    env.timer = env.period;

    swp.enabled = swp.period > 0_u8 || swp.shift_count > 0_u8;
    swp.timer = swp.period;
    if(swp.timer == 0_i16) {
        swp.timer = 8_i16;
    }

    swp.shadow = freq_data.sample_rate;
    if(swp.shift_count > 0_u8) {
        sweep_calculation();
    }

    adjust_output_volume();
}

void pulse_channel::disable() noexcept
{
    length_counter = 0_u32;
    enabled = false;
    output = 0_u8;
}

u16 pulse_channel::sweep_calculation() noexcept
{
    auto new_freq = swp.shadow >> swp.shift_count;
    switch(swp.direction) {
        case sweep::mode::increase:
            new_freq = swp.shadow + new_freq;
            break;
        case sweep::mode::decrease:
            new_freq = swp.shadow - new_freq;
            break;
    }

    if(new_freq >= 2048_u16) {
        enabled = false;
        output = 0_u8;
    }

    return new_freq;
}

void pulse_channel::adjust_output_volume() noexcept
{
    if(enabled && dac_enabled) {
        output = volume;
    } else {
        output = 0_u8;
    }
}

void pulse_channel::write(const register_index index, const u8 data)
{
    switch(index) {
        case register_index::sweep:
            swp.period = (data >> 4_u8) & 0x7_u8;
            swp.direction = to_enum<sweep::mode>(bit::extract(data, 3_u8));
            swp.shift_count = data & 0x7_u8;
            break;
        case register_index::wave_data:
            wav_data.duty = data >> 6_u8;
            wav_data.sound_length = data & 0x3F_u8;

            adjust_waveform_duty_index();
            adjust_output_volume();
            break;
        case register_index::envelope:
            dac_enabled = (data & 0xF8_u8) != 0x00_u32;
            env.period = data & 0x7_u8;
            env.direction = to_enum<envelope::mode>(bit::extract(data, 3_u8));
            env.initial_volume = data >> 4_u8;
            break;
        case register_index::freq_data:
            freq_data.sample_rate = bit::set_byte(freq_data.sample_rate, 0_u8, data);
            break;
        case register_index::freq_control:
            freq_data.sample_rate = bit::set_byte(freq_data.sample_rate, 1_u8, data & 0x7_u8);
            freq_data.freq_control.use_counter = bit::test(data, 6_u8);
            if(bit::test(data, 7_u8)) {
                restart();
            }
            break;
    }
}

} // namespace gba::apu
