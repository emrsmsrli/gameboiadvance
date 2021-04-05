/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>

#include <gba/ppu/ppu.h>
#include <gba/helper/sort.h>

namespace gba::ppu {

namespace {

struct layer {
    enum class type { bg0, bg1, bg2, bg3, obj, bd };

    static inline u32 invalid_priority = 4_u32;

    type layer_type{type::bd};
    u32 priority = invalid_priority;
};

} // namespace

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

    std::fill(obj_buffer_.begin(), obj_buffer_.end(), obj_buffer_entry{});

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

            const bool is_transparent = dot == color::transparent();
            obj_buffer_entry& obj_entry = obj_buffer_[make_unsigned(global_x)];
            if(blend_mode == obj_attr0::blend_mode::obj_window) {
                if(dispcnt_.win_obj_enabled && !is_transparent){
                    win_buffer_[make_unsigned(global_x)] = &win_out_.obj;
                }
            } else if(const u32 priority = obj.attr2.priority(); priority < obj_entry.priority || obj_entry.dot == color::transparent()) {
                obj_entry.priority = priority;
                if(!is_transparent) {
                    obj_entry.dot = dot;
                    obj_entry.is_alpha_blending = blend_mode == obj_attr0::blend_mode::alpha_blending;
                }
            }
        }
    }
}

void engine::compose_impl(static_vector<bg_priority_pair, 4> ids) noexcept
{
    const color backdrop = backdrop_color();

    const auto dot_for_layer = [&](const layer& l, u32 x) {
        switch(l.layer_type) {
            case layer::type::bg0: case layer::type::bg1:
            case layer::type::bg2: case layer::type::bg3:
                return bg_buffers_[from_enum<u32>(l.layer_type)][x];
            case layer::type::obj:
                return obj_buffer_[x].dot;
            case layer::type::bd:
                return backdrop;
            default: UNREACHABLE();
        }
    };

    const auto is_blend_enabled = [&](bldcnt::target& target, const layer::type layer) {
        switch(layer) {
            case layer::type::bg0: case layer::type::bg1:
            case layer::type::bg2: case layer::type::bg3:
                return target.bg[from_enum<u32>(layer)];
            case layer::type::obj:
                return target.obj;
            case layer::type::bd:
                return target.backdrop;
            default: UNREACHABLE();
        }
    };

    // stable sort bg ids to render them in least importance order
    insertion_sort(ids);
    std::reverse(ids.begin(), ids.end());

    const bool any_window_enabled = dispcnt_.win0_enabled || dispcnt_.win1_enabled || dispcnt_.win_obj_enabled;
    if(any_window_enabled) {
        generate_window_buffer();
    }

    for(u32 x : range(screen_width)) {
        layer top_layer;
        layer bottom_layer;

        bool has_alpha_obj_dot = false;
        win_enable_bits* win_enable = win_buffer_[x];

        for(const auto& [priority, bg_id] : ids) {
            if(!any_window_enabled || win_enable->bg_enabled[bg_id]) {
                const color bg_dot = bg_buffers_[bg_id][x];
                if(bg_dot != color::transparent()) {
                    bottom_layer = std::exchange(top_layer, layer{to_enum<layer::type>(bg_id), priority});
                }
            }
        }

        if((!any_window_enabled || win_enable->obj_enabled) && dispcnt_.obj_enabled && obj_buffer_[x].dot != color::transparent()) {
            const u32 obj_prio = obj_buffer_[x].priority;
            if(obj_prio <= top_layer.priority) {
                bottom_layer.layer_type = std::exchange(top_layer.layer_type, layer::type::obj);
                has_alpha_obj_dot = obj_buffer_[x].is_alpha_blending;
            } else if(obj_prio <= bottom_layer.priority) {
                bottom_layer.layer_type = layer::type::obj;
            }
        }

        color top_dot = dot_for_layer(top_layer, x);
        if(!any_window_enabled || win_enable->blend_enabled || has_alpha_obj_dot) {
            const color bottom_dot = dot_for_layer(bottom_layer, x);
            const bool blend_dst_enabled = is_blend_enabled(bldcnt_.first, top_layer.layer_type);
            const bool blend_src_enabled = is_blend_enabled(bldcnt_.second, bottom_layer.layer_type);

            if(has_alpha_obj_dot && blend_src_enabled) {
                top_dot = blend(top_dot, bottom_dot, bldcnt::effect::alpha_blend);
            } else if(blend_dst_enabled && bldcnt_.type != bldcnt::effect::none && (blend_src_enabled || bldcnt_.type != bldcnt::effect::alpha_blend)) {
                top_dot = blend(top_dot, bottom_dot, bldcnt_.type);
            }
        }

        final_buffer_[x] = top_dot;
    }

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

color engine::blend(const color first, const color second, const bldcnt::effect type) noexcept
{
    constexpr u8 max_ev = 0x10_u8;
    constexpr u8 max_intensity = 0x1F_u8;

    switch(type) {
        case bldcnt::effect::none:
            return first;
        case bldcnt::effect::alpha_blend: {
            const u32 eva = std::min(max_ev, blend_settings_.eva);
            const u32 evb = std::min(max_ev, blend_settings_.evb);

            const color_unpacked first_unpacked = unpack(first);
            const color_unpacked second_unpacked = unpack(second);
            return pack(color_unpacked{
                std::min(max_intensity, narrow<u8>((first_unpacked.r * eva + second_unpacked.r * evb) >> 4_u8)),
                std::min(max_intensity, narrow<u8>((first_unpacked.g * eva + second_unpacked.g * evb) >> 4_u8)),
                std::min(max_intensity, narrow<u8>((first_unpacked.b * eva + second_unpacked.b * evb) >> 4_u8)),
            });
        }
        case bldcnt::effect::brightness_inc: {
            const u32 evy = std::min(max_ev, blend_settings_.evy);
            color_unpacked unpacked = unpack(first);

            unpacked.r += narrow<u8>(((max_intensity - unpacked.r) * evy) >> 4_u8);
            unpacked.g += narrow<u8>(((max_intensity - unpacked.g) * evy) >> 4_u8);
            unpacked.b += narrow<u8>(((max_intensity - unpacked.b) * evy) >> 4_u8);

            return pack(unpacked);
        }
        case bldcnt::effect::brightness_dec: {
            const u32 evy = std::min(max_ev, blend_settings_.evy);
            color_unpacked unpacked = unpack(first);

            unpacked.r -=  narrow<u8>((unpacked.r * evy) >> 4_u8);
            unpacked.g -=  narrow<u8>((unpacked.g * evy) >> 4_u8);
            unpacked.b -=  narrow<u8>((unpacked.b * evy) >> 4_u8);

            return pack(unpacked);
        }
        default:
            UNREACHABLE();
    }
}

} // namespace gba::ppu
