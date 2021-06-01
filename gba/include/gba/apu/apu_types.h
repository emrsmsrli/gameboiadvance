/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_APU_TYPES_H
#define GAMEBOIADVANCE_APU_TYPES_H

#include <gba/core/container.h>
#include <gba/core/scheduler.h>
#include <gba/core/math.h>
#include <gba/arm/dma_controller.h>
#include <gba/helper/range.h>

namespace gba::apu {

struct terminal {
    static constexpr inline u32 left = 0_u32;
    static constexpr inline u32 right = 1_u32;
    static constexpr inline u32::type count = 2_u32;
};

template<typename T>
struct stereo_sample {
    T left;
    T right;

    stereo_sample operator+(const stereo_sample<T> other) const noexcept
    {
        return {left + other.left, right + other.right};
    }

    stereo_sample operator-(const stereo_sample<T> other) const noexcept
    {
        return {left - other.left, right - other.right};
    }

    stereo_sample operator*(const T factor) const noexcept
    {
        return {left * factor, right * factor};
    }
};

struct soundbias {
    u16 bias = 0x200_u16;
    u8 resolution;

    [[nodiscard]] FORCEINLINE u32 sample_interval() const noexcept { return 512_u32 >> resolution; }
    [[nodiscard]] FORCEINLINE u32 sample_rate() const noexcept { return 32768_u32 << resolution; }
};

struct cnt {
    struct fifo_cnt {
        bool full_volume = false;
        array<bool, terminal::count> enables{false, false};
        u8 selected_timer_id;
    };

    array<u8, terminal::count> volumes;
    using channel_enable_t = array<bool, 4>;
    array<channel_enable_t, terminal::count> psg_enables{{
      {false, false, false, false},
      {false, false, false, false}}
    };

    u8 psg_volume;
    fifo_cnt fifo_a;
    fifo_cnt fifo_b;

    template<u32::type N>
    void write(const u8 data) noexcept
    {
        static_assert(N < 4);
        switch(N) {
            case 0:
                volumes[terminal::right] = data & 0x7_u8;
                volumes[terminal::left] = (data >> 4_u8) & 0x7_u8;
                break;
            case 1:
                for(u8 ch : range<u8>(4_u8)) {
                    psg_enables[terminal::right][ch] = bit::test(data, ch);
                    psg_enables[terminal::left][ch] = bit::test(data, ch + 4_u8);
                }
                break;
            case 2:
                psg_volume = data & 0b11_u8;
                fifo_a.full_volume = bit::test(data, 2_u8);
                fifo_b.full_volume = bit::test(data, 3_u8);
                break;
            case 3:
                fifo_a.enables[terminal::right] = bit::test(data, 0_u8);
                fifo_a.enables[terminal::left] = bit::test(data, 1_u8);
                fifo_a.selected_timer_id = bit::extract(data, 2_u8);

                fifo_b.enables[terminal::right] = bit::test(data, 4_u8);
                fifo_b.enables[terminal::left] = bit::test(data, 5_u8);
                fifo_b.selected_timer_id = bit::extract(data, 6_u8);
                break;
            default:
                break;
        }
    }

    template<u32::type N>
    [[nodiscard]] u8 read() const noexcept
    {
        static_assert(N < 4);
        switch(N) {
            case 0: return volumes[terminal::right] | (volumes[terminal::left] << 4_u8);
            case 1: {
                u8 ret;
                for(u8 ch : range<u8>(4_u8)) {
                    if(psg_enables[terminal::right][ch]) { ret = bit::set(ret, ch); }
                    if(psg_enables[terminal::left][ch]) { ret = bit::set(ret, ch + 4_u8); }
                }
                return ret;
            }
            case 2:
                return psg_volume
                  | bit::from_bool<u8>(fifo_a.full_volume) << 2_u8
                  | bit::from_bool<u8>(fifo_b.full_volume) << 3_u8;
            case 3:
                return bit::from_bool<u8>(fifo_a.enables[terminal::right])
                  | bit::from_bool<u8>(fifo_a.enables[terminal::left]) << 1_u8
                  | fifo_a.selected_timer_id << 2_u8
                  | bit::from_bool<u8>(fifo_b.enables[terminal::right]) << 4_u8
                  | bit::from_bool<u8>(fifo_b.enables[terminal::left]) << 5_u8
                  | fifo_b.selected_timer_id << 6_u8;
            default:
                return 0_u8;
        }
    }
};

struct sweep {
    enum class mode : u8::type {
        increase = 0, decrease = 1
    };

