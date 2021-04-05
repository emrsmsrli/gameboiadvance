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
void engine::render_bg_regular(const BG&... bgs) noexcept
{
    const auto render_if_enabled = [&](auto& bg) {
        if(dispcnt_.bg_enabled[bg.id]) {
            render_bg_regular_impl(bg);
        }
    };

    (..., render_if_enabled(bgs));
}

template<typename... BG>
void engine::render_bg_affine(const BG&... bgs) noexcept
{
    const auto render_if_enabled = [&](auto& bg) {
        if(dispcnt_.bg_enabled[bg.id]) {
            scanline_buffer& buffer = bg_buffers_[bg.id];
            const usize tile_base = bg.cnt.char_base_block * 16_kb;
            const usize map_entry_base = bg.cnt.screen_entry_base_block * 2_kb;

            const u32 map_block_size = 16_u32 * (1_u32 << bg.cnt.screen_size);
            const u32 map_dot_count = map_block_size * tile_dot_count;

            render_affine_loop(bg, make_signed(map_dot_count), make_signed(map_dot_count),
              [&](const u32 screen_x, const u32 x, const u32 y) {
                  const u8 tile_idx = memcpy<u8>(vram_,
                    map_entry_base + (y / tile_dot_count) * map_block_size + (x / tile_dot_count));
                  buffer[screen_x] = tile_dot_8bpp(x % tile_dot_count, y % tile_dot_count,
                    tile_base + tile_idx * tile_dot_count * tile_dot_count, palette_8bpp_target::bg);
              });
        }
    };

    (..., render_if_enabled(bgs));
}

template<typename F>
void engine::render_affine_loop(const bg_affine& bg, const i32 w, const i32 h, F&& render_func) noexcept
{
    scanline_buffer& buffer = bg_buffers_[bg.id];
    i32 ref_y = make_signed(bg.y_ref.internal);
    i32 ref_x = make_signed(bg.x_ref.internal);
    const i32 pa = make_signed(bg.pa);
    const i32 pc = make_signed(bg.pc);

    mosaic_bg_.internal.h = 0_u8;
    for(u32 screen_x : range(screen_width)) {
        i32 x = ref_x >> 8_i32;
        i32 y = ref_y >> 8_i32;

        if(bg.cnt.mosaic_enabled) {
            if(++mosaic_bg_.internal.h == mosaic_bg_.h) {
                ref_x += pa * make_signed(mosaic_bg_.h);
                ref_y += pc * make_signed(mosaic_bg_.h);
                mosaic_bg_.internal.h = 0_u8;
            }
        } else {
            ref_x += pa;
            ref_y += pc;
        }

        if(bg.cnt.wraparound) {
            if (x >= w) {
                x %= w;
            } else if (x < 0_i32) {
                x = w + (x % w);
            }

            if (y >= h) {
                y %= h;
            } else if (y < 0_i32) {
                y = h + (y % h);
            }
        } else if(!range(w).contains(x) || !range(h).contains(y)) {
            buffer[screen_x] = color::transparent();
            continue;
        }

        render_func(screen_x, make_unsigned(x), make_unsigned(y));
    }
}

template<typename... BG>
void engine::compose(const BG&... bgs) noexcept
{
    static_vector<bg_priority_pair, 4> ids;
    const auto add_id_if_enabled = [&](auto&& bg) {
        if(dispcnt_.bg_enabled[bg.id]) {
            ids.push_back({bg.cnt.priority, bg.id});
        }
    };

    (..., add_id_if_enabled(bgs));
    compose_impl(ids);
}

template<typename BG>
void engine::render_bg_regular_impl(const BG& bg) noexcept
{
    static constexpr array<dimension<u16>, 4> bg_map_size_masks{{
      dimension<u16>{0x0FF_u16, 0x0FF_u16}, // 32x32
      dimension<u16>{0x1FF_u16, 0x0FF_u16}, // 64x32
      dimension<u16>{0x0FF_u16, 0x1FF_u16}, // 32x64
      dimension<u16>{0x1FF_u16, 0x1FF_u16}, // 64x64
    }};

    scanline_buffer& buffer = bg_buffers_[bg.id];
    const usize tile_base = bg.cnt.char_base_block * 16_kb;
    const usize map_entry_base = bg.cnt.screen_entry_base_block * 2_kb;
    const dimension<u16>& map_mask = bg_map_size_masks[bg.cnt.screen_size];

    const u8 mosaic_v_offset = bg.cnt.mosaic_enabled
      ? mosaic_bg_.internal.v
      : 0_u8;
    const u16 map_y = (vcount_ + bg.voffset - mosaic_v_offset) & map_mask.v;
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

    if(bg.cnt.mosaic_enabled && mosaic_bg_.h != 1_u32) {
        mosaic_bg_.internal.h = 0_u8;
        for(u32 screen_x : range(screen_width)) {
            buffer[screen_x] = buffer[screen_x - mosaic_bg_.internal.h];
            if (++mosaic_bg_.internal.h == mosaic_bg_.h) {
                mosaic_bg_.internal.h = 0_u8;
            }
        }
    }
}

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_PPU_RENDER_INL
