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
    if(!dispcnt_.obj_enabled) {
        return;
    }

    static constexpr obj_affine identity_affine;

    static constexpr range<u8> bitmap_modes{3_u8, 6_u8};
    const bool in_bitmap_mode = bitmap_modes.contains(dispcnt_.bg_mode);

    const view<obj_affine> affine_view{oam_};
    const u32 render_cycles_max = dispcnt_.hblank_interval_free
      ? 954_u32
      : 1210_u32;
    u32 render_cycles_spent = 0_u32;

    std::fill(obj_buffer_.begin(), obj_buffer_.end(), color::transparent());

    for(const obj& obj : view<obj>{oam_}) {
        const obj_attr0::rendering_mode render_mode = obj.attr0.render_mode();
        const obj_attr0::blend_mode blend_mode = obj.attr0.blending();
        const u32 shape_idx = obj.attr0.shape_idx();

        if(render_mode == obj_attr0::rendering_mode::hidden || blend_mode == obj_attr0::blend_mode::prohibited) {
            continue;
        }

        const bool is_affine = render_mode == obj_attr0::rendering_mode::affine
          || render_mode == obj_attr0::rendering_mode::affine_double;

        const dimension<u8> dimensions = obj.dimensions[shape_idx][obj.attr1.size_idx()];
        dimension<u8> half_dimensions{dimensions.h / 2_u8, dimensions.v / 2_u8};

        i32 y = obj.attr0.y();
        i32 x = obj.attr1.x();

        if(y >= make_signed(screen_height)) { y -= 256_i32; }
        if(x >= make_signed(screen_width)) { x -= 512_i32; }

        y += half_dimensions.v;
        x += half_dimensions.h;

        dimension<u32> flip_offsets{0_u32, 0_u32};
        obj_affine affine_matrix;
        u32 cycles_per_dot = 1_u32;

        if(is_affine) {
            affine_matrix = affine_view[obj.attr1.affine_idx()];
            cycles_per_dot = 2_u32;

            if(render_mode == obj_attr0::rendering_mode::affine_double) {
                y += half_dimensions.v;
                x += half_dimensions.h;
                half_dimensions = dimensions;
            }
        } else {
            static constexpr i16 p_flip = 0xFF00_i16;
            if(obj.attr1.h_flipped()) {
                flip_offsets.h = 1_u32;
                affine_matrix.pa = p_flip;
            }

            if(obj.attr1.v_flipped()) {
                flip_offsets.v = 1_u32;
                affine_matrix.pd = p_flip;
            }
        }

        if(!range(y - half_dimensions.v, y + half_dimensions.v).contains(vcount_)) {
            continue;
        }

        if(is_affine) {
            render_cycles_spent += 10_u32;
        }

        i32 local_y = make_signed(widen<u32>(vcount_)) - y;
        mosaic_obj_.internal.h = 0_u8;

        const bool mosaic_enabled = obj.attr0.mosaic_enabled();
        if(mosaic_enabled) {
            mosaic_obj_.internal.h = narrow<u8>((x - half_dimensions.h) % mosaic_obj_.h);
            local_y -= mosaic_obj_.internal.v;
        }

        for(i32 obj_local_x : range(-make_signed(half_dimensions.h), make_signed(half_dimensions.h))) {
            if(render_cycles_spent > render_cycles_max) {
                return;
            }

            const i32 local_x = obj_local_x - mosaic_obj_.internal.h;
            const i32 global_x = obj_local_x + x;

            render_cycles_spent += cycles_per_dot;

            if(!range(make_signed(screen_width)).contains(global_x)) {
                continue;
            }

            if(mosaic_enabled && (++mosaic_obj_.internal.h == mosaic_obj_.h)) {
                mosaic_obj_.internal.h = 0_u8;
            }

            // map screen coord -> texture coord
            const i32 tex_x = ((affine_matrix.pa * local_x + affine_matrix.pb * local_y) >> 8_i32) + (dimensions.h / 2_u8) - flip_offsets.h;
            const i32 tex_y = ((affine_matrix.pc * local_x + affine_matrix.pd * local_y) >> 8_i32) + (dimensions.v / 2_u8) - flip_offsets.v;

            if(!range<i32>(dimensions.h).contains(tex_x) || !range<i32>(dimensions.v).contains(tex_y)) {
                continue;
            }

            const u32 dot_x  = make_unsigned(tex_x) % tile_dot_count;
            const u32 dot_y  = make_unsigned(tex_y) % tile_dot_count;
            const u32 tile_x = make_unsigned(tex_x) / tile_dot_count;
            const u32 tile_y = make_unsigned(tex_y) / tile_dot_count;

            color dot;
            if(obj.attr0.color_depth_8bit()) {
                const u32 tile_idx = dispcnt_.obj_mapping_1d
                  ? obj.attr2.tile_idx() + tile_y * (dimensions.h / 4_u8) + tile_x * 2_u32
                  : bit::clear(obj.attr2.tile_idx(), 0_u8) + tile_y * 32_u32 + tile_x * 2_u32;

                if(in_bitmap_mode && tile_idx < 512_u32) {
                    continue;
                }

                dot = tile_dot_8bpp(dot_x, dot_y, 0x1'0000_u32 + tile_idx * 32_u32, palette_8bpp_target::obj);
            } else {
                const u32 tile_idx = dispcnt_.obj_mapping_1d
                  ? obj.attr2.tile_idx() + tile_y * (dimensions.h / 8_u8) + tile_x
                  : obj.attr2.tile_idx() + tile_y * 32_u32 + tile_x;

                if(in_bitmap_mode && tile_idx < 512_u32) {
                    continue;
                }

                dot = tile_dot_4bpp(dot_x, dot_y, 0x1'0000_u32 + tile_idx * 32_u32, obj.attr2.palette_idx());
            }

            obj_buffer_[make_unsigned(global_x)] = dot;
        }
    }
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
    const u8 color_idxs = memcpy<u8>(vram_, tile_addr + (y * tile_dot_count / 2_u32) + (x / 2_u32));
    return palette_color(
      bit::test(x, 0_u8)
        ? color_idxs >> 4_u8
        : color_idxs & 0xF_u8,
      palette_idx);
}

} // namespace gba::ppu