    i32 timer;
    u16 shadow;

    u8 period;
    mode direction{mode::increase};
    u8 shift_count;

    bool enabled = false;

    [[nodiscard]] FORCEINLINE u8 read() const noexcept { return shift_count | from_enum<u8>(direction) << 3_u8 | period << 4_u8; }
};

struct wave_data {
    u8 duty;
    u8 sound_length;

    [[nodiscard]] FORCEINLINE u8 read() const noexcept { return duty << 6_u8; }
};

struct envelope {
    enum class mode : u8::type {
        decrease = 0, increase = 1
    };

    i32 timer;

    u8 period;
    mode direction{mode::decrease};
    u8 initial_volume;

    [[nodiscard]] FORCEINLINE u8 read() const noexcept
    {
        return period | from_enum<u8>(direction) << 3_u8 | initial_volume << 4_u8;
    }
};

struct frequency_control {
    bool use_counter = false;

    [[nodiscard]] FORCEINLINE u8 read() const noexcept { return bit::from_bool<u8>(use_counter) << 6_u8; }
};

struct frequency_data {
    u16 sample_rate;
    frequency_control freq_control;
};

struct polynomial_counter {
    u8 shift_clock_frequency;
    bool has_7_bit_counter_width = false;
    u8 dividing_ratio;

    [[nodiscard]] FORCEINLINE u8 read() const noexcept
    {
        return dividing_ratio
          | bit::from_bool<u8>(has_7_bit_counter_width) << 3_u8
          | shift_clock_frequency << 4_u8;
    }
};

struct pulse_channel {
private:
    scheduler* scheduler_;
    scheduler::event::handle timer_event_id;

public:
    enum class register_index {
        sweep = 0,
        wave_data = 1,
        envelope = 2,
        freq_data = 3,
        freq_control = 4
    };

    sweep swp;
    wave_data wav_data;
    envelope env;
    frequency_data freq_data;

    u32 length_counter;
    u32 waveform_duty_index;
    u8 waveform_phase;
    u8 volume;
    u8 output;

    bool enabled = true;
    bool dac_enabled = true;

    explicit pulse_channel(scheduler* scheduler) noexcept;

    void generate_output_sample(u64 late_cycles) noexcept;
    [[nodiscard]] i8 get_output() const noexcept;

    void write(register_index index, u8 data);

    void length_click() noexcept;
    void sweep_click() noexcept;
    void envelope_click() noexcept;

    void restart() noexcept;
    void disable() noexcept;

    [[nodiscard]] u32 calculate_sample_rate() const noexcept { return 128_u32 * (2048_u32 - freq_data.sample_rate) / 8_u32; }
    void adjust_waveform_duty_index() noexcept { waveform_duty_index = wav_data.duty * 8_u32 + waveform_phase; }

    u16 sweep_calculation() noexcept;
    void adjust_output_volume() noexcept;
};

struct wave_channel {
private:
    scheduler* scheduler_;
    scheduler::event::handle timer_event_id;

public:
    enum class register_index {
        enable = 0,
        sound_length = 1,
        output_level = 2,
        freq_data = 3,
        freq_control = 4
    };

    u8 sound_length;
    u8 output_level;
    bool force_output_level = false;
    frequency_data freq_data;

    u32 length_counter;
    u8 sample_index;
    u8 output;

    bool enabled = true;
    bool dac_enabled = true;

