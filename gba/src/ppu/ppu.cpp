/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/ppu/ppu.h>

#include <algorithm>

#include <gba/archive.h>
#include <gba/core/scheduler.h>
#include <gba/helper/range.h>

namespace gba::ppu {

namespace {

constexpr u32 cycles_hdraw = 1006_u32;
constexpr u32 cycles_hblank = 226_u32;
constexpr u8 total_lines = 228_u8;
constexpr u8 vcount_max = total_lines - 1_u8;

constexpr u8 video_dma_start_line = 2_u8;
constexpr u8 video_dma_end_line = 162_u8;

} // namespace

engine::engine(scheduler* scheduler) noexcept
  : scheduler_{scheduler}
{
    hw_event_registry::get().register_entry(MAKE_HW_EVENT(ppu::engine::on_hblank), "ppu::hblank");
    hw_event_registry::get().register_entry(MAKE_HW_EVENT(ppu::engine::on_hdraw), "ppu::hdraw");

    scheduler_->add_hw_event(cycles_hdraw, MAKE_HW_EVENT(ppu::engine::on_hblank));
}

void engine::check_vcounter_irq() noexcept
{
    const bool prev_vcounter = dispstat_.vcounter;
    const bool current_vcounter = dispstat_.vcount_setting == vcount_;
    dispstat_.vcounter = current_vcounter;

    if(dispstat_.vcounter_irq_enabled && !prev_vcounter && current_vcounter) {
        irq_.request_interrupt(cpu::interrupt_source::vcounter_match);
    }
}

void engine::on_hdraw(const u32 late_cycles) noexcept
{
    scheduler_->add_hw_event(cycles_hdraw - late_cycles, MAKE_HW_EVENT(ppu::engine::on_hblank));
    dispstat_.hblank = false;

    vcount_ = (vcount_ + 1_u8) % total_lines;
    if(vcount_ == screen_height) {
        dispstat_.vblank = true;
        event_on_vblank();

        dma_.request_dma(dma::occasion::vblank);

        if(dispstat_.vblank_irq_enabled) {
            irq_.request_interrupt(cpu::interrupt_source::vblank);
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

void engine::on_hblank(const u32 late_cycles) noexcept
{
    scheduler_->add_hw_event(cycles_hblank - late_cycles, MAKE_HW_EVENT(ppu::engine::on_hdraw));
    dispstat_.hblank = true;

    if(dispstat_.hblank_irq_enabled) {
        irq_.request_interrupt(cpu::interrupt_source::hblank);
    }

    const bool any_window_enabled = dispcnt_.win0_enabled || dispcnt_.win1_enabled || dispcnt_.win_obj_enabled;
    if(any_window_enabled) {
        generate_window_buffer();
    }

    if(vcount_ < screen_height) {
        dma_.request_dma(dma::occasion::hblank);
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
        dma_.request_dma(dma::occasion::video);
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

void engine::serialize(archive& archive) const noexcept
{
    archive.serialize(palette_ram_);
    archive.serialize(vram_);
    archive.serialize(oam_);

    archive.serialize(dispcnt_.read_lower());
    archive.serialize(dispcnt_.read_upper());
    archive.serialize(dispstat_.read_lower());
    archive.serialize(dispstat_.read_upper());
    archive.serialize(vcount_);

    archive.serialize(bg0_.cnt.read_lower());
    archive.serialize(bg0_.cnt.read_upper());
    archive.serialize(bg0_.hoffset);
    archive.serialize(bg0_.voffset);

    archive.serialize(bg1_.cnt.read_lower());
    archive.serialize(bg1_.cnt.read_upper());
    archive.serialize(bg1_.hoffset);
    archive.serialize(bg1_.voffset);

    archive.serialize(bg2_.cnt.read_lower());
    archive.serialize(bg2_.cnt.read_upper());
    archive.serialize(bg2_.hoffset);
    archive.serialize(bg2_.voffset);
    archive.serialize(bg2_.x_ref.ref);
    archive.serialize(bg2_.y_ref.ref);
    archive.serialize(bg2_.x_ref.internal);
    archive.serialize(bg2_.y_ref.internal);
    archive.serialize(bg2_.pa);
    archive.serialize(bg2_.pb);
    archive.serialize(bg2_.pc);
    archive.serialize(bg2_.pd);

    archive.serialize(bg3_.cnt.read_lower());
    archive.serialize(bg3_.cnt.read_upper());
    archive.serialize(bg3_.hoffset);
    archive.serialize(bg3_.voffset);
    archive.serialize(bg3_.x_ref.ref);
    archive.serialize(bg3_.y_ref.ref);
    archive.serialize(bg3_.x_ref.internal);
    archive.serialize(bg3_.y_ref.internal);
    archive.serialize(bg3_.pa);
    archive.serialize(bg3_.pb);
    archive.serialize(bg3_.pc);
    archive.serialize(bg3_.pd);

    archive.serialize(win0_.top_left.x);
    archive.serialize(win0_.top_left.y);
    archive.serialize(win0_.bottom_right.x);
    archive.serialize(win0_.bottom_right.y);
    archive.serialize(win1_.top_left.x);
    archive.serialize(win1_.top_left.y);
    archive.serialize(win1_.bottom_right.x);
    archive.serialize(win1_.bottom_right.y);

    archive.serialize(win_in_.win0.read());
    archive.serialize(win_in_.win1.read());
    archive.serialize(win_out_.obj.read());
    archive.serialize(win_out_.outside.read());
    archive.serialize(win_can_draw_flags_);

    archive.serialize(green_swap_);
    archive.serialize(mosaic_bg_.v);
    archive.serialize(mosaic_bg_.h);
    archive.serialize(mosaic_bg_.internal.v);
    archive.serialize(mosaic_bg_.internal.h);
    archive.serialize(mosaic_obj_.v);
    archive.serialize(mosaic_obj_.h);
    archive.serialize(mosaic_obj_.internal.v);
    archive.serialize(mosaic_obj_.internal.h);
    archive.serialize(bldcnt_.first.read());
    archive.serialize(bldcnt_.second.read());
    archive.serialize(bldcnt_.type);
    archive.serialize(blend_settings_.eva);
    archive.serialize(blend_settings_.evb);
    archive.serialize(blend_settings_.evy);

    archive.serialize(obj_buffer_);
    archive.serialize(final_buffer_);
    for(const scanline_buffer& buf : bg_buffers_) {
        archive.serialize(buf);
    }
}

void engine::deserialize(const archive& archive) noexcept
{
    archive.deserialize(palette_ram_);
    archive.deserialize(vram_);
    archive.deserialize(oam_);

    dispcnt_.write_lower(archive.deserialize<u8>());
    dispcnt_.write_upper(archive.deserialize<u8>());
    dispstat_.write_lower(archive.deserialize<u8>());
    dispstat_.write_upper(archive.deserialize<u8>());
    archive.deserialize(vcount_);

    bg0_.cnt.write_lower(archive.deserialize<u8>());
    bg0_.cnt.write_upper(archive.deserialize<u8>());
    archive.deserialize(bg0_.hoffset);
    archive.deserialize(bg0_.voffset);

    bg1_.cnt.write_lower(archive.deserialize<u8>());
    bg1_.cnt.write_upper(archive.deserialize<u8>());
    archive.deserialize(bg1_.hoffset);
    archive.deserialize(bg1_.voffset);

    bg2_.cnt.write_lower(archive.deserialize<u8>());
    bg2_.cnt.write_upper(archive.deserialize<u8>());
    archive.deserialize(bg2_.hoffset);
    archive.deserialize(bg2_.voffset);
    archive.deserialize(bg2_.x_ref.ref);
    archive.deserialize(bg2_.y_ref.ref);
    archive.deserialize(bg2_.x_ref.internal);
    archive.deserialize(bg2_.y_ref.internal);
    archive.deserialize(bg2_.pa);
    archive.deserialize(bg2_.pb);
    archive.deserialize(bg2_.pc);
    archive.deserialize(bg2_.pd);

    bg3_.cnt.write_lower(archive.deserialize<u8>());
    bg3_.cnt.write_upper(archive.deserialize<u8>());
    archive.deserialize(bg3_.hoffset);
    archive.deserialize(bg3_.voffset);
    archive.deserialize(bg3_.x_ref.ref);
    archive.deserialize(bg3_.y_ref.ref);
    archive.deserialize(bg3_.x_ref.internal);
    archive.deserialize(bg3_.y_ref.internal);
    archive.deserialize(bg3_.pa);
    archive.deserialize(bg3_.pb);
    archive.deserialize(bg3_.pc);
    archive.deserialize(bg3_.pd);

    archive.deserialize(win0_.top_left.x);
    archive.deserialize(win0_.top_left.y);
    archive.deserialize(win0_.bottom_right.x);
    archive.deserialize(win0_.bottom_right.y);
    archive.deserialize(win1_.top_left.x);
    archive.deserialize(win1_.top_left.y);
    archive.deserialize(win1_.bottom_right.x);
    archive.deserialize(win1_.bottom_right.y);

    win_in_.win0.write(archive.deserialize<u8>());
    win_in_.win1.write(archive.deserialize<u8>());
    win_out_.obj.write(archive.deserialize<u8>());
    win_out_.outside.write(archive.deserialize<u8>());
    archive.deserialize(win_can_draw_flags_);

    archive.deserialize(green_swap_);
    archive.deserialize(mosaic_bg_.v);
    archive.deserialize(mosaic_bg_.h);
    archive.deserialize(mosaic_bg_.internal.v);
    archive.deserialize(mosaic_bg_.internal.h);
    archive.deserialize(mosaic_obj_.v);
    archive.deserialize(mosaic_obj_.h);
    archive.deserialize(mosaic_obj_.internal.v);
    archive.deserialize(mosaic_obj_.internal.h);
    bldcnt_.first.write(archive.deserialize<u8>());
    bldcnt_.second.write(archive.deserialize<u8>());
    archive.deserialize(bldcnt_.type);
    archive.deserialize(blend_settings_.eva);
    archive.deserialize(blend_settings_.evb);
    archive.deserialize(blend_settings_.evy);

    archive.deserialize(obj_buffer_);
    archive.deserialize(final_buffer_);
    for(scanline_buffer& buf : bg_buffers_) {
        archive.deserialize(buf);
    }

    generate_window_buffer();
}

} // namespace gba::ppu
