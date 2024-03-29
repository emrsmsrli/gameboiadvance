/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_TYPES_H
#define GAMEBOIADVANCE_PPU_TYPES_H

#include <type_traits>

#include <gba/core/math.h>

namespace gba::ppu {

struct coord { u8 x; u8 y; };

template<typename T>
struct dimension { T h; T v; };

struct color_unpacked {
    u8 r;
    u8 g;
    u8 b;
};

struct color {
    static constexpr u16 r_mask = 0x1F_u16;
    static constexpr u16 g_mask = r_mask << 5_u8;
    static constexpr u16 b_mask = g_mask << 5_u16;

    u16 val;

    [[nodiscard]] FORCEINLINE constexpr u32 to_u32() const noexcept
    {
        const u32 color = val;
        const u32 r = (color & r_mask) << 27_u32;
        const u32 g = (color & g_mask) << 14_u32;
        const u32 b = (color & b_mask) << 1_u32;
        return r | g | b | 0xFF_u32;
    }

    constexpr void swap_green(color& other) noexcept
    {
        const u16 this_green = val & g_mask;
        val = mask::set(mask::clear(val, g_mask), other.val & g_mask);
        other.val = mask::set(mask::clear(other.val, g_mask), this_green);
    }

    bool operator==(const color other) const noexcept { return val == other.val; }
    bool operator!=(const color other) const noexcept { return val != other.val; }

    FORCEINLINE static constexpr color white() noexcept { return color{0x7FFF_u16}; }
    FORCEINLINE static constexpr color transparent() noexcept { return color{0x8000_u16}; }

    template<typename Ar>
    void serialize(Ar& archive) const noexcept { archive.serialize(val); }
    template<typename Ar>
    void deserialize(const Ar& archive) noexcept { archive.deserialize(val); }
};

FORCEINLINE constexpr color_unpacked unpack(const color packed) noexcept
{
    return color_unpacked{
      narrow<u8>(packed.val & color::r_mask),
      narrow<u8>((packed.val & color::g_mask) >> 5_u16),
      narrow<u8>((packed.val & color::b_mask) >> 10_u8)
    };
}

FORCEINLINE constexpr color pack(const color_unpacked unpacked) noexcept
{
    return color{widen<u16>(unpacked.r) | widen<u16>(unpacked.g) << 5_u16 | widen<u16>(unpacked.b) << 10_u16};
}

struct dispcnt {
    u8 bg_mode;
    u8 frame_select;
    bool hblank_interval_free = false;
    bool obj_mapping_1d = false;
    bool forced_blank = false;
    array<bool, 4> bg_enabled{false, false, false, false};
    bool obj_enabled = false;
    bool win0_enabled = false;
    bool win1_enabled = false;
    bool win_obj_enabled = false;

    [[nodiscard]] FORCEINLINE u8 read_lower() const noexcept
    {
        return bit::from_bool<u8>(forced_blank) << 7_u8
          | bit::from_bool<u8>(obj_mapping_1d) << 6_u8
          | bit::from_bool<u8>(hblank_interval_free) << 5_u8
          | frame_select << 4_u8
          | bg_mode;
    }

    [[nodiscard]] FORCEINLINE u8 read_upper() const noexcept
    {
        return bit::from_bool<u8>(bg_enabled[0_usize])
          | bit::from_bool<u8>(bg_enabled[1_usize]) << 1_u8
          | bit::from_bool<u8>(bg_enabled[2_usize]) << 2_u8
          | bit::from_bool<u8>(bg_enabled[3_usize]) << 3_u8
          | bit::from_bool<u8>(obj_enabled) << 4_u8
          | bit::from_bool<u8>(win0_enabled) << 5_u8
          | bit::from_bool<u8>(win1_enabled) << 6_u8
          | bit::from_bool<u8>(win_obj_enabled) << 7_u8;
    }

    FORCEINLINE void write_lower(const u8 data) noexcept
    {
        bg_mode = data & 0b111_u8;
        frame_select = bit::extract(data, 4_u8);
        hblank_interval_free = bit::test(data, 5_u8);
        obj_mapping_1d = bit::test(data, 6_u8);
        forced_blank = bit::test(data, 7_u8);
    }

