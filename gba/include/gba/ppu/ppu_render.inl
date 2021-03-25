/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_RENDER_INL
#define GAMEBOIADVANCE_PPU_RENDER_INL

#include <algorithm>
#include <map>

#include <gba/helper/range.h>

namespace gba::ppu {

template<typename... BG>
void engine::render_bg_regular(BG&... bgs) noexcept
{
    const auto render_if_enabled = [&](auto& bg) {
        if(dispcnt_.enable_bg[bg.id]) {
            render_bg_regular_impl(bg);
        }
    };

    (..., render_if_enabled(bgs));
}

template<typename... BG>
void engine::render_bg_affine(BG&... bgs) noexcept
{
    const auto render_if_enabled = [&](auto& bg) {
        if(dispcnt_.enable_bg[bg.id]) {
            render_bg_affine_impl(bg);
        }
    };

    (..., render_if_enabled(bgs));
}

template<typename... BG>
void engine::compose(BG&... bgs) noexcept
{
    static_vector<u32, 4> ids;
    const auto add_id_if_enabled = [&](auto&& bg) {
        if(dispcnt_.enable_bg[bg.id]) {
            ids.push_back(bg.id);
        }
    };

    (..., add_id_if_enabled(bgs));
    compose_impl(ids);
}

template<typename BG>
void engine::render_bg_regular_impl(BG& bg) noexcept
{
    static constexpr array bg_map_size_masks{
      dimension<u16>{0x0FF_u16, 0x0FF_u16}, // 32x32
      dimension<u16>{0x1FF_u16, 0x0FF_u16}, // 64x32
      dimension<u16>{0x0FF_u16, 0x1FF_u16}, // 32x64
      dimension<u16>{0x1FF_u16, 0x1FF_u16}, // 64x64
    };

    scanline_buffer& buffer = bg_buffers_[bg.id];
    const usize tile_base = bg.cnt.char_base_block * 16_kb;
    const usize map_entry_base = bg.cnt.screen_entry_base_block * 2_kb;
    const dimension<u16>& map_mask = bg_map_size_masks[bg.cnt.screen_size];

    const u16 map_y = (vcount_ + bg.voffset) & map_mask.v;
    const u8 tile_y = narrow<u8>(map_y / tile_dot_count);

    std::map<bg_map_entry, tile_line> tile_line_cache;
    tile_line current_tile_line;

    for(u16 screen_x = 0_u16; screen_x < screen_width;) {
        const u16 map_x = (screen_x + bg.hoffset) & map_mask.h;
        const u8 tile_x = narrow<u8>(map_x / tile_dot_count);
        const u32 block_index = map_entry_index(tile_x, tile_y, bg);

        const bg_map_entry entry{memcpy<u16>(vram_, map_entry_base + block_index * 2_u32)};
        if(const auto it = tile_line_cache.find(entry); it != tile_line_cache.end()) {
            current_tile_line = it->second;
        } else {
            const u16 dot_y = (map_y & 7_u16) ^ (7_u16 * bit::from_bool<u16>(entry.vflipped()));
            if(bg.cnt.color_depth_8bit) {
                tile_line_8bpp(current_tile_line, dot_y, tile_base, entry);
            } else {
                tile_line_4bpp(current_tile_line, dot_y, tile_base, entry);
            }

            if(entry.hflipped()) {
                std::reverse(current_tile_line.begin(), current_tile_line.end());
            }

            tile_line_cache.emplace(entry, current_tile_line);
        }

        const u32 start_x = screen_x == 0_u32
          ? (map_x & 7_u16)
          : 0_u16;
        const u32 end_x = screen_x + tile_dot_count > screen_width
          ? screen_width - screen_x
          : tile_dot_count;
        for(u32 tile_dot_x : range(start_x, end_x)) {
            buffer[screen_x++] = current_tile_line[tile_dot_x];
        }
    }
}

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_PPU_RENDER_INL
