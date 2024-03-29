/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_H
#define GAMEBOIADVANCE_PPU_H

#include <gba/cpu/dma_controller.h>
#include <gba/cpu/irq_controller_handle.h>
#include <gba/cpu/mmio_addr.h>
#include <gba/core/container.h>
#include <gba/core/event/event.h>
#include <gba/core/fwd.h>
#include <gba/ppu/ppu_types.h>

namespace gba::ppu {

constexpr u32::type screen_width = 240_u32;
constexpr u32::type screen_height = 160_u32;

constexpr u32::type tile_dot_count = 8_u32;

using scanline_buffer = array<color, screen_width>;

class engine {
    friend core;

    cpu::irq_controller_handle irq_;
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
    array<bool, 2> win_can_draw_flags_{false, false};

    bool green_swap_ = false;
    mosaic mosaic_bg_;
    mosaic mosaic_obj_;
    bldcnt bldcnt_;
    blend_settings blend_settings_;

    scanline_buffer final_buffer_;
    array<scanline_buffer, 4> bg_buffers_;
    array<obj_buffer_entry, screen_width> obj_buffer_;
    array<win_enable_bits*, screen_width> win_buffer_;

public:
    static constexpr u32 cycles_per_frame = 280'896_u32;

    event<u8, const scanline_buffer&> event_on_scanline;
    event<> event_on_vblank;

    explicit engine(scheduler* scheduler) noexcept;

    void set_irq_controller_handle(const cpu::irq_controller_handle irq) noexcept { irq_ = irq; }
    void set_dma_controller_handle(const dma::controller_handle dma) noexcept { dma_ = dma; }

    void check_vcounter_irq() noexcept;

    void serialize(archive& archive) const noexcept;
    void deserialize(const archive& archive) noexcept;

private:
    enum class palette_8bpp_target { bg = 0_u32, obj = 16_u32 };

    using tile_line = array<color, tile_dot_count>;

    void on_hblank(u32 late_cycles) noexcept;
    void on_hdraw(u32 late_cycles) noexcept;

    void render_scanline() noexcept;

    template<typename... BG>
    void render_bg_regular(const BG&... bgs) noexcept;

    template<typename... BG>
    void render_bg_affine(const BG&... bgs) noexcept;

    template<typename F>
    void render_affine_loop(const bg_affine& bg, i32 w, i32 h, F&& render_func) noexcept;

    void render_obj() noexcept;

    template<typename... BG>
    void compose(const BG&... bgs) noexcept;

    template<typename BG>
    void render_bg_regular_impl(const BG& bg) noexcept;

    void generate_window_buffer() noexcept;
    void generate_window_buffer(window& win, win_enable_bits* enable_bits) noexcept;
    void compose_impl(static_vector<bg_priority_pair, 4> ids) noexcept;
    [[nodiscard]] color blend(color first, color second, bldcnt::effect type) const noexcept;

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
        return color{memcpy<u16>(palette_ram_, (palette_idx * 16_u32 + color_idx) * 2_u16) & 0x7FFF_u16};
    }

    void tile_line_8bpp(tile_line& out_line, u32 y, usize base_addr, bg_map_entry entry) const noexcept;
    void tile_line_4bpp(tile_line& out_line, u32 y, usize base_addr, bg_map_entry entry) const noexcept;

    [[nodiscard]] color tile_dot_8bpp(u32 x, u32 y, usize tile_addr, palette_8bpp_target target) const noexcept;
    [[nodiscard]] color tile_dot_4bpp(u32 x, u32 y, usize tile_addr, u8 palette_idx) const noexcept;
};

} // namespace gba::ppu

#include <gba/ppu/ppu_render.inl>

#endif //GAMEBOIADVANCE_PPU_H
