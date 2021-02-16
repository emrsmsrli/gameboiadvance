/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_TYPES_H
#define GAMEBOIADVANCE_TYPES_H

#include <type_traits>

#include <gba/core/math.h>

namespace gba::ppu {

struct color {
    u8 r;
    u8 g;
    u8 b;
    u8 a;

    [[nodiscard]] u32 to_u32() const noexcept
    {
        return widen<u32>(r) << 24_u32 | widen<u32>(g) << 16_u32
             | widen<u32>(b) << 8_u32  | widen<u32>(a);
    }
};

struct dispcnt {
    u8 bg_mode;
    u8 frame_select;
    bool hblank_interval_free = false;
    bool obj_mapping_1d = false;
    bool forced_blank = false;
    array<bool, 4> enable_bg;
    bool enable_obj = false;
    bool enable_w0 = false;
    bool enable_w1 = false;
    bool enable_wobj = false; // window - different from enable_obj
};

struct dispstat {
    bool vblank = false;
    bool hblank = false;
    bool vcounter = false;
    bool vblank_irq = false;
    bool hblank_irq = false;
    bool vcounter_irq = false;
    u8 vcount_setting;
};

/*****************/

struct bgcnt_affine_base { bool display_area_overflow = false; };
struct bg_regular_base {};
struct bg_affine_base {
    struct reference_point {
        u32 ref;
        u32 internal;

        FORCEINLINE void set_byte(const u8 n, const u8 data) noexcept
        {
            ref = make_unsigned(math::sign_extend<28>(bit::set_byte(ref, n, data) & 0x0FFF'FFFF_u32));
            internal = ref;
        }
    };

    reference_point x_ref;
    reference_point y_ref;

    u16 pa;
    u16 pb;
    u16 pc;
    u16 pd;
};

namespace traits {

template<typename T> struct is_bg_type { static constexpr bool value = false; };
template<> struct is_bg_type<bg_regular_base> { static constexpr bool value = true; };
template<> struct is_bg_type<bg_affine_base> { static constexpr bool value = true; };
template<> struct is_bg_type<bgcnt_affine_base> { static constexpr bool value = true; };

template<typename T> struct is_affine_bg { static constexpr bool value = false; };
template<> struct is_affine_bg<bg_affine_base> { static constexpr bool value = true; };

} // namespace traits

template<typename T>
struct bgcnt : T {
    static_assert(traits::is_bg_type<T>::value);

    u8 priority;
    u8 char_base_block;
    bool mosaic_enabled = false;
    bool color_depth_8bit = false;
    u8 screen_size;

    void write_lower(const u8 data) noexcept
    {
        priority = data & 0b11_u8;
        char_base_block = (data >> 2_u8) & 0b11_u8;
        mosaic_enabled = bit::test(data, 6_u8);
        color_depth_8bit = bit::test(data, 7_u8);
    }

    void write_upper(const u8 data) noexcept
    {
        char_base_block = data & 0x1F_u8;
        screen_size = (data >> 6_u8) & 0b11_u8;
        if constexpr(std::is_same_v<T, ppu::bgcnt_affine_base>) {
            T::display_area_overflow = bit::test(data, 5_u8);
        }
    }

    [[nodiscard]] u8 read_lower() const noexcept
    {
        return priority
          | char_base_block << 2_u8
          | bit::from_bool<u8>(mosaic_enabled) << 6_u8
          | bit::from_bool<u8>(color_depth_8bit) << 7_u8;
    }

    [[nodiscard]] u8 read_upper() const noexcept
    {
        u8 data = char_base_block | screen_size << 6_u8;
        if constexpr(std::is_same_v<T, ppu::bgcnt_affine_base>) {
            data |= bit::from_bool<u8>(T::display_area_overflow) << 5_u8;
        }
        return data;
    }
};

template<typename T>
struct bg : T {
    static_assert(traits::is_bg_type<T>::value);

    u32 id;
    bgcnt<std::conditional_t<traits::is_affine_bg<T>::value, bgcnt_affine_base, bg_regular_base>> cnt;
    u16 hoffset;
    u16 voffset;

    explicit bg(const u32 i) : id{i} {}
};

using bg_regular = bg<bg_regular_base>;
using bg_affine = bg<bg_affine_base>;

/*****************/

struct window {
    u32 id;
    u8 dim_x1;
    u8 dim_x2;
    u8 dim_y1;
    u8 dim_y2;

    explicit window(u32 i) : id{i} {}
};

struct win_enable_bits {
    array<bool, 4> bg_enable;
    bool obj_enable = false;
    bool special_effect = false;
};

struct win_in {
    win_enable_bits win0;
    win_enable_bits win1;
};

struct win_out {
    win_enable_bits outside;
    win_enable_bits obj;
};

/*****************/

struct mosaic {
    struct size { u8 h; u8 v; };

    size bg;
    size obj;
};

/*****************/

struct bldcnt {
    enum class effect : u8::type { none, alpha_blend, brightness_inc, brightness_dec };

    struct target {
        array<bool, 4> bg;
        bool obj;
        bool backdrop;
    };

    target first;
    target second;
    effect effect_type;
};

struct blend_settings {
    u8 eva;
    u8 evb;
    u8 evy; // BLDY
};

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_TYPES_H