    FORCEINLINE void write_upper(const u8 data) noexcept
    {
        obj_enabled = bit::test(data, 4_u8);
        win0_enabled = bit::test(data, 5_u8);
        win1_enabled = bit::test(data, 6_u8);
        win_obj_enabled = bit::test(data, 7_u8);
        for(u8 bg = 0_u8; bg < bg_enabled.size(); ++bg) {
            bg_enabled[bg] = bit::test(data, bg);
        }
    }
};

struct dispstat {
    bool vblank = false;
    bool hblank = false;
    bool vcounter = false;
    bool vblank_irq_enabled = false;
    bool hblank_irq_enabled = false;
    bool vcounter_irq_enabled = false;
    u8 vcount_setting;

    [[nodiscard]] FORCEINLINE u8 read_lower() const noexcept
    {
        return bit::from_bool<u8>(vblank)
          | bit::from_bool<u8>(hblank) << 1_u8
          | bit::from_bool<u8>(vcounter) << 2_u8
          | bit::from_bool<u8>(vblank_irq_enabled) << 3_u8
          | bit::from_bool<u8>(hblank_irq_enabled) << 4_u8
          | bit::from_bool<u8>(vcounter_irq_enabled) << 5_u8;
    }

    [[nodiscard]] FORCEINLINE u8 read_upper() const noexcept
    {
        return vcount_setting;
    }

    FORCEINLINE void write_lower(const u8 data) noexcept
    {
        vblank_irq_enabled = bit::test(data, 3_u8);
        hblank_irq_enabled = bit::test(data, 4_u8);
        vcounter_irq_enabled = bit::test(data, 5_u8);
    }

    FORCEINLINE void write_upper(const u8 data) noexcept { vcount_setting = data; }
};

/*****************/

struct bgcnt_affine_base { bool wraparound = false; };
struct bg_regular_base {};
struct bg_affine_base {
    struct reference_point {
        u32 ref;
        u32 internal;

        FORCEINLINE void latch() noexcept { internal = ref; }

