/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cpu/arm7tdmi.h>

#include <algorithm>

#include <gba/cpu/arm7tdmi_decoder_table_gen.h>

namespace gba::cpu {

namespace {

constexpr decoder_table_generator::arm_decoder_table arm_table = decoder_table_generator::generate_arm();
constexpr decoder_table_generator::thumb_decoder_table thumb_table = decoder_table_generator::generate_thumb();

} // namespace

arm7tdmi::arm7tdmi(bus_interface* bus, scheduler* scheduler) noexcept
  : bus_{bus},
    scheduler_{scheduler},
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
}

void arm7tdmi::execute_instruction() noexcept
{
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
        pipeline_.decoding = bus_->read_16(pc(), pipeline_.fetch_type);
        auto func = thumb_table[instruction >> 6_u32];
        ASSERT(func.is_valid());
        func(this, narrow<u16>(instruction));
    } else {
        pc() = mask::clear(pc(), 0b11_u32); // word align
        pipeline_.decoding = bus_->read_32(pc(), pipeline_.fetch_type);

        if(condition_met(instruction >> 28_u32)) {
            auto func = arm_table[((instruction >> 16_u32) & 0xFF0_u32) | ((instruction >> 4_u32) & 0xF_u32)];
            ASSERT(func.is_valid());
            func(this, instruction);
        } else {
            pipeline_.fetch_type = mem_access::seq;
            pc() += 4_u32;
        }
    }
}

void arm7tdmi::schedule_update_irq_signal() noexcept
{
    scheduled_irq_signal_ = ime_ && interrupt_available();

    if(scheduled_irq_signal_ != irq_signal_) {
        scheduler_->remove_event(irq_signal_delay_handle_);
        irq_signal_delay_handle_ = scheduler_->ADD_HW_EVENT(1_u32, arm7tdmi::update_irq_signal);
    }
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

void arm7tdmi::switch_mode(const privilege_mode mode) noexcept
{
    const register_bank old_bank = bank_from_privilege_mode(cpsr().mode);
    const register_bank new_bank = bank_from_privilege_mode(mode);

    cpsr().mode = mode;

    if(old_bank == new_bank) {
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

} // namespace gba::cpu
