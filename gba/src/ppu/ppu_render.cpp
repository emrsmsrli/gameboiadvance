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

void engine::compose_impl(const static_vector<u32, 4>& ids) noexcept
{
    // stub

    if(UNLIKELY(green_swap_)) {
        for(u32 x = 0_u32; x < screen_width; x += 2_u32) {
            final_buffer_[x].swap_green(final_buffer_[x + 1_u32]);
        }
    }
}

void engine::tile_line_8bpp(tile_line& out_line, const u32 y, const usize base_addr, const bg_map_entry entry) const noexcept
{
    constexpr u32 total_tile_size = tile_dot_count * tile_dot_count;
    for(u32 x : range(tile_dot_count)) {
        const u8 color_idx = memcpy<u8>(vram_, base_addr + entry.tile_idx() * total_tile_size + y * tile_dot_count + x);
        out_line[x] = palette_color(color_idx, 0_u8);
    }
}

void engine::tile_line_4bpp(tile_line& out_line, const u32 y, const usize base_addr, const bg_map_entry entry) const noexcept
{
    constexpr u32 total_tile_size = tile_dot_count * tile_dot_count / 2_u32;
    for(u32 x = 0_u32; x < tile_dot_count; x += 2_u32) {
        const u8 color_idxs = memcpy<u8>(vram_, base_addr + entry.tile_idx() * total_tile_size + y * 4_u32 + x / 2_u32);
        const u8 palette_idx = entry.palette_idx();
        out_line[x] = palette_color(color_idxs & 0xF_u8, palette_idx);
        out_line[x + 1_u32] = palette_color(color_idxs >> 4_u8, palette_idx);
    }
}

color engine::tile_dot_8bpp(const u32 x, const u32 y, const usize tile_addr, const palette_8bpp_target target) const noexcept
{
    const u8 color_idx = memcpy<u8>(vram_, tile_addr + y * tile_dot_count + x);
    return palette_color(color_idx, from_enum<u8>(target));
}

color engine::tile_dot_4bpp(const u32 x, const u32 y, const usize tile_addr, const u8 palette_idx) const noexcept
{
    const usize offset = tile_addr + (y * tile_dot_count / 2_u32) + (x / 2_u32);
    const u8 color_idxs = memcpy<u8>(vram_, offset);
    return palette_color(
      bit::test(color_idxs, 0_u8)
        ? color_idxs & 0xF_u8
        : color_idxs >> 4_u8,
      palette_idx);
}

} // namespace gba::ppu
