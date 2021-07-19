/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_APU_H
#define GAMEBOIADVANCE_APU_H

#include <gba/apu/apu_resampler.h>
#include <gba/apu/apu_types.h>
#include <gba/arm/dma_controller.h>
#include <gba/core/fwd.h>

namespace gba::apu {

class engine {
    friend class arm::arm7tdmi;

    scheduler* scheduler_;
    dma::controller_handle dma_;

    bool power_on_ = false;

    cnt control_;
    soundbias soundbias_;

    pulse_channel channel_1_;
    pulse_channel channel_2_;
    wave_channel channel_3_;
    noise_channel channel_4_;
    fifo fifo_a_;
    fifo fifo_b_;

    u8 frame_sequencer_;

    sound_buffer<stereo_sample<float>> buffer_;
    cubic_resampler<stereo_sample<float>> resampler_;

public:
    engine(timer::timer* timer1, timer::timer* timer2, scheduler* scheduler) noexcept;

    FORCEINLINE void set_dma_controller_handle(const dma::controller_handle dma) noexcept { dma_ = dma; }
    FORCEINLINE void set_dst_sample_rate(const u32 sample_rate) noexcept { resampler_.set_dst_sample_rate(sample_rate); }
    FORCEINLINE void set_buffer_capacity(const usize capacity) noexcept { buffer_.set_capacity(capacity); }
    FORCEINLINE event<vector<stereo_sample<float>>>& get_buffer_overflow_event() noexcept { return buffer_.on_overflow; }

private:
    template<u32::type ChannelIdx, typename RegisterEnum>
    void write(const RegisterEnum reg_idx, const u8 data) noexcept
    {
        if(!power_on_) { return; }
        if constexpr(ChannelIdx == 1)       { channel_1_.write(reg_idx, data); }
        else if constexpr(ChannelIdx == 2)  { channel_2_.write(reg_idx, data); }
        else if constexpr(ChannelIdx == 3)  { channel_3_.write(reg_idx, data); }
        else if constexpr(ChannelIdx == 4)  { channel_4_.write(reg_idx, data); }
    }

    void tick_sequencer(u64 late_cycles) noexcept;
    void tick_mixer(u64 late_cycles) noexcept;
    [[nodiscard]] i16 generate_sample(u32 terminal) noexcept;

    void on_timer_overflow(timer::timer* timer) noexcept;
};

} // namespace gba::apu

#endif //GAMEBOIADVANCE_APU_H
