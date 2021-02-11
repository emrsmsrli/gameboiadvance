/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_TYPES_H
#define GAMEBOIADVANCE_TYPES_H

#include <type_traits>

#include <gba/core/integer.h>

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

/*0-2   BG Mode                    (0-5=Video Mode 0-5, 6-7=Prohibited)
3     Reserved / CGB Mode        (0=GBA, 1=CGB; can be set only by BIOS opcodes)
4     Display Frame Select       (0-1=Frame 0-1) (for BG Modes 4,5 only)
5     H-Blank Interval Free      (1=Allow access to OAM during H-Blank)
6     OBJ Character VRAM Mapping (0=Two dimensional, 1=One dimensional)
7     Forced Blank               (1=Allow FAST access to VRAM,Palette,OAM)
8     Screen Display BG0         (0=Off, 1=On)
9     Screen Display BG1         (0=Off, 1=On)
10    Screen Display BG2         (0=Off, 1=On)
11    Screen Display BG3         (0=Off, 1=On)
12    Screen Display OBJ         (0=Off, 1=On)
13    Window 0 Display Flag      (0=Off, 1=On)
14    Window 1 Display Flag      (0=Off, 1=On)
15    OBJ Window Display Flag    (0=Off, 1=On)*/
struct dispcnt {
    u8 bg_mode;
    u8 frame_select;
    bool hblank_interval_free = false;
    bool obj_mapping_1d = false;
    bool forced_blank = false;
    array<bool, 4> enable_bg;
    bool enable_w0 = false;
    bool enable_w1 = false;
    bool enable_wobj = false;
};

/*
Display status and Interrupt control. The H-Blank conditions are generated once per scanline, including for the 'hidden' scanlines during V-Blank.
  Bit   Expl.
  0     V-Blank flag   (Read only) (1=VBlank) (set in line 160..226; not 227)
  1     H-Blank flag   (Read only) (1=HBlank) (toggled in all lines, 0..227)
  2     V-Counter flag (Read only) (1=Match)  (set in selected line)     (R)
  3     V-Blank IRQ Enable         (1=Enable)                          (R/W)
  4     H-Blank IRQ Enable         (1=Enable)                          (R/W)
  5     V-Counter IRQ Enable       (1=Enable)                          (R/W)
  6     Not used (0) / DSi: LCD Initialization Ready (0=Busy, 1=Ready)   (R)
  7     Not used (0) / NDS: MSB of V-Vcount Setting (LYC.Bit8) (0..262)(R/W)
  8-15  V-Count Setting (LYC)      (0..227)                            (R/W)
*/
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
    u32 x_ref; // (W)
    u32 y_ref; // (W)
    u16 pa;    // BG2 Rotation/Scaling Parameter A (alias dx) (W)
    u16 pb;    // BG2 Rotation/Scaling Parameter B (alias dmx) (W)
    u16 pc;    // BG2 Rotation/Scaling Parameter C (alias dy) (W)
    u16 pd;    // BG2 Rotation/Scaling Parameter D (alias dmy) (W)
};

namespace traits {

template<typename T> struct is_bg_type { static constexpr bool value = false; };
template<> struct is_bg_type<bg_regular_base> { static constexpr bool value = true; };
template<> struct is_bg_type<bg_affine_base> { static constexpr bool value = true; };
template<> struct is_bg_type<bgcnt_affine_base> { static constexpr bool value = true; };

template<typename T> struct is_affine_bg { static constexpr bool value = false; };
template<> struct is_affine_bg<bg_affine_base> { static constexpr bool value = true; };

} // namespace traits

/*
  Bit   Expl.
  0-1   BG Priority           (0-3, 0=Highest)
  2-3   Character Base Block  (0-3, in units of 16 KBytes) (=BG Tile Data)
  4-5   Not used (must be zero) (except in NDS mode: MSBs of char base)
  6     Mosaic                (0=Disable, 1=Enable)
  7     Colors/Palettes       (0=16/16, 1=256/1)
  8-12  Screen Base Block     (0-31, in units of 2 KBytes) (=BG Map Data)
  13    BG0/BG1: Not used (except in NDS mode: Ext Palette Slot for BG0/BG1)
  13    BG2/BG3: Display Area Overflow (0=Transparent, 1=Wraparound)
  14-15 Screen Size (0-3)
*/
template<typename T>
struct bgcnt : T {
    static_assert(traits::is_bg_type<T>::value);

    u8 priority;
    u8 char_base_block;
    bool mosaic_enabled = false;
    bool color_depth_8bit = false;
    u8 screen_size;
};

template<typename T>
struct bg : T {
    static_assert(traits::is_bg_type<T>::value);

    u32 id;
    bgcnt<std::conditional_t<traits::is_affine_bg<T>::value, bgcnt_affine_base, bg_regular_base>> cnt;
    u16 hoffset;
    u16 voffset;

    explicit bg(const u32 id) : id{id} {}
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

/*
  Bit   Expl.
  0     BG0 1st Target Pixel (Background 0)
  1     BG1 1st Target Pixel (Background 1)
  2     BG2 1st Target Pixel (Background 2)
  3     BG3 1st Target Pixel (Background 3)
  4     OBJ 1st Target Pixel (Top-most OBJ pixel)
  5     BD  1st Target Pixel (Backdrop)
  6-7   Color Special Effect (0-3, see below)
         0 = None                (Special effects disabled)
         1 = Alpha Blending      (1st+2nd Target mixed)
         2 = Brightness Increase (1st Target becomes whiter)
         3 = Brightness Decrease (1st Target becomes blacker)
  8     BG0 2nd Target Pixel (Background 0)
  9     BG1 2nd Target Pixel (Background 1)
  10    BG2 2nd Target Pixel (Background 2)
  11    BG3 2nd Target Pixel (Background 3)
  12    OBJ 2nd Target Pixel (Top-most OBJ pixel)
  13    BD  2nd Target Pixel (Backdrop)*/
struct bldcnt {
    enum class effect : u8::type { none, alpha_blend, brightness_inc, brightness_dec };

    array<bool, 4> bg_first_target;
    bool obj_first_target;
    bool backdrop_first_target;
    effect effect_type;
    array<bool, 4> bg_second_target;
    bool obj_second_target;
    bool backdrop_second_target;
};

struct bldalpha {
    u8 eva;
    u8 evb;
};

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_TYPES_H
