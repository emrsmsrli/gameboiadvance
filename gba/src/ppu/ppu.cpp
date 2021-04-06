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

void engine::check_vcounter_irq() noexcept
{
    const bool prev_vcounter = dispstat_.vcounter;
    const bool current_vcounter = dispstat_.vcount_setting == vcount_;
    dispstat_.vcounter = current_vcounter;

    if(dispstat_.vcounter_irq_enabled && !prev_vcounter && current_vcounter) {
        irq_.request_interrupt(arm::interrupt_source::vcounter_match);
    }
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

        mosaic_bg_.reset();
        mosaic_obj_.reset();

        bg2_.x_ref.latch();
        bg2_.y_ref.latch();
        bg3_.x_ref.latch();
        bg3_.y_ref.latch();
    } else if(vcount_ == vcount_max) {
        dispstat_.vblank = false;
    }

    check_vcounter_irq();
}

void engine::on_hblank(const u64 cycles_late) noexcept
{
    scheduler_->add_event(cycles_hblank - cycles_late, {connect_arg<&engine::on_hdraw>, this});
    dispstat_.hblank = true;

    if(dispstat_.hblank_irq_enabled) {
        irq_.request_interrupt(arm::interrupt_source::hblank);
    }

    const bool any_window_enabled = dispcnt_.win0_enabled || dispcnt_.win1_enabled || dispcnt_.win_obj_enabled;
    if(any_window_enabled) {
        generate_window_buffer();
    }

    if(vcount_ < screen_height) {
        dma_.request_dma(dma::controller::occasion::hblank);
        render_scanline();

        mosaic_bg_.update_internal_v();
        mosaic_obj_.update_internal_v();

        if(dispcnt_.bg_mode > 0_u8) {
            const auto update_bg_affine_internals = [&](bg_affine& bg) {
                const i32 pb = make_signed(bg.pb);
                const i32 pd = make_signed(bg.pd);

                if(bg.cnt.mosaic_enabled) {
                    if(mosaic_bg_.internal.v == 0_u8) {
                        bg.x_ref.internal += pb * make_signed(mosaic_bg_.v);
                        bg.y_ref.internal += pd * make_signed(mosaic_bg_.v);
                    }
                } else {
                    bg.x_ref.internal += pb;
                    bg.y_ref.internal += pd;
                }
            };

            update_bg_affine_internals(bg2_);
            update_bg_affine_internals(bg3_);
        }
    }

    static constexpr range<u8> video_dma_range{video_dma_start_line, video_dma_end_line};
    if(vcount_ == video_dma_range.max()) {
        dma_.disable_video_transfer();
    } else if(video_dma_range.contains(vcount_)) {
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
            compose(bg0_, bg1_, bg2_);
            break;
        case 2:
            render_bg_affine(bg2_, bg3_);
            render_obj();
            compose(bg2_, bg3_);
            break;
        case 3:
            render_affine_loop(bg2_, make_signed(screen_width), make_signed(screen_height),
              [&](const u32 screen_x, const u32 x, const u32 y) {
                  bg_buffers_[2_usize][screen_x] = color{memcpy<u16>(vram_, (y * screen_width + x) * 2_u32)};
              });

            render_obj();
            compose(bg2_);
            break;
        case 4:
            render_affine_loop(bg2_, make_signed(screen_width), make_signed(screen_height),
              [&](const u32 screen_x, const u32 x, const u32 y) {
                  bg_buffers_[2_usize][screen_x] = palette_color_opaque(
                    memcpy<u8>(vram_, dispcnt_.frame_select * 40_kb + (y * screen_width + x)));
              });

            render_obj();
            compose(bg2_);
            break;
        case 5: {
            constexpr u32 small_bitmap_width = 160_u32;
            constexpr i32 small_bitmap_height = 128_i32;
            render_affine_loop(bg2_, make_signed(small_bitmap_width), small_bitmap_height,
              [&](const u32 screen_x, const u32 x, const u32 y) {
                  bg_buffers_[2_usize][screen_x] = color{memcpy<u16>(vram_,
                    dispcnt_.frame_select * 40_kb + (y * small_bitmap_width + x) * 2_u32)};
              });

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

    event_on_scanline(vcount_, final_buffer_);
}

} // namespace gba::ppu
