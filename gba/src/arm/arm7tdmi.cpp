/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>
#include <gba/core.h>

namespace gba::arm {

arm7tdmi::arm7tdmi(core* core, vector<u8> bios) noexcept
  : core_{core},
    bios_{std::move(bios)},
    pipeline_{
      mem_access::non_seq,
      0xF000'0000_u32,
      0xF000'0000_u32
    }
{
    if(bios_.empty()) {
        r(0_u8) = 0x0000'0CA5_u32;
        r(13_u8) = 0x0300'7F00_u32;
        r(14_u8) = 0x0800'0000_u32;
        r(15_u8) = 0x0800'0000_u32;
        irq_.r13 = 0x0300'7FA0_u32;
        svc_.r13 = 0x0300'7FE0_u32;
        irq_.spsr.mode = privilege_mode::usr;
        svc_.spsr.mode = privilege_mode::usr;
        cpsr().mode = privilege_mode::sys;
    } else {
        ASSERT(bios_.size() == 16_kb);
    }

    update_waitstate_table();
}

void arm7tdmi::tick() noexcept
{
    if(UNLIKELY(haltcnt_ == halt_control::halted && interrupt_available())) {
        haltcnt_ = halt_control::running;
    }

    if(LIKELY(haltcnt_ == halt_control::running)) {
        if(irq_signal_) {
            process_interrupts();
        }

        u32& pc = r(15_u8);

#if WITH_DEBUGGER
        if(on_instruction_execute(pc - (cpsr_.t ? 4_u32 : 8_u32))) {
            return;
        }
#endif // WITH_DEBUGGER

        const u32 instruction = pipeline_.executing;
        pipeline_.executing = pipeline_.decoding;

        if(cpsr().t) {
            pc = bit::clear(pc, 0_u8); // halfword align
            pipeline_.decoding = read_16(pc, pipeline_.fetch_type);
            auto func = thumb_table_[instruction >> 6_u32];
            ASSERT(func.is_valid());
            func(this, narrow<u16>(instruction));
        } else {
            pc = mask::clear(pc, 0b11_u32); // word align
            pipeline_.decoding = read_32(pc, pipeline_.fetch_type);

            if(condition_met(instruction >> 28_u32)) {
                auto func = arm_table_[((instruction >> 16_u32) & 0xFF0_u32) | ((instruction >> 4_u32) & 0xF_u32)];
                ASSERT(func.is_valid());
                func(this, instruction);
            } else {
                pipeline_.fetch_type = mem_access::seq;
                pc += 4_u32;
            }
        }
     } else {
        tick_components(core_->schdlr.remaining_cycles_to_next_event());
     }
}

void arm7tdmi::schedule_update_irq_signal() noexcept
{
    scheduled_irq_signal_ = ime_ && interrupt_available();

    if(scheduled_irq_signal_ != irq_signal_) {
        core_->schdlr.remove_event(irq_signal_delay_handle_);
        irq_signal_delay_handle_ = core_->schdlr.ADD_EVENT(1_usize, arm7tdmi::update_irq_signal);
    }
}

void arm7tdmi::update_irq_signal(u64 /*late_cycles*/) noexcept
{
    irq_signal_ = scheduled_irq_signal_;
}

void arm7tdmi::process_interrupts() noexcept
{
    if(cpsr().i) {
        return;
    }

    irq_.spsr = cpsr();
    cpsr().mode = privilege_mode::irq;
    cpsr().i = true;

    if(cpsr().t) {
        cpsr().t = false;
        r(14_u8) = r15_;
    } else {
        r(14_u8) = r15_ - 4_u32;
    }

    r15_ = 0x0000'0018_u32;
    pipeline_flush<instruction_mode::arm>();
}

void arm7tdmi::tick_internal() noexcept
{
    tick_components(1_u64);
}

void arm7tdmi::tick_components(const u64 cycles) noexcept
{
    // todo break this into pieces that handle pak prefetch system https://mgba.io/2015/06/27/cycle-counting-prefetch/

    core_->dma_controller.run_channels();
    core_->schdlr.add_cycles(cycles);
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

bool arm7tdmi::condition_met(const u32 cond) const noexcept
{
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

} // namespace gba::arm
