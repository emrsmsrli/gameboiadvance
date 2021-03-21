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

constexpr auto addr_ie = 0x0400'0200_u32;            // 2     R/W  IE        Interrupt Enable Register
constexpr auto addr_if = 0x0400'0202_u32;            // 2     R/W  IF        Interrupt Request Flags / IRQ Acknowledge
constexpr auto addr_waitcnt = 0x0400'0204_u32;       // 2     R/W  WAITCNT   Game Pak Waitstate Control
constexpr auto addr_ime = 0x0400'0208_u32;           // 2     R/W  IME       Interrupt Master Enable Register
constexpr auto addr_postboot = 0x0400'0300_u32;      // 1     R/W  POSTFLG   Undocumented - Post Boot Flag
constexpr auto addr_haltcnt = 0x0400'0301_u32;       // 1     W    HALTCNT   Undocumented - Power Down Control

constexpr auto addr_tm0cnt_l = 0x0400'0100_u32;      // 2     R/W  TM0CNT_L  Timer 0 Counter/Reload
constexpr auto addr_tm0cnt_h = 0x0400'0102_u32;      // 2     R/W  TM0CNT_H  Timer 0 Control
constexpr auto addr_tm1cnt_l = 0x0400'0104_u32;      // 2     R/W  TM1CNT_L  Timer 1 Counter/Reload
constexpr auto addr_tm1cnt_h = 0x0400'0106_u32;      // 2     R/W  TM1CNT_H  Timer 1 Control
constexpr auto addr_tm2cnt_l = 0x0400'0108_u32;      // 2     R/W  TM2CNT_L  Timer 2 Counter/Reload
constexpr auto addr_tm2cnt_h = 0x0400'010A_u32;      // 2     R/W  TM2CNT_H  Timer 2 Control
constexpr auto addr_tm3cnt_l = 0x0400'010C_u32;      // 2     R/W  TM3CNT_L  Timer 3 Counter/Reload
constexpr auto addr_tm3cnt_h = 0x0400'010E_u32;      // 2     R/W  TM3CNT_H  Timer 3 Control

constexpr auto addr_dma0sad = 0x0400'00B0_u32;       //  4    W    DMA0SAD   DMA 0 Source Address
constexpr auto addr_dma0dad = 0x0400'00B4_u32;       //  4    W    DMA0DAD   DMA 0 Destination Address
constexpr auto addr_dma0cnt_l = 0x0400'00B8_u32;     //  2    W    DMA0CNT_L DMA 0 Word Count
constexpr auto addr_dma0cnt_h = 0x0400'00BA_u32;     //  2    R/W  DMA0CNT_H DMA 0 Control
constexpr auto addr_dma1sad = 0x0400'00BC_u32;       //  4    W    DMA1SAD   DMA 1 Source Address
constexpr auto addr_dma1dad = 0x0400'00C0_u32;       //  4    W    DMA1DAD   DMA 1 Destination Address
constexpr auto addr_dma1cnt_l = 0x0400'00C4_u32;     //  2    W    DMA1CNT_L DMA 1 Word Count
constexpr auto addr_dma1cnt_h = 0x0400'00C6_u32;     //  2    R/W  DMA1CNT_H DMA 1 Control
constexpr auto addr_dma2sad = 0x0400'00C8_u32;       //  4    W    DMA2SAD   DMA 2 Source Address
constexpr auto addr_dma2dad = 0x0400'00CC_u32;       //  4    W    DMA2DAD   DMA 2 Destination Address
constexpr auto addr_dma2cnt_l = 0x0400'00D0_u32;     //  2    W    DMA2CNT_L DMA 2 Word Count
constexpr auto addr_dma2cnt_h = 0x0400'00D2_u32;     //  2    R/W  DMA2CNT_H DMA 2 Control
constexpr auto addr_dma3sad = 0x0400'00D4_u32;       //  4    W    DMA3SAD   DMA 3 Source Address
constexpr auto addr_dma3dad = 0x0400'00D8_u32;       //  4    W    DMA3DAD   DMA 3 Destination Address
constexpr auto addr_dma3cnt_l = 0x0400'00DC_u32;     //  2    W    DMA3CNT_L DMA 3 Word Count
constexpr auto addr_dma3cnt_h = 0x0400'00DE_u32;     //  2    R/W  DMA3CNT_H DMA 3 Control

} // namespace arm

namespace keypad {

constexpr auto addr_state = 0x0400'0130_u32;         // 2    R    KEYINPUT  Key Status
constexpr auto addr_control = 0x0400'0132_u32;       // 2    R/W  KEYCNT    Key Interrupt Control

} // namespace keypad

