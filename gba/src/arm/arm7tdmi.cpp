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
        /* EQ */ case 0x0: return cpsr_.z;
        /* NE */ case 0x1: return !cpsr_.z;
        /* CS */ case 0x2: return cpsr_.c;
        /* CC */ case 0x3: return !cpsr_.c;
        /* MI */ case 0x4: return cpsr_.n;
        /* PL */ case 0x5: return !cpsr_.n;
        /* VS */ case 0x6: return cpsr_.v;
        /* VC */ case 0x7: return !cpsr_.v;
        /* HI */ case 0x8: return cpsr_.c && !cpsr_.z;
        /* LS */ case 0x9: return !cpsr_.c || cpsr_.z;
        /* GE */ case 0xA: return cpsr_.n == cpsr_.v;
        /* LT */ case 0xB: return cpsr_.n != cpsr_.v;
        /* GT */ case 0xC: return !cpsr_.z && cpsr_.n == cpsr_.v;
        /* LE */ case 0xD: return cpsr_.z || cpsr_.n != cpsr_.v;
        /* AL */ case 0xE: return true;
    }

    // NV
    return false;
}

} // namespace gba