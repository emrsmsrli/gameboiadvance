/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_MMIO_ADDR_H
#define GAMEBOIADVANCE_MMIO_ADDR_H

#include <gba/core/integer.h>

namespace gba {

namespace arm {

constexpr auto addr_ie = 0x400'0200_u32;            //  2    R/W  IE        Interrupt Enable Register
constexpr auto addr_if = 0x400'0202_u32;            //  2    R/W  IF        Interrupt Request Flags / IRQ Acknowledge
constexpr auto addr_waitcnt = 0x400'0204_u32;       //  2    R/W  WAITCNT   Game Pak Waitstate Control
constexpr auto addr_ime = 0x400'0208_u32;           //  2    R/W  IME       Interrupt Master Enable Register
constexpr auto addr_postboot = 0x400'0300_u32;      //  1    R/W  POSTFLG   Undocumented - Post Boot Flag
constexpr auto addr_haltcnt = 0x400'0301_u32;       //  1    W    HALTCNT   Undocumented - Power Down Control

constexpr auto addr_tm0cnt_l = 0x400'0100_u32;      // 2     R/W  TM0CNT_L Timer 0 Counter/Reload
constexpr auto addr_tm0cnt_h = 0x400'0102_u32;      // 2     R/W  TM0CNT_H Timer 0 Control
constexpr auto addr_tm1cnt_l = 0x400'0104_u32;      // 2     R/W  TM1CNT_L Timer 1 Counter/Reload
constexpr auto addr_tm1cnt_h = 0x400'0106_u32;      // 2     R/W  TM1CNT_H Timer 1 Control
constexpr auto addr_tm2cnt_l = 0x400'0108_u32;      // 2     R/W  TM2CNT_L Timer 2 Counter/Reload
constexpr auto addr_tm2cnt_h = 0x400'010A_u32;      // 2     R/W  TM2CNT_H Timer 2 Control
constexpr auto addr_tm3cnt_l = 0x400'010C_u32;      // 2     R/W  TM3CNT_L Timer 3 Counter/Reload
constexpr auto addr_tm3cnt_h = 0x400'010E_u32;      // 2     R/W  TM3CNT_H Timer 3 Control

} // namespace arm

namespace keypad {

constexpr auto addr_state = 0x0400'0130_u32;
constexpr auto addr_control = 0x0400'0132_u32;

} // namespace keypad

namespace ppu {

constexpr auto addr_dispcnt = 0x0400'0000_u32;       //  2    R/W  DISPCNT   LCD Control
constexpr auto addr_greenswap = 0x0400'0002_u32;     //  2    R/W  -         Undocumented - Green Swap
constexpr auto addr_dispstat = 0x0400'0004_u32;      //  2    R/W  DISPSTAT  General LCD Status (STAT,LYC)
constexpr auto addr_vcount = 0x0400'0006_u32;        //  2    R    VCOUNT    Vertical Counter (LY)
constexpr auto addr_bg0cnt = 0x0400'0008_u32;        //  2    R/W  BG0CNT    BG0 Control
constexpr auto addr_bg1cnt = 0x0400'000A_u32;        //  2    R/W  BG1CNT    BG1 Control
constexpr auto addr_bg2cnt = 0x0400'000C_u32;        //  2    R/W  BG2CNT    BG2 Control
constexpr auto addr_bg3cnt = 0x0400'000E_u32;        //  2    R/W  BG3CNT    BG3 Control
constexpr auto addr_bg0hofs = 0x0400'0010_u32;       //  2    W    BG0HOFS   BG0 X-Offset
constexpr auto addr_bg0vofs = 0x0400'0012_u32;       //  2    W    BG0VOFS   BG0 Y-Offset
constexpr auto addr_bg1hofs = 0x0400'0014_u32;       //  2    W    BG1HOFS   BG1 X-Offset
constexpr auto addr_bg1vofs = 0x0400'0016_u32;       //  2    W    BG1VOFS   BG1 Y-Offset
constexpr auto addr_bg2hofs = 0x0400'0018_u32;       //  2    W    BG2HOFS   BG2 X-Offset
constexpr auto addr_bg2vofs = 0x0400'001A_u32;       //  2    W    BG2VOFS   BG2 Y-Offset
constexpr auto addr_bg3hofs = 0x0400'001C_u32;       //  2    W    BG3HOFS   BG3 X-Offset
constexpr auto addr_bg3vofs = 0x0400'001E_u32;       //  2    W    BG3VOFS   BG3 Y-Offset
constexpr auto addr_bg2pa = 0x0400'0020_u32;         //  2    W    BG2PA     BG2 Rotation/Scaling Parameter A (dx)
constexpr auto addr_bg2pb = 0x0400'0022_u32;         //  2    W    BG2PB     BG2 Rotation/Scaling Parameter B (dmx)
constexpr auto addr_bg2pc = 0x0400'0024_u32;         //  2    W    BG2PC     BG2 Rotation/Scaling Parameter C (dy)
constexpr auto addr_bg2pd = 0x0400'0026_u32;         //  2    W    BG2PD     BG2 Rotation/Scaling Parameter D (dmy)
constexpr auto addr_bg2x = 0x0400'0028_u32;          //  4    W    BG2X      BG2 Reference Point X-Coordinate
constexpr auto addr_bg2y = 0x0400'002C_u32;          //  4    W    BG2Y      BG2 Reference Point Y-Coordinate
constexpr auto addr_bg3pa = 0x0400'0030_u32;         //  2    W    BG3PA     BG3 Rotation/Scaling Parameter A (dx)
constexpr auto addr_bg3pb = 0x0400'0032_u32;         //  2    W    BG3PB     BG3 Rotation/Scaling Parameter B (dmx)
constexpr auto addr_bg3pc = 0x0400'0034_u32;         //  2    W    BG3PC     BG3 Rotation/Scaling Parameter C (dy)
constexpr auto addr_bg3pd = 0x0400'0036_u32;         //  2    W    BG3PD     BG3 Rotation/Scaling Parameter D (dmy)
constexpr auto addr_bg3x = 0x0400'0038_u32;          //  4    W    BG3X      BG3 Reference Point X-Coordinate
constexpr auto addr_bg3y = 0x0400'003C_u32;          //  4    W    BG3Y      BG3 Reference Point Y-Coordinate
constexpr auto addr_win0h = 0x0400'0040_u32;         //  2    W    WIN0H     Window 0 Horizontal Dimensions
constexpr auto addr_win1h = 0x0400'0042_u32;         //  2    W    WIN1H     Window 1 Horizontal Dimensions
constexpr auto addr_win0v = 0x0400'0044_u32;         //  2    W    WIN0V     Window 0 Vertical Dimensions
constexpr auto addr_win1v = 0x0400'0046_u32;         //  2    W    WIN1V     Window 1 Vertical Dimensions
constexpr auto addr_winin = 0x0400'0048_u32;         //  2    R/W  WININ     Inside of Window 0 and 1
constexpr auto addr_winout = 0x0400'004A_u32;        //  2    R/W  WINOUT    Inside of OBJ Window & Outside of Windows
constexpr auto addr_mosaic = 0x0400'004C_u32;        //  2    W    MOSAIC    Mosaic Size
constexpr auto addr_bldcnt = 0x0400'0050_u32;        //  2    R/W  BLDCNT    Color Special Effects Selection
constexpr auto addr_bldalpha = 0x0400'0052_u32;      //  2    R/W  BLDALPHA  Alpha Blending Coefficients
constexpr auto addr_bldy = 0x0400'0054_u32;          //  2    W    BLDY      Brightness (Fade-In/Out) Coefficient

} // namespace ppu

} // namespace gba

#endif //GAMEBOIADVANCE_MMIO_ADDR_H
