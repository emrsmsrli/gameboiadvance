/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/ppu/ppu.h>

#include <algorithm>

#include <gba/core/scheduler.h>
#include <gba/helper/range.h>

namespace gba::ppu {

namespace {

constexpr u64 cycles_hdraw = 1006_u64;
constexpr u64 cycles_hblank = 226_u64;
constexpr u8 total_lines = 228_u8;
constexpr u8 vcount_max = total_lines - 1_u8;

constexpr u8 video_dma_start_line = 2_u8;
constexpr u8 video_dma_end_line = 162_u8;

} // namespace

engine::engine(scheduler* scheduler) noexcept
  : scheduler_{scheduler}
{
    scheduler_->add_event(cycles_hdraw, {connect_arg<&engine::on_hblank>, this});
}

void engine::on_hdraw(const u64 cycles_late) noexcept
{
    scheduler_->add_event(cycles_hdraw - cycles_late, {connect_arg<&engine::on_hblank>, this});
    dispstat_.hblank = false;

    vcount_ = (vcount_ + 1_u8) % total_lines;
    if(vcount_ == screen_height) {
        dispstat_.vblank = true;
        event_on_vblank();

        dma_.request_dma(dma::controller::occasion::vblank);

        if(dispstat_.vblank_irq_enabled) {
            irq_.request_interrupt(arm::interrupt_source::vblank);
        }

        // todo reset mosaic regs

        bg2_.x_ref.latch();
        bg2_.y_ref.latch();
        bg3_.x_ref.latch();
        bg3_.y_ref.latch();
    } else if(vcount_ == vcount_max) {
        dispstat_.vblank = false;
    }

    const bool prev_vcounter = dispstat_.vcounter;
    const bool current_vcounter = dispstat_.vcount_setting == vcount_;
    dispstat_.vcounter = current_vcounter;

    if(dispstat_.vcounter_irq_enabled && !prev_vcounter && current_vcounter) {
        irq_.request_interrupt(arm::interrupt_source::vcounter_match);
    }
}

void engine::on_hblank(const u64 cycles_late) noexcept
{
    scheduler_->add_event(cycles_hblank - cycles_late, {connect_arg<&engine::on_hdraw>, this});
    dispstat_.hblank = true;

    if(dispstat_.hblank_irq_enabled) {
        irq_.request_interrupt(arm::interrupt_source::hblank);
    }

    if(vcount_ < screen_height) {
        dma_.request_dma(dma::controller::occasion::hblank);
        render_scanline();

        // todo update mosaic
        // todo update affine regs
    }

    if(vcount_ == video_dma_end_line) {
        dma_.disable_video_transfer();
    } else if(vcount_ >= video_dma_start_line) {
        dma_.request_dma(dma::controller::occasion::video);
    }
}

void engine::render_scanline() noexcept
{
    if(dispcnt_.forced_blank) {
        std::fill(final_buffer_.begin(), final_buffer_.end(), color::white());
        return;
    }

    switch(dispcnt_.bg_mode.get()) {
        case 0:
            render_bg_regular(bg0_, bg1_, bg2_, bg3_);
            render_obj();
            compose(bg0_, bg1_, bg2_, bg3_);
            break;
        case 1:
            render_bg_regular(bg0_, bg1_);
            render_bg_affine(bg2_);
            render_obj();
            compose(bg0_, bg1_);
            break;
        case 2:
            render_bg_affine(bg2_, bg3_);
            render_obj();
            compose(bg2_, bg3_);
            break;
        case 3:
            for(u32 x : range(screen_width)) {
                bg_buffers_[2_usize][x] = color{memcpy<u16>(vram_, (vcount_ * screen_width + x) * 2_u32)};
            }
            render_obj();
            compose(bg2_);
            break;
        case 4:
            for(u32 x : range(screen_width)) {
                bg_buffers_[2_usize][x] = palette_color_opaque(memcpy<u8>(vram_,
                  dispcnt_.frame_select * 40_kb + (vcount_ * screen_width + x)));
            }
            render_obj();
            compose(bg2_);
            break;
        case 5: {
            constexpr u32 small_bitmap_width = 160_u32;
            constexpr u32 small_bitmap_height = 128_u32;
            if(vcount_ < small_bitmap_height) {
                for(const u32 x : range(small_bitmap_width)) {
                    bg_buffers_[2_usize][x] = color{memcpy<u16>(vram_,
                      dispcnt_.frame_select * 40_kb + (vcount_ * small_bitmap_width + x) * 2_u32)};
                }
                // fill remaining backdrop
                for(const u32 x : range(small_bitmap_width, u32(screen_width))) {
                    bg_buffers_[2_usize][x] = backdrop_color();
                }
            } else {
                for(const u32 x : range(screen_width)) {
                    bg_buffers_[2_usize][x] = backdrop_color();
                }
            }
            render_obj();
            compose(bg2_);
            break;
        }
        case 6:
        case 7: // invalid modes, fill line with backdrop color
            std::fill(final_buffer_.begin(), final_buffer_.end(), backdrop_color());
            break;
        default:
            UNREACHABLE();
    }

    if(UNLIKELY(green_swap_)) {
        for(u32 x = 0_u32; x < screen_width; x += 2_u32) {
            final_buffer_[x].swap_green(final_buffer_[x + 1_u32]);
        }
    }

    event_on_scanline(vcount_, final_buffer_);
}

} // namespace gba::ppu
