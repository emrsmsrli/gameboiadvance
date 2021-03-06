/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_APU_RESAMPLER_H
#define GAMEBOIADVANCE_APU_RESAMPLER_H

#include <gba/apu/apu_sound_buffer.h>
#include <gba/core/integer.h>
#include <gba/core/event/event.h>

namespace gba::apu {

template<typename Sample>
class resampler {
protected:
    sound_buffer<Sample>& buffer_;
    u32 src_sample_rate_;
    u32 dst_sample_rate_;
    float resample_phase_ = 0.f;
    float resample_phase_shift_ = 1.f;

public:
    explicit resampler(sound_buffer<Sample>& buffer) noexcept
      : buffer_{buffer} {}
    virtual ~resampler() = default;

    virtual void write_sample(const Sample sample) = 0;

    FORCEINLINE void set_src_sample_rate(const u32 src_sample_rate) noexcept
    {
        src_sample_rate_ = src_sample_rate;
        calculate_resample_interval();
    }

    FORCEINLINE void set_dst_sample_rate(const u32 dst_sample_rate) noexcept
    {
        dst_sample_rate_ = dst_sample_rate;
        calculate_resample_interval();
    }

private:
    FORCEINLINE void calculate_resample_interval() noexcept
    {
        resample_phase_shift_ = static_cast<float>(src_sample_rate_.get())
          / static_cast<float>(dst_sample_rate_.get());
    }
};

template<typename Sample>
class cubic_resampler : public resampler<Sample> {
    array<Sample, 3> previous_samples_{};

public:
    explicit cubic_resampler(sound_buffer<Sample>& buffer) noexcept
      : resampler<Sample>{buffer} {}

    void write_sample(const Sample sample) final
    {
        while(this->resample_phase_ < 1.0f) {
            const float mu = this->resample_phase_;
            const float mu_sq = mu * mu;
            const Sample a0 = sample - previous_samples_[0_u32] - previous_samples_[2_u32] + previous_samples_[1_u32];
            const Sample a1 = previous_samples_[2_u32] - previous_samples_[1_u32] - a0;
            const Sample a2 = previous_samples_[0_u32] - previous_samples_[2_u32];
            const Sample a3 = previous_samples_[1_u32];

            this->buffer_.write(a0 * mu * mu_sq
              + a1 * mu_sq
              + a2 * mu
              + a3);

            this->resample_phase_ += this->resample_phase_shift_;
        }

        this->resample_phase_ = this->resample_phase_ - 1.0f;

        previous_samples_[2_u32] = previous_samples_[1_u32];
        previous_samples_[1_u32] = previous_samples_[0_u32];
        previous_samples_[0_u32] = sample;
    }
};

} // namespace gba::apu

#endif //GAMEBOIADVANCE_APU_RESAMPLER_H