    bool wave_bank_2d = false;
    u8 wave_bank;

    using wave_pattern_bank = array<u8, 16>;
    array<wave_pattern_bank, 2> wave_ram{};

    explicit wave_channel(scheduler* scheduler) noexcept;

    void generate_output_sample(u64 late_cycles) noexcept;
    [[nodiscard]] i8 get_output() const noexcept;

    void write(register_index index, u8 data) noexcept;

    void length_click() noexcept;
    void restart() noexcept;
    void disable() noexcept;

    [[nodiscard]] u32 calculate_sample_rate() const noexcept { return (2048_u32 - freq_data.sample_rate) * 8_u32; }

    void write_wave_ram(const u32 address, const u8 data) noexcept { wave_ram[wave_bank ^ 1_u8][address] = data; }
    [[nodiscard]] u8 read_wave_ram(const u32 address) const noexcept { return wave_ram[wave_bank ^ 1_u8][address]; }
};

struct noise_channel {
private:
    scheduler* scheduler_;
    scheduler::event::handle timer_event_id;

public:
    enum class register_index {
        sound_length = 1,
        envelope = 2,
        polynomial_counter = 3,
        freq_control = 4
    };

    u8 sound_length;
    envelope env;
    polynomial_counter polynomial_cnt;
    frequency_control freq_control;

    u32 length_counter;
    u16 lfsr;
    u8 volume;
    i8 output;

    bool enabled = true;
    bool dac_enabled = true;

    explicit noise_channel(scheduler* scheduler) noexcept;

    void generate_output_sample(u64 late_cycles) noexcept;
    [[nodiscard]] FORCEINLINE i8 get_output() const noexcept { return output; }

    void write(register_index index, u8 data) noexcept;

    void length_click() noexcept;
    void envelope_click() noexcept;

    void restart() noexcept;
    void disable() noexcept;

    [[nodiscard]] u32 calculate_sample_rate() const noexcept
    {
        // fixme from crab >>>>> ((@divisor_code == 0 ? 8_u32 : @divisor_code.to_u32 << 4) << @clock_shift) * 4
        const u8 divisor = polynomial_cnt.dividing_ratio;
        return (divisor == 0_u8 ? 4_u8 : divisor * 8_u32) << polynomial_cnt.shift_clock_frequency;
    }
};

class fifo {
    static inline constexpr u32::type capacity = 32_u32;

    cnt::fifo_cnt* control_;
    dma::occasion dma_occasion_;
    u8 latch_;

    array<u8, capacity> data_;
    u32 read_idx_;
    u32 write_idx_;
    u32 size_;

public:
    fifo(cnt::fifo_cnt* control, dma::occasion dma_occasion)
      : control_{control},
        dma_occasion_{dma_occasion} {}

    void reset() noexcept
    {
        read_idx_ = 0_u32;
        write_idx_ = 0_u32;
        size_ = 0_u32;
    }

    void write(const u8 sample) noexcept
    {
        if(size_ < capacity) {
            data_[write_idx_] = sample;
            write_idx_ = (write_idx_ + 1_u32) % capacity;
            size_++;
        }
    }

    [[nodiscard]] u8 read() noexcept
    {
        const u8 value = data_[read_idx_];
        if(size_ > 0_u32) {
            read_idx_ = (read_idx_ + 1_u32) % capacity;
            size_--;
        }
        return value;
    }

    [[nodiscard]] FORCEINLINE u32 size() const noexcept { return size_; }
    [[nodiscard]] FORCEINLINE u8 latch() const noexcept { return latch_; }

    void on_timer_overflow(u32 timer_id, dma::controller_handle dma) noexcept
    {
        if(control_->selected_timer_id == timer_id) {
            latch_ = read();
            if(size() <= 16_u32) {
                dma.request_dma(dma_occasion_);
            }
        }
    }
};

} // namespace gba::apu

#endif //GAMEBOIADVANCE_APU_TYPES_H
