/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_H
#define GAMEBOIADVANCE_PPU_H

#include <gba/arm/irq_controller_handle.h>
#include <gba/arm/dma_controller.h>
#include <gba/core/container.h>
#include <gba/core/event/event.h>
#include <gba/core/fwd.h>
#include <gba/ppu/types.h>
#include <gba/arm/mmio_addr.h>

namespace gba::ppu {

constexpr auto screen_width = 240_u32;
constexpr auto screen_height = 160_u32;

constexpr auto tile_dot_count = 8_u32;

using scanline_buffer = array<color, screen_width>;

class engine {
    friend arm::arm7tdmi;

    arm::irq_controller_handle irq_;
    dma::controller_handle dma_;
    scheduler* scheduler_;

    vector<u8> palette_ram_{1_kb};
    vector<u8> vram_{96_kb};
    vector<u8> oam_{1_kb};

    dispcnt dispcnt_;
    dispstat dispstat_;
    u8 vcount_;  // current scanline

    bg_regular bg0_{0_u32};
    bg_regular bg1_{1_u32};
    bg_affine bg2_{2_u32};
    bg_affine bg3_{3_u32};

    window win0_{0_u32};
    window win1_{1_u32};
    win_in win_in_;
    win_out win_out_;

    bool green_swap_ = false;
    mosaic mosaic_bg_;
    mosaic mosaic_obj_;
    bldcnt bldcnt_;
    blend_settings blend_settings_;

    array<scanline_buffer, 4> bg_buffers_;
    scanline_buffer obj_buffer_;
    scanline_buffer final_buffer_;

public:
    static constexpr auto cycles_per_frame = 280'896_u64;

    event<u8, const scanline_buffer&> event_on_scanline;
    event<> event_on_vblank;

    engine(scheduler* scheduler) noexcept;

    void set_irq_controller_handle(const arm::irq_controller_handle irq) noexcept { irq_ = irq; }
    void set_dma_controller_handle(const dma::controller_handle dma) noexcept { dma_ = dma; }

private:
    using tile_line = array<color, tile_dot_count>;

    void on_hblank(u64 cycles_late) noexcept;
    void on_hdraw(u64 cycles_late) noexcept;

    void render_scanline() noexcept;

    template<typename... BG>
    void render_bg_regular(BG&... bgs) noexcept;

    template<typename... BG>
    void render_bg_affine(BG&... bgs) noexcept;

    void render_obj() noexcept;

    template<typename... BG>
    void compose(BG&... bgs) noexcept;

    template<typename BG>
    void render_bg_regular_impl(BG& bg) noexcept;
    void render_bg_affine_impl(bg_affine& bg) noexcept;

    void compose_impl(const static_vector<u32, 4>& ids) noexcept;

    template<typename BG>
    [[nodiscard]] static FORCEINLINE u32 map_entry_index(const u32 tile_x, const u32 tile_y, const BG& bg) noexcept
    {
        u32 n = tile_x + tile_y * 32_u32;
        if(tile_x >= 0x20_u32) { n += 0x03E0_u32; }
        if(tile_y >= 0x20_u32 && bg.cnt.screen_size == 3_u8) { n += 0x0400_u32; }
        return n;
    }

    [[nodiscard]] FORCEINLINE color backdrop_color() const noexcept { return palette_color_opaque(0_u8); }
    [[nodiscard]] FORCEINLINE color palette_color(const u8 color_idx, const u8 palette_idx = 0_u8) const noexcept
    {
        if(color_idx == 0_u8) { return color::transparent(); }
        return palette_color_opaque(color_idx, palette_idx);
    }

    [[nodiscard]] FORCEINLINE color palette_color_opaque(const u8 color_idx, const u8 palette_idx = 0_u8) const noexcept
    {
        return color{memcpy<u16>(palette_ram_, (palette_idx * 16_u8 + color_idx) * 2_u16)};
    }

    void tile_line_8bpp(tile_line& out_line, u16 y, usize base_addr, bg_map_entry entry) const noexcept;
    void tile_line_4bpp(tile_line& out_line, u16 y, usize base_addr, bg_map_entry entry) const noexcept;
};

} // namespace gba::ppu

#include <gba/ppu/ppu_render.inl>

#endif //GAMEBOIADVANCE_PPU_H