namespace ppu {

constexpr auto addr_dispcnt = 0x0400'0000_u32;       // 2    R/W  DISPCNT   LCD Control
constexpr auto addr_greenswap = 0x0400'0002_u32;     // 2    R/W  -         Undocumented - Green Swap
constexpr auto addr_dispstat = 0x0400'0004_u32;      // 2    R/W  DISPSTAT  General LCD Status (STAT,LYC)
constexpr auto addr_vcount = 0x0400'0006_u32;        // 2    R    VCOUNT    Vertical Counter (LY)
constexpr auto addr_bg0cnt = 0x0400'0008_u32;        // 2    R/W  BG0CNT    BG0 Control
constexpr auto addr_bg1cnt = 0x0400'000A_u32;        // 2    R/W  BG1CNT    BG1 Control
constexpr auto addr_bg2cnt = 0x0400'000C_u32;        // 2    R/W  BG2CNT    BG2 Control
constexpr auto addr_bg3cnt = 0x0400'000E_u32;        // 2    R/W  BG3CNT    BG3 Control
constexpr auto addr_bg0hofs = 0x0400'0010_u32;       // 2    W    BG0HOFS   BG0 X-Offset
constexpr auto addr_bg0vofs = 0x0400'0012_u32;       // 2    W    BG0VOFS   BG0 Y-Offset
constexpr auto addr_bg1hofs = 0x0400'0014_u32;       // 2    W    BG1HOFS   BG1 X-Offset
constexpr auto addr_bg1vofs = 0x0400'0016_u32;       // 2    W    BG1VOFS   BG1 Y-Offset
constexpr auto addr_bg2hofs = 0x0400'0018_u32;       // 2    W    BG2HOFS   BG2 X-Offset
constexpr auto addr_bg2vofs = 0x0400'001A_u32;       // 2    W    BG2VOFS   BG2 Y-Offset
constexpr auto addr_bg3hofs = 0x0400'001C_u32;       // 2    W    BG3HOFS   BG3 X-Offset
constexpr auto addr_bg3vofs = 0x0400'001E_u32;       // 2    W    BG3VOFS   BG3 Y-Offset
constexpr auto addr_bg2pa = 0x0400'0020_u32;         // 2    W    BG2PA     BG2 Rotation/Scaling Parameter A (dx)
constexpr auto addr_bg2pb = 0x0400'0022_u32;         // 2    W    BG2PB     BG2 Rotation/Scaling Parameter B (dmx)
constexpr auto addr_bg2pc = 0x0400'0024_u32;         // 2    W    BG2PC     BG2 Rotation/Scaling Parameter C (dy)
constexpr auto addr_bg2pd = 0x0400'0026_u32;         // 2    W    BG2PD     BG2 Rotation/Scaling Parameter D (dmy)
constexpr auto addr_bg2x = 0x0400'0028_u32;          // 4    W    BG2X      BG2 Reference Point X-Coordinate
constexpr auto addr_bg2y = 0x0400'002C_u32;          // 4    W    BG2Y      BG2 Reference Point Y-Coordinate
constexpr auto addr_bg3pa = 0x0400'0030_u32;         // 2    W    BG3PA     BG3 Rotation/Scaling Parameter A (dx)
constexpr auto addr_bg3pb = 0x0400'0032_u32;         // 2    W    BG3PB     BG3 Rotation/Scaling Parameter B (dmx)
constexpr auto addr_bg3pc = 0x0400'0034_u32;         // 2    W    BG3PC     BG3 Rotation/Scaling Parameter C (dy)
constexpr auto addr_bg3pd = 0x0400'0036_u32;         // 2    W    BG3PD     BG3 Rotation/Scaling Parameter D (dmy)
constexpr auto addr_bg3x = 0x0400'0038_u32;          // 4    W    BG3X      BG3 Reference Point X-Coordinate
constexpr auto addr_bg3y = 0x0400'003C_u32;          // 4    W    BG3Y      BG3 Reference Point Y-Coordinate
constexpr auto addr_win0h = 0x0400'0040_u32;         // 2    W    WIN0H     Window 0 Horizontal Dimensions
constexpr auto addr_win1h = 0x0400'0042_u32;         // 2    W    WIN1H     Window 1 Horizontal Dimensions
constexpr auto addr_win0v = 0x0400'0044_u32;         // 2    W    WIN0V     Window 0 Vertical Dimensions
constexpr auto addr_win1v = 0x0400'0046_u32;         // 2    W    WIN1V     Window 1 Vertical Dimensions
constexpr auto addr_winin = 0x0400'0048_u32;         // 2    R/W  WININ     Inside of Window 0 and 1
constexpr auto addr_winout = 0x0400'004A_u32;        // 2    R/W  WINOUT    Inside of OBJ Window & Outside of Windows
constexpr auto addr_mosaic = 0x0400'004C_u32;        // 2    W    MOSAIC    Mosaic Size
constexpr auto addr_bldcnt = 0x0400'0050_u32;        // 2    R/W  BLDCNT    Color Special Effects Selection
constexpr auto addr_bldalpha = 0x0400'0052_u32;      // 2    R/W  BLDALPHA  Alpha Blending Coefficients
constexpr auto addr_bldy = 0x0400'0054_u32;          // 2    W    BLDY      Brightness (Fade-In/Out) Coefficient

} // namespace ppu

