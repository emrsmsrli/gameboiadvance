/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/apu/apu.h>

#include <gba/arm/timer.h>
#include <gba/core/scheduler.h>

namespace gba::apu {

namespace {

constexpr u8 frame_sequencer_max = 8_u8;
constexpr u64 frame_sequencer_cycles = arm::clock_speed / 512_u64;  // runs at 512hz

constexpr array<i16, 4> psg_volume_tab{1_i16, 2_i16, 4_i16, 0_i16};
constexpr array<i16, 2> dma_volume_tab{2_i16, 4_i16};

} // namespace

engine::engine(timer::timer* timer1, timer::timer* timer2, scheduler* scheduler) noexcept
  : scheduler_{scheduler},
    channel_1_{scheduler},
    channel_2_{scheduler},
    channel_3_{scheduler},
    channel_4_{scheduler},
    fifo_a_{&control_.fifo_a, dma::occasion::fifo_a},
    fifo_b_{&control_.fifo_b, dma::occasion::fifo_b},
    resampler_{buffer_}
{
    scheduler_->ADD_HW_EVENT(frame_sequencer_cycles, apu::engine::tick_sequencer);
    scheduler_->ADD_HW_EVENT(soundbias_.sample_interval(), apu::engine::tick_mixer);

    timer1->on_overflow.add_delegate({connect_arg<&engine::on_timer_overflow>, this});
    timer2->on_overflow.add_delegate({connect_arg<&engine::on_timer_overflow>, this});

    resampler_.set_src_sample_rate(soundbias_.sample_rate());
}

void engine::tick_sequencer(const u64 late_cycles) noexcept
{
    scheduler_->ADD_HW_EVENT(frame_sequencer_cycles - late_cycles, apu::engine::tick_sequencer);

    switch(frame_sequencer_.get()) {
        case 0:
        case 4:
            channel_1_.length_click();
            channel_2_.length_click();
            channel_3_.length_click();
            channel_4_.length_click();
            break;
        case 2:
        case 6:
            channel_1_.sweep_click();
            channel_1_.length_click();
            channel_2_.length_click();
            channel_3_.length_click();
            channel_4_.length_click();
            break;
        case 7:
            channel_1_.envelope_click();
            channel_2_.envelope_click();
            channel_4_.envelope_click();
            break;
        default:
            break;
    }

    frame_sequencer_ = (frame_sequencer_ + 1_u8) % frame_sequencer_max;
}

void engine::tick_mixer(const u64 late_cycles) noexcept
{
    resampler_.write_sample(stereo_sample<float>{
      generate_sample(terminal::left).get() / float(0x200),
      generate_sample(terminal::right).get() / float(0x200)
    });

    scheduler_->ADD_HW_EVENT(soundbias_.sample_interval() - late_cycles, apu::engine::tick_mixer);
}

i16 engine::generate_sample(const u32 terminal) noexcept
{
    const i16 psg_volume = psg_volume_tab[control_.psg_volume];
    i16 sample;

    if(control_.psg_enables[terminal][0_u32]) { sample += channel_1_.get_output(); }
    if(control_.psg_enables[terminal][1_u32]) { sample += channel_2_.get_output(); }
    if(control_.psg_enables[terminal][2_u32]) { sample += channel_3_.get_output(); }
    if(control_.psg_enables[terminal][3_u32]) { sample += channel_4_.get_output(); }

    sample += sample * psg_volume * control_.volumes[terminal] / 28_i16;

    if(control_.fifo_a.enables[terminal]) {
        sample += make_signed(fifo_a_.latch()) * dma_volume_tab[bit::from_bool(control_.fifo_a.full_volume)];
    }
    if(control_.fifo_b.enables[terminal]) {
        sample += make_signed(fifo_b_.latch()) * dma_volume_tab[bit::from_bool(control_.fifo_b.full_volume)];
    }

    return std::clamp<i16>(sample + soundbias_.bias, 0_i16, 0x3FF_i16) - 0x200_i16;
}

void engine::on_timer_overflow(timer::timer* timer) noexcept
{
    if(!power_on_) {
        return;
    }

    fifo_a_.on_timer_overflow(timer->id(), dma_);
    fifo_b_.on_timer_overflow(timer->id(), dma_);
}

} // namespace gba::apu
