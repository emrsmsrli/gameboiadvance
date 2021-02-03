/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_H
#define GAMEBOIADVANCE_PPU_H

#include <gba/core/container.h>

namespace gba::ppu {

class engine {
    friend class arm::arm7tdmi;

    vector<u8> palette_ram_{1_kb};
    vector<u8> vram_{96_kb};
    vector<u8> oam_{1_kb};

public:
    static constexpr auto addr_dispcnt = 0x0400'0000_u32;       //  2    R/W  DISPCNT   LCD Control
    static constexpr auto addr_greenswap = 0x0400'0002_u32;     //  2    R/W  -         Undocumented - Green Swap
    static constexpr auto addr_dispstat = 0x0400'0004_u32;      //  2    R/W  DISPSTAT  General LCD Status (STAT,LYC)
    static constexpr auto addr_vcount = 0x0400'0006_u32;        //  2    R    VCOUNT    Vertical Counter (LY)
    static constexpr auto addr_bg0cnt = 0x0400'0008_u32;        //  2    R/W  BG0CNT    BG0 Control
    static constexpr auto addr_bg1cnt = 0x0400'000A_u32;        //  2    R/W  BG1CNT    BG1 Control
    static constexpr auto addr_bg2cnt = 0x0400'000C_u32;        //  2    R/W  BG2CNT    BG2 Control
    static constexpr auto addr_bg3cnt = 0x0400'000E_u32;        //  2    R/W  BG3CNT    BG3 Control
    static constexpr auto addr_bg0hofs = 0x0400'0010_u32;       //  2    W    BG0HOFS   BG0 X-Offset
    static constexpr auto addr_bg0vofs = 0x0400'0012_u32;       //  2    W    BG0VOFS   BG0 Y-Offset
    static constexpr auto addr_bg1hofs = 0x0400'0014_u32;       //  2    W    BG1HOFS   BG1 X-Offset
    static constexpr auto addr_bg1vofs = 0x0400'0016_u32;       //  2    W    BG1VOFS   BG1 Y-Offset
    static constexpr auto addr_bg2hofs = 0x0400'0018_u32;       //  2    W    BG2HOFS   BG2 X-Offset
    static constexpr auto addr_bg2vofs = 0x0400'001A_u32;       //  2    W    BG2VOFS   BG2 Y-Offset
    static constexpr auto addr_bg3hofs = 0x0400'001C_u32;       //  2    W    BG3HOFS   BG3 X-Offset
    static constexpr auto addr_bg3vofs = 0x0400'001E_u32;       //  2    W    BG3VOFS   BG3 Y-Offset
    static constexpr auto addr_bg2pa = 0x0400'0020_u32;         //  2    W    BG2PA     BG2 Rotation/Scaling Parameter A (dx)
    static constexpr auto addr_bg2pb = 0x0400'0022_u32;         //  2    W    BG2PB     BG2 Rotation/Scaling Parameter B (dmx)
    static constexpr auto addr_bg2pc = 0x0400'0024_u32;         //  2    W    BG2PC     BG2 Rotation/Scaling Parameter C (dy)
    static constexpr auto addr_bg2pd = 0x0400'0026_u32;         //  2    W    BG2PD     BG2 Rotation/Scaling Parameter D (dmy)
    static constexpr auto addr_bg2x = 0x0400'0028_u32;          //  4    W    BG2X      BG2 Reference Point X-Coordinate
    static constexpr auto addr_bg2y = 0x0400'002C_u32;          //  4    W    BG2Y      BG2 Reference Point Y-Coordinate
    static constexpr auto addr_bg3pa = 0x0400'0030_u32;         //  2    W    BG3PA     BG3 Rotation/Scaling Parameter A (dx)
    static constexpr auto addr_bg3pb = 0x0400'0032_u32;         //  2    W    BG3PB     BG3 Rotation/Scaling Parameter B (dmx)
    static constexpr auto addr_bg3pc = 0x0400'0034_u32;         //  2    W    BG3PC     BG3 Rotation/Scaling Parameter C (dy)
    static constexpr auto addr_bg3pd = 0x0400'0036_u32;         //  2    W    BG3PD     BG3 Rotation/Scaling Parameter D (dmy)
    static constexpr auto addr_bg3x = 0x0400'0038_u32;          //  4    W    BG3X      BG3 Reference Point X-Coordinate
    static constexpr auto addr_bg3y = 0x0400'003C_u32;          //  4    W    BG3Y      BG3 Reference Point Y-Coordinate
    static constexpr auto addr_win0h = 0x0400'0040_u32;         //  2    W    WIN0H     Window 0 Horizontal Dimensions
    static constexpr auto addr_win1h = 0x0400'0042_u32;         //  2    W    WIN1H     Window 1 Horizontal Dimensions
    static constexpr auto addr_win0v = 0x0400'0044_u32;         //  2    W    WIN0V     Window 0 Vertical Dimensions
    static constexpr auto addr_win1v = 0x0400'0046_u32;         //  2    W    WIN1V     Window 1 Vertical Dimensions
    static constexpr auto addr_winin = 0x0400'0048_u32;         //  2    R/W  WININ     Inside of Window 0 and 1
    static constexpr auto addr_winout = 0x0400'004A_u32;        //  2    R/W  WINOUT    Inside of OBJ Window & Outside of Windows
    static constexpr auto addr_mosaic = 0x0400'004C_u32;        //  2    W    MOSAIC    Mosaic Size
    static constexpr auto addr_bldcnt = 0x0400'0050_u32;        //  2    R/W  BLDCNT    Color Special Effects Selection
    static constexpr auto addr_bldalpha = 0x0400'0052_u32;      //  2    R/W  BLDALPHA  Alpha Blending Coefficients
    static constexpr auto addr_bldy = 0x0400'0054_u32;          //  2    W    BLDY      Brightness (Fade-In/Out) Coefficient

    engine() = default;
};

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_PPU_H