        template<u8::type N>
        FORCEINLINE void set_byte(const u8 data) noexcept
        {
            static_assert(N < 4_u8);

            ref = bit::set_byte(ref, N, data);
            if constexpr(N == 3_u8) {
                ref = make_unsigned(math::sign_extend<28>(ref & 0x0FFF'FFFF_u32));
            }

            latch();
        }
    };

    reference_point x_ref;
    reference_point y_ref;

    u16 pa = 0x0100_u16;
    u16 pb;
    u16 pc;
    u16 pd = 0x0100_u16;
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
    u8 _unused_2;
    bool mosaic_enabled = false;
    bool color_depth_8bit = false;
    u8 screen_entry_base_block;
    u8 screen_size;

    [[nodiscard]] FORCEINLINE u8 read_lower() const noexcept
    {
        return priority | char_base_block << 2_u8 | _unused_2 << 4_u8
          | bit::from_bool<u8>(mosaic_enabled) << 6_u8
          | bit::from_bool<u8>(color_depth_8bit) << 7_u8;
    }

    [[nodiscard]] FORCEINLINE u8 read_upper() const noexcept
    {
        u8 data = screen_entry_base_block | screen_size << 6_u8;
        if constexpr(std::is_same_v<T, ppu::bgcnt_affine_base>) {
            data |= bit::from_bool<u8>(T::wraparound) << 5_u8;
        }
        return data;
    }

    FORCEINLINE void write_lower(const u8 data) noexcept
    {
        priority = data & 0b11_u8;
        char_base_block = (data >> 2_u8) & 0b11_u8;
        _unused_2 = (data >> 4_u8) & 0b11_u8;
        mosaic_enabled = bit::test(data, 6_u8);
        color_depth_8bit = bit::test(data, 7_u8);
    }

    FORCEINLINE void write_upper(const u8 data) noexcept
    {
        screen_entry_base_block = data & 0x1F_u8;
        screen_size = data >> 6_u8;
        if constexpr(std::is_same_v<T, ppu::bgcnt_affine_base>) {
            T::wraparound = bit::test(data, 5_u8);
        }
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
    coord top_left;
    coord bottom_right;

    explicit window(const u32 i) : id{i} {}
};

struct win_enable_bits {
    array<bool, 4> bg_enabled{false, false, false, false};
    bool obj_enabled = false;
    bool blend_enabled = false;

    [[nodiscard]] FORCEINLINE u8 read() const noexcept
    {
        return bit::from_bool<u8>(bg_enabled[0_usize])
          | bit::from_bool<u8>(bg_enabled[1_usize]) << 1_u8
          | bit::from_bool<u8>(bg_enabled[2_usize]) << 2_u8
          | bit::from_bool<u8>(bg_enabled[3_usize]) << 3_u8
          | bit::from_bool<u8>(obj_enabled) << 4_u8
          | bit::from_bool<u8>(blend_enabled) << 5_u8;
    }

    FORCEINLINE void write(const u8 data) noexcept
    {
        bg_enabled[0_usize] = bit::test(data, 0_u8);
        bg_enabled[1_usize] = bit::test(data, 1_u8);
        bg_enabled[2_usize] = bit::test(data, 2_u8);
        bg_enabled[3_usize] = bit::test(data, 3_u8);
        obj_enabled = bit::test(data, 4_u8);
        blend_enabled = bit::test(data, 5_u8);
    }
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

struct mosaic : dimension<u8> {
    dimension<u8> internal;

    mosaic() noexcept : dimension{1_u8, 1_u8} {}

    FORCEINLINE void reset() noexcept { internal = dimension{0_u8, 0_u8}; }
    FORCEINLINE void update_internal_v() noexcept
    {
        if(++internal.v == v) {
            internal.v = 0_u8;
        }
    }
};

/*****************/

struct bldcnt {
    enum class effect : u8::type { none, alpha_blend, brightness_inc, brightness_dec };

    struct target {
        array<bool, 4> bg{false, false, false, false};
        bool obj = false;
        bool backdrop = false;

        [[nodiscard]] FORCEINLINE u8 read() const noexcept
        {
            return bit::from_bool<u8>(bg[0_usize])
              | bit::from_bool<u8>(bg[1_usize]) << 1_u8
              | bit::from_bool<u8>(bg[2_usize]) << 2_u8
              | bit::from_bool<u8>(bg[3_usize]) << 3_u8
              | bit::from_bool<u8>(obj) << 4_u8
              | bit::from_bool<u8>(backdrop) << 5_u8;
        }

        FORCEINLINE void write(const u8 data) noexcept
        {
            bg[0_usize] = bit::test(data, 0_u8);
            bg[1_usize] = bit::test(data, 1_u8);
            bg[2_usize] = bit::test(data, 2_u8);
            bg[3_usize] = bit::test(data, 3_u8);
            obj = bit::test(data, 4_u8);
            backdrop = bit::test(data, 5_u8);
        }
    };

    target first;
    target second;
    effect type{effect::none};
};

/* 1.4 fixed point */
struct blend_settings {
    u8 eva;
    u8 evb;
    u8 evy; // BLDY
};

/*****************/

struct bg_map_entry {
    u16 value;

    [[nodiscard]] FORCEINLINE u16 tile_idx() const noexcept { return value & 0x3FF_u16; }
    [[nodiscard]] FORCEINLINE bool hflipped() const noexcept { return bit::test(value, 10_u8); }
    [[nodiscard]] FORCEINLINE bool vflipped() const noexcept { return bit::test(value, 11_u8); }
    [[nodiscard]] FORCEINLINE u8 palette_idx() const noexcept { return narrow<u8>(value >> 12_u16); }

    [[nodiscard]] bool operator==(const bg_map_entry other) const noexcept { return value == other.value; }
    [[nodiscard]] bool operator<(const bg_map_entry other) const noexcept { return value < other.value; }
};

struct obj_attr0 {
    enum class blend_mode { normal, alpha_blending, obj_window, prohibited };
    enum class rendering_mode { normal, affine, hidden, affine_double };

    u16 value;

    [[nodiscard]] FORCEINLINE u8 y() const noexcept { return narrow<u8>(value); }
    [[nodiscard]] FORCEINLINE rendering_mode render_mode() const noexcept { return to_enum<rendering_mode>((value >> 8_u16) & 0b11_u16); }
    [[nodiscard]] FORCEINLINE blend_mode blending() const noexcept { return to_enum<blend_mode>((value >> 10_u16) & 0b11_u16); }
    [[nodiscard]] FORCEINLINE bool mosaic_enabled() const noexcept { return bit::test(value, 12_u8); }
    [[nodiscard]] FORCEINLINE bool color_depth_8bit() const noexcept { return bit::test(value, 13_u8); }
    [[nodiscard]] FORCEINLINE u32 shape_idx() const noexcept { return value >> 14_u8; }
};

struct obj_attr1 {
    u16 value;

    [[nodiscard]] FORCEINLINE u16 x() const noexcept { return value & 0x1FF_u16; }
    [[nodiscard]] FORCEINLINE u32 affine_idx() const noexcept { return (value >> 9_u16) & 0x1F_u16; }
    [[nodiscard]] FORCEINLINE u32 size_idx() const noexcept { return value >> 14_u8; }
    [[nodiscard]] FORCEINLINE bool h_flipped() const noexcept { return bit::test(value, 12_u8); }
    [[nodiscard]] FORCEINLINE bool v_flipped() const noexcept { return bit::test(value, 13_u8); }
};

struct obj_attr2 {
    u16 value;

    [[nodiscard]] FORCEINLINE u16 tile_idx() const noexcept { return value & 0x3FF_u16; }
    [[nodiscard]] FORCEINLINE u32 priority() const noexcept { return (value >> 10_u16) & 0b11_u16; }
    [[nodiscard]] FORCEINLINE u8 palette_idx() const noexcept { return narrow<u8>(value >> 12_u16) + 16_u8; }
};

struct obj {
private:
    using dimen_t = dimension<u8>;
    using shape_table_t = array<dimen_t, 4>;
    using dimen_table_t = array<shape_table_t, 3>;

public:
    static constexpr dimen_table_t dimensions{
      shape_table_t{dimen_t{8_u8, 8_u8}, dimen_t{16_u8, 16_u8}, dimen_t{32_u8, 32_u8}, dimen_t{64_u8, 64_u8}},
      shape_table_t{dimen_t{16_u8, 8_u8}, dimen_t{32_u8, 8_u8}, dimen_t{32_u8, 16_u8}, dimen_t{64_u8, 32_u8}},
      shape_table_t{dimen_t{8_u8, 16_u8}, dimen_t{8_u8, 32_u8}, dimen_t{16_u8, 32_u8}, dimen_t{32_u8, 64_u8}}
    };

    obj_attr0 attr0;
    obj_attr1 attr1;
    obj_attr2 attr2;
    [[maybe_unused]] u16 _fill;
};

struct obj_affine {
private:
    using fill_t = array<u16, 3>;

public:
    [[maybe_unused]] fill_t _fill0;
    i16 pa = 0x0100_i16;
    [[maybe_unused]] fill_t _fill1;
    i16 pb;
    [[maybe_unused]] fill_t _fill2;
    i16 pc;
    [[maybe_unused]] fill_t _fill3;
    i16 pd = 0x0100_i16;
};

/*****************/

struct bg_priority_pair {
    u32 priority;
    u32 idx;

    bool operator<(const bg_priority_pair& other) const noexcept { return priority < other.priority; }
};

struct obj_buffer_entry {
    u32 priority = 4_u32;
    color dot = color::transparent();
    bool is_alpha_blending = false;

    template<typename Ar>
    void serialize(Ar& archive) const noexcept
    {
        archive.serialize(priority);
        archive.serialize(dot);
        archive.serialize(is_alpha_blending);
    }

    template<typename Ar>
    void deserialize(const Ar& archive) noexcept
    {
        archive.deserialize(priority);
        archive.deserialize(dot);
        archive.deserialize(is_alpha_blending);
    }
};

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_PPU_TYPES_H
