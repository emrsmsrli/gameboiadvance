/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/ppu/ppu.h>

namespace gba::ppu {

void engine::render_obj() noexcept
{
    // stub
}

void engine::render_bg_affine_impl(bg_affine& bg) noexcept
{
    // stub
}

void engine::compose_impl(const static_vector<u32, 4>& ids) noexcept
{
    // stub
}

void engine::tile_line_8bpp(tile_line& out_line, const u16 y, const usize base_addr, const bg_map_entry entry) const noexcept
{
    for(u32 x : range(tile_dot_count)) {
        const u8 color_idx = memcpy<u8>(vram_, base_addr + entry.tile_idx() * 64_u32 + y * 8_u32 + x);
        out_line[x] = palette_color(color_idx, 0_u8);
    }
}

void engine::tile_line_4bpp(tile_line& out_line, const u16 y, const usize base_addr, const bg_map_entry entry) const noexcept
{
    for(u32 x = 0_u32; x < tile_dot_count; x += 2_u32) {
        const u8 color_idxs = memcpy<u8>(vram_, base_addr + entry.tile_idx() * 32_u32 + y * 4_u32 + x / 2_u32);
        const u8 palette_idx = entry.palette_idx();
        out_line[x] = palette_color(color_idxs & 0xF_u8, palette_idx);
        out_line[x + 1_u32] = palette_color(color_idxs >> 4_u8, palette_idx);
    }
}

} // namespace gba::ppu
