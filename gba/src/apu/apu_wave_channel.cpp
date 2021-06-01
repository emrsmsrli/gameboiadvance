/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/apu/apu_types.h>

namespace gba::apu {

namespace {

constexpr array<i8, 4> volume_table{0_i8, 4_i8, 2_i8, 1_i8};

} // namespace

wave_channel::wave_channel(scheduler* scheduler) noexcept
  : scheduler_{scheduler}
{
    timer_event_id = scheduler_->ADD_EVENT(calculate_sample_rate(), wave_channel::generate_output_sample);
}

void wave_channel::generate_output_sample(const u64 late_cycles) noexcept
{
    timer_event_id = scheduler_->ADD_EVENT(calculate_sample_rate() - late_cycles, wave_channel::generate_output_sample);

    if(enabled && dac_enabled) {
        u8 sample_pair = wave_ram[wave_bank][sample_index / 2_u8];

        if(!bit::test(sample_index, 0_u8)) {
            sample_pair >>= 4_u8;
        } else {
            sample_pair &= 0x0F_u8;
        }

        output = sample_pair;
    } else {
        output = 0_u8;
    }

    ++sample_index;
    if(sample_index == 32_u8) {
        sample_index = 0_u8;
        if(wave_bank_2d) {
            wave_bank ^= 1_u8;
        }
    }
}

i8 wave_channel::get_output() const noexcept
{
    return (make_signed(output) - 8_i8) * 4_i8 * (force_output_level ? 3_i8 : volume_table[output_level]);
}

void wave_channel::length_click() noexcept
{
    if(length_counter > 0_u32 && freq_data.freq_control.use_counter) {
        --length_counter;
        if(length_counter == 0_u32) {
            enabled = false;
        }
    }
}

void wave_channel::restart() noexcept
{
    scheduler_->remove_event(timer_event_id);
    timer_event_id = scheduler_->ADD_EVENT(calculate_sample_rate(), wave_channel::generate_output_sample);

    enabled = true;
    sample_index = 0_u8;
    length_counter = sound_length;
}

void wave_channel::disable() noexcept
{
    length_counter = 0_u32;
    enabled = false;
}

void wave_channel::write(const register_index index, const u8 data) noexcept
{
    switch(index) {
        case register_index::enable:
            wave_bank_2d = bit::test(data, 5_u8);
            wave_bank = bit::extract(data, 6_u8);
            dac_enabled = bit::test(data, 7_u8);
            break;
        case register_index::sound_length:
            sound_length = data;
            break;
        case register_index::output_level:
            output_level = data >> 5_u8;
            force_output_level = bit::test(data, 7_u8);
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