namespace apu {

constexpr auto addr_sound1cnt_l = 0x0400'0060_u32;   //  2   R/W  SOUND1CNT_L Channel 1 Sweep register       (NR10)
constexpr auto addr_sound1cnt_h = 0x0400'0062_u32;   //  2   R/W  SOUND1CNT_H Channel 1 Duty/Length/Envelope (NR11, NR12)
constexpr auto addr_sound1cnt_x = 0x0400'0064_u32;   //  2   R/W  SOUND1CNT_X Channel 1 Frequency/Control    (NR13, NR14)
constexpr auto addr_sound2cnt_l = 0x0400'0068_u32;   //  2   R/W  SOUND2CNT_L Channel 2 Duty/Length/Envelope (NR21, NR22)
constexpr auto addr_sound2cnt_h = 0x0400'006C_u32;   //  2   R/W  SOUND2CNT_H Channel 2 Frequency/Control    (NR23, NR24)
constexpr auto addr_sound3cnt_l = 0x0400'0070_u32;   //  2   R/W  SOUND3CNT_L Channel 3 Stop/Wave RAM select (NR30)
constexpr auto addr_sound3cnt_h = 0x0400'0072_u32;   //  2   R/W  SOUND3CNT_H Channel 3 Length/Volume        (NR31, NR32)
constexpr auto addr_sound3cnt_x = 0x0400'0074_u32;   //  2   R/W  SOUND3CNT_X Channel 3 Frequency/Control    (NR33, NR34)
constexpr auto addr_sound4cnt_l = 0x0400'0078_u32;   //  2   R/W  SOUND4CNT_L Channel 4 Length/Envelope      (NR41, NR42)
constexpr auto addr_sound4cnt_h = 0x0400'007C_u32;   //  2   R/W  SOUND4CNT_H Channel 4 Frequency/Control    (NR43, NR44)
constexpr auto addr_soundcnt_l = 0x0400'0080_u32;    //  2   R/W  SOUNDCNT_L  Control Stereo/Volume/Enable   (NR50, NR51)
constexpr auto addr_soundcnt_h = 0x0400'0082_u32;    //  2   R/W  SOUNDCNT_H  Control Mixing/DMA Control
constexpr auto addr_soundcnt_x = 0x0400'0084_u32;    //  2   R/W  SOUNDCNT_X  Control Sound on/off           (NR52)
constexpr auto addr_soundbias = 0x0400'0088_u32;     //  2   BIOS SOUNDBIAS   Sound PWM Control
constexpr auto addr_wave_ram = 0x0400'0090_u32;      //2x10h R/W  WAVE_RAM  Channel 3 Wave Pattern RAM (2 banks!!)
constexpr auto addr_fifo_a = 0x0400'00A0_u32;        //  4   W    FIFO_A    Channel A FIFO, Data 0-3
constexpr auto addr_fifo_b = 0x0400'00A4_u32;        //  4   W    FIFO_B    Channel B FIFO, Data 0-3

} // namespace apu

namespace sio {

constexpr auto addr_siodata32 = 0x0400'0120_u32;     //  4   R/W  SIODATA32 SIO Data (Normal-32bit Mode; shared with below)
constexpr auto addr_siomulti0 = 0x0400'0120_u32;     //  2   R/W  SIOMULTI0 SIO Data 0 (Parent)    (Multi-Player Mode)
constexpr auto addr_siomulti1 = 0x0400'0122_u32;     //  2   R/W  SIOMULTI1 SIO Data 1 (1st Child) (Multi-Player Mode)
constexpr auto addr_siomulti2 = 0x0400'0124_u32;     //  2   R/W  SIOMULTI2 SIO Data 2 (2nd Child) (Multi-Player Mode)
constexpr auto addr_siomulti3 = 0x0400'0126_u32;     //  2   R/W  SIOMULTI3 SIO Data 3 (3rd Child) (Multi-Player Mode)
constexpr auto addr_siocnt = 0x0400'0128_u32;        //  2   R/W  SIOCNT    SIO Control Register
constexpr auto addr_siomlt_send = 0x0400'012A_u32;   //  2   R/W  SIOMLT_SEND SIO Data (Local of MultiPlayer; shared below)
constexpr auto addr_siodata8 = 0x0400'012A_u32;      //  2   R/W  SIODATA8  SIO Data (Normal-8bit and UART Mode)

constexpr auto addr_rcnt = 0x0400'0134_u32;          //  2   R/W  RCNT      SIO Mode Select/General Purpose Data
constexpr auto addr_joycnt = 0x0400'0140_u32;        //  2   R/W  JOYCNT    SIO JOY Bus Control
constexpr auto addr_joy_recv = 0x0400'0150_u32;      //  4   R/W  JOY_RECV  SIO JOY Bus Receive Data
constexpr auto addr_joy_trans = 0x0400'0154_u32;     //  4   R/W  JOY_TRANS SIO JOY Bus Transmit Data
constexpr auto addr_joystat = 0x0400'0158_u32;       //  2   R/?  JOYSTAT   SIO JOY Bus Receive Status

} // namespace sio

} // namespace gba

#endif //GAMEBOIADVANCE_MMIO_ADDR_H
