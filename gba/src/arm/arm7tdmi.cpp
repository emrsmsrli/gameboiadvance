/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>

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
    cpsr().mode = privilege_mode::svc;
    switch_mode(cpsr().mode);
    cpsr().i = true;
    cpsr().f = true;

    if(bios_.empty()) {
        r_[0_u8] = 0x0000'0CA5_u32;
        sp() = 0x0300'7F00_u32;
        lr() = 0x0800'0000_u32;
        pc() = 0x0800'0000_u32;
        reg_banks_[register_bank::irq].named.r13 = 0x0300'7FA0_u32;
        reg_banks_[register_bank::svc].named.r13 = 0x0300'7FE0_u32;
        switch_mode(privilege_mode::sys);
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

#if WITH_DEBUGGER
        if(on_instruction_execute(pc() - (cpsr_.t ? 4_u32 : 8_u32))) {
            return;
        }
#endif // WITH_DEBUGGER

        const u32 instruction = pipeline_.executing;
        pipeline_.executing = pipeline_.decoding;

        if(cpsr().t) {
            pc() = bit::clear(pc(), 0_u8); // halfword align
            pipeline_.decoding = read_16(pc(), pipeline_.fetch_type);
            auto func = thumb_table_[instruction >> 6_u32];
            ASSERT(func.is_valid());
            func(this, narrow<u16>(instruction));
        } else {
            pc() = mask::clear(pc(), 0b11_u32); // word align
            pipeline_.decoding = read_32(pc(), pipeline_.fetch_type);

            if(condition_met(instruction >> 28_u32)) {
                auto func = arm_table_[((instruction >> 16_u32) & 0xFF0_u32) | ((instruction >> 4_u32) & 0xF_u32)];
                ASSERT(func.is_valid());
                func(this, instruction);
            } else {
                pipeline_.fetch_type = mem_access::seq;
                pc() += 4_u32;
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
        irq_signal_delay_handle_ = core_->schdlr.ADD_HW_EVENT(1_usize, arm7tdmi::update_irq_signal);
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

    spsr_banks_[register_bank::irq] = cpsr();
    switch_mode(privilege_mode::irq);
    cpsr().i = true;

    if(cpsr().t) {
        cpsr().t = false;
        lr() = pc();
    } else {
        lr() = pc() - 4_u32;
    }

    pc() = 0x0000'0018_u32;
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

void arm7tdmi::switch_mode(const privilege_mode mode) noexcept
{
    const register_bank old_bank = bank_from_privilege_mode(cpsr().mode);
    const register_bank new_bank = bank_from_privilege_mode(mode);

    cpsr().mode = mode;

    if (old_bank == new_bank) {
        return;
    }

    banked_regs& old_regs = reg_banks_[old_bank];
    banked_regs& new_regs = reg_banks_[new_bank];
    if(old_bank == register_bank::fiq || new_bank == register_bank::fiq) {
        std::copy(r_.begin() + 8_i32, r_.begin() + 15_i32, old_regs.r.begin());
        std::copy(new_regs.r.begin(), new_regs.r.end(), r_.begin() + 8_i32);
    } else {
        old_regs.named.r13 = sp();
        old_regs.named.r14 = lr();
        sp() = new_regs.named.r13;
        lr() = new_regs.named.r14;
    }
}

bool arm7tdmi::condition_met(const u32 cond) const noexcept
{
    if(LIKELY(cond == /* AL */ 0xE_u32)) {
        return true;
    }

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
    }

    // NV
    return false;
}

} // namespace gba::arm
