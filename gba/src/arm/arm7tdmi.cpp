/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba {

void arm7tdmi::tick() noexcept
{

}

u32& arm7tdmi::r(const u8 index) noexcept
{
    switch(index.get()) {
        case  0: return r0_;
        case  1: return r1_;
        case  2: return r2_;
        case  3: return r3_;
        case  4: return r4_;
        case  5: return r5_;
        case  6: return r6_;
        case  7: return r7_;
        case  8: return cpsr_.mode == privilege_mode::fiq ? fiq_.r8 : r8_;
        case  9: return cpsr_.mode == privilege_mode::fiq ? fiq_.r9 : r9_;
        case 10: return cpsr_.mode == privilege_mode::fiq ? fiq_.r10 : r10_;
        case 11: return cpsr_.mode == privilege_mode::fiq ? fiq_.r11 : r11_;
        case 12: return cpsr_.mode == privilege_mode::fiq ? fiq_.r12 : r12_;
        case 13: switch(cpsr_.mode) {
            case privilege_mode::fiq: return fiq_.r13;
            case privilege_mode::irq: return irq_.r13;
            case privilege_mode::svc: return svc_.r13;
            case privilege_mode::abt: return abt_.r13;
            case privilege_mode::und: return und_.r13;
            default: return r13_;
        }
        case 14: switch(cpsr_.mode) {
            case privilege_mode::fiq: return fiq_.r14;
            case privilege_mode::irq: return irq_.r14;
            case privilege_mode::svc: return svc_.r14;
            case privilege_mode::abt: return abt_.r14;
            case privilege_mode::und: return und_.r14;
            default: return r14_;
        }
        case 15: return r15_;
    }

    UNREACHABLE();
}

psr& arm7tdmi::spsr() noexcept
{
    switch(cpsr_.mode) {
        case privilege_mode::fiq: return fiq_.spsr;
        case privilege_mode::irq: return irq_.spsr;
        case privilege_mode::svc: return svc_.spsr;
        case privilege_mode::abt: return abt_.spsr;
        case privilege_mode::und: return und_.spsr;
        default:
            UNREACHABLE();
    }
}

bool arm7tdmi::condition_met(const u32 instruction) const noexcept
{
    const u32 cond = instruction >> 28_u32;
    switch(cond.get()) {
        /* EQ */ case 0x0u: return cpsr_.z;
        /* NE */ case 0x1u: return !cpsr_.z;
        /* CS */ case 0x2u: return cpsr_.c;
        /* CC */ case 0x3u: return !cpsr_.c;
        /* MI */ case 0x4u: return cpsr_.n;
        /* PL */ case 0x5u: return !cpsr_.n;
        /* VS */ case 0x6u: return cpsr_.v;
        /* VC */ case 0x7u: return !cpsr_.v;
        /* HI */ case 0x8u: return cpsr_.c && !cpsr_.z;
        /* LS */ case 0x9u: return !cpsr_.c || cpsr_.z;
        /* GE */ case 0xAu: return cpsr_.n == cpsr_.v;
        /* LT */ case 0xBu: return cpsr_.n != cpsr_.v;
        /* GT */ case 0xCu: return !cpsr_.z && cpsr_.n == cpsr_.v;
        /* LE */ case 0xDu: return cpsr_.z || cpsr_.n != cpsr_.v;
        /* AL */ case 0xEu: return true; // fixme does not add prefetch cycles?
    }

    // NV
    return false;
}

/*General Internal Memory

  00000000-00003FFF   BIOS - System ROM         (16 KBytes)
  00004000-01FFFFFF   Not used
  02000000-0203FFFF   WRAM - On-board Work RAM  (256 KBytes) 2 Wait
  02040000-02FFFFFF   Not used
  03000000-03007FFF   WRAM - On-chip Work RAM   (32 KBytes)
  03008000-03FFFFFF   Not used
  04000000-040003FE   I/O Registers
  04000400-04FFFFFF   Not used

Internal Display Memory

  05000000-050003FF   BG/OBJ Palette RAM        (1 Kbyte)
  05000400-05FFFFFF   Not used
  06000000-06017FFF   VRAM - Video RAM          (96 KBytes)
  06018000-06FFFFFF   Not used
  07000000-070003FF   OAM - OBJ Attributes      (1 Kbyte)
  07000400-07FFFFFF   Not used

External Memory (Game Pak)

  08000000-09FFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 0
  0A000000-0BFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 1
  0C000000-0DFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 2
  0E000000-0E00FFFF   Game Pak SRAM    (max 64 KBytes) - 8bit Bus width
  0E010000-0FFFFFFF   Not used

Unused Memory Area

  10000000-FFFFFFFF   Not used (upper 4bits of address bus unused)

 ---------------------------------------------------------------------------------------------------------------------

Reading from Unused Memory (00004000-01FFFFFF,10000000-FFFFFFFF)
Accessing unused memory at 00004000h-01FFFFFFh, and 10000000h-FFFFFFFFh (and 02000000h-03FFFFFFh when RAM is disabled via Port 4000800h)
returns the recently pre-fetched opcode. For ARM code this is simply:

  WORD = [$+8]

For THUMB code the result consists of two 16bit fragments and depends on the address area and alignment where the opcode was stored.
For THUMB code in Main RAM, Palette Memory, VRAM, and Cartridge ROM this is:

  LSW = [$+4], MSW = [$+4]

For THUMB code in BIOS or OAM (and in 32K-WRAM on Original-NDS (in GBA mode)):

  LSW = [$+4], MSW = [$+6]   ;for opcodes at 4-byte aligned locations
  LSW = [$+2], MSW = [$+4]   ;for opcodes at non-4-byte aligned locations

For THUMB code in 32K-WRAM on GBA, GBA SP, GBA Micro, NDS-Lite (but not NDS):

  LSW = [$+4], MSW = OldHI   ;for opcodes at 4-byte aligned locations
  LSW = OldLO, MSW = [$+4]   ;for opcodes at non-4-byte aligned locations

Whereas OldLO/OldHI are usually:

  OldLO=[$+2], OldHI=[$+2]

Unless the previous opcode's prefetch was overwritten;
 that can happen if the previous opcode was itself an LDR opcode, ie. if it was itself reading data:

  OldLO=LSW(data), OldHI=MSW(data)
  Theoretically, this might also change if a DMA transfer occurs.

Note: Additionally, as usually, the 32bit data value will be rotated if the data address wasn't 4-byte aligned,
 and the upper bits of the 32bit value will be masked in case of LDRB/LDRH reads.
Note: The opcode prefetch is caused by the prefetch pipeline in the CPU itself, not by the external gamepak prefetch,
 ie. it works for code in ROM and RAM as well.

*/

} // namespace gba
