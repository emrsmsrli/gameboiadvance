/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM7TDMI_H
#define GAMEBOIADVANCE_ARM7TDMI_H

#include <gba/cpu/bus_interface.h>
#include <gba/cpu/irq_controller_handle.h>
#include <gba/core/container.h>
#include <gba/core/fwd.h>
#include <gba/core/math.h>
#include <gba/core/scheduler.h>
#include <gba/helper/bitflags.h>
#include <gba/helper/function_ptr.h>
#include <gba/helper/lookup_table.h>
#include <gba/helper/range.h>

namespace gba::cpu {

enum class register_bank : u32::type {
    none, irq, svc, fiq, abt, und
};

enum class privilege_mode : u8::type {
    usr = 0x10,  // user
    fiq = 0x11,  // fast interrupt
    irq = 0x12,  // interrupt
    svc = 0x13,  // service
    abt = 0x17,  // abort
    und = 0x1b,  // undefined
    sys = 0x1f,  // system
};

[[nodiscard]] FORCEINLINE constexpr register_bank bank_from_privilege_mode(const privilege_mode mode) noexcept
{
    switch(mode) {
        case privilege_mode::sys:
        case privilege_mode::usr: return register_bank::none;
        case privilege_mode::fiq: return register_bank::fiq;
        case privilege_mode::irq: return register_bank::irq;
        case privilege_mode::svc: return register_bank::svc;
        case privilege_mode::abt: return register_bank::abt;
        case privilege_mode::und: return register_bank::und;
        default: UNREACHABLE();  // don't handle invalid modes
    }
}

struct psr {
    bool n = false;  // signed flag
    bool z = false;  // zero flag
    bool c = false;  // carry flag
    bool v = false;  // overflow flag
    bool i = false;  // irq disabled flag
    bool f = false;  // fiq disabled flag
    bool t = false;  // thumb mode flag
    privilege_mode mode{privilege_mode::svc};

    constexpr explicit operator u32() const noexcept
    {
        return from_enum<u32>(mode)
          | bit::from_bool(t) << 5_u32
          | bit::from_bool(f) << 6_u32
          | bit::from_bool(i) << 7_u32
          | bit::from_bool(v) << 28_u32
          | bit::from_bool(c) << 29_u32
          | bit::from_bool(z) << 30_u32
          | bit::from_bool(n) << 31_u32;
    }

    constexpr psr& operator=(const psr&) = default;
    constexpr psr& operator=(const u32 data) noexcept
    {
        mode = to_enum<privilege_mode>(data & 0x1F_u32);
        copy_without_mode(data);
        return *this;
    }

    constexpr void copy_without_mode(const psr& other) noexcept
    {
        t = other.t;
        f = other.f;
        i = other.i;

        v = other.v;
        c = other.c;
        z = other.z;
        n = other.n;
    }

    constexpr void copy_without_mode(const u32 data) noexcept
    {
        t = bit::test(data, 5_u8);
        f = bit::test(data, 6_u8);
        i = bit::test(data, 7_u8);

        v = bit::test(data, 28_u8);
        c = bit::test(data, 29_u8);
        z = bit::test(data, 30_u8);
        n = bit::test(data, 31_u8);
    }
};

union banked_regs {
    struct regs {
        u32 r8; u32 r9; u32 r10; u32 r11; u32 r12; u32 r13; u32 r14;
    } named;
    array<u32, 7> r{};
};

struct reg_banks {
    array<banked_regs, 6> reg_banks;

    FORCEINLINE constexpr banked_regs& operator[](const register_bank bank) noexcept { return reg_banks[from_enum<u32>(bank)]; }
    FORCEINLINE constexpr const banked_regs& operator[](const register_bank bank) const noexcept { return reg_banks[from_enum<u32>(bank)]; }
};

struct spsr_banks {
    array<psr, 5> banks;

    FORCEINLINE constexpr psr& operator[](const register_bank bank) noexcept
    {
        ASSERT(bank != register_bank::none);
        return banks[from_enum<u32>(bank) - 1_u32];
    }
    FORCEINLINE constexpr const psr& operator[](const register_bank bank) const noexcept
    {
        ASSERT(bank != register_bank::none);
        return banks[from_enum<u32>(bank) - 1_u32];
    }
};

enum class instruction_mode { arm, thumb };

struct pipeline {
    mem_access fetch_type{mem_access::non_seq};
    u32 executing;
    u32 decoding;
};

class arm7tdmi {
protected:
    bus_interface* bus_;
    scheduler* scheduler_;

    array<u32, 16> r_{};
    reg_banks reg_banks_{};

    psr cpsr_;
    spsr_banks spsr_banks_{};

    u16 ie_;
    u16 if_;
    bool ime_ = false;
    bool irq_signal_ = false;
    bool scheduled_irq_signal_ = false;
    scheduler::hw_event::handle irq_signal_delay_handle_;

    pipeline pipeline_;

public:
#if WITH_DEBUGGER
    delegate<bool(u32)> on_instruction_execute;
#endif // WITH_DEBUGGER

    arm7tdmi(bus_interface* bus, scheduler* scheduler) noexcept;

    void execute_instruction() noexcept;

    FORCEINLINE void request_interrupt(const interrupt_source irq) noexcept
    {
        if_ |= from_enum<u16>(irq);
        schedule_update_irq_signal();
    }

    irq_controller_handle get_interrupt_handle() noexcept { return irq_controller_handle{this}; }

private:
    [[nodiscard]] u32 read_32_aligned(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_16_signed(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_16_aligned(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_8_signed(u32 addr, mem_access access) noexcept;

    void update_irq_signal(u64 /*late_cycles*/) noexcept { irq_signal_ = scheduled_irq_signal_; }
    void process_interrupts() noexcept;

    // ARM instructions
    void data_processing_imm_shifted_reg(u32 instr) noexcept;
    void data_processing_reg_shifted_reg(u32 instr) noexcept;
    void data_processing_imm(u32 instr) noexcept;
    void data_processing(u32 instr, u32 first_op, u32 second_op, bool carry) noexcept;
    void branch_exchange(u32 instr) noexcept;
    void halfword_data_transfer_reg(u32 instr) noexcept;
    void halfword_data_transfer_imm(u32 instr) noexcept;
    void halfword_data_transfer(u32 instr, u32 offset) noexcept;
    void psr_transfer_reg(u32 instr) noexcept;
    void psr_transfer_imm(u32 instr) noexcept;
    void psr_transfer_msr(u32 instr, u32 operand, bool use_spsr) noexcept;
    void multiply(u32 instr) noexcept;
    void multiply_long(u32 instr) noexcept;
    void single_data_swap(u32 instr) noexcept;
    void single_data_transfer(u32 instr) noexcept;
    void undefined(u32 instr) noexcept;
    void block_data_transfer(u32 instr) noexcept;
    void branch_with_link(u32 instr) noexcept;
    void swi_arm(u32 instr) noexcept;

    // THUMB instructions
    void move_shifted_reg(u16 instr) noexcept;
    void add_subtract(u16 instr) noexcept;
    void mov_cmp_add_sub_imm(u16 instr) noexcept;
    void alu(u16 instr) noexcept;
    void hireg_bx(u16 instr) noexcept;
    void pc_rel_load(u16 instr) noexcept;
    void ld_str_reg(u16 instr) noexcept;
    void ld_str_sign_extended_byte_hword(u16 instr) noexcept;
    void ld_str_imm(u16 instr) noexcept;
    void ld_str_hword(u16 instr) noexcept;
    void ld_str_sp_relative(u16 instr) noexcept;
    void ld_addr(u16 instr) noexcept;
    void add_offset_to_sp(u16 instr) noexcept;
    void push_pop(u16 instr) noexcept;
    void ld_str_multiple(u16 instr) noexcept;
    void branch_cond(u16 instr) noexcept;
    void swi_thumb(u16 instr) noexcept;
    void branch(u16 instr) noexcept;
    void long_branch_link(u16 instr) noexcept;

    // decoder helpers
    [[nodiscard]] bool condition_met(u32 cond) const noexcept;

    [[nodiscard]] FORCEINLINE bool in_privileged_mode() const noexcept { return cpsr_.mode != privilege_mode::usr; }
    [[nodiscard]] FORCEINLINE bool in_exception_mode() const noexcept { return in_privileged_mode() && cpsr_.mode != privilege_mode::sys; }

    template<u8::type Count>
    static_vector<u8, Count> generate_register_list(const u32 instr) noexcept
    {
        static_vector<u8, Count> regs;
        for(u8 r : range(Count)) {
            if(bit::test(instr, r)) {
                regs.push_back(r);
            }
        }
        return regs;
    }

protected:
    [[nodiscard]] FORCEINLINE psr& cpsr() noexcept { return cpsr_; }
    [[nodiscard]] FORCEINLINE psr& spsr() noexcept { return spsr_banks_[bank_from_privilege_mode(cpsr().mode)]; }

    [[nodiscard]] FORCEINLINE u32& sp() noexcept { return r_[13_u32]; }
    [[nodiscard]] FORCEINLINE u32 sp() const noexcept { return r_[13_u32]; }
    [[nodiscard]] FORCEINLINE u32& lr() noexcept { return r_[14_u32]; }
    [[nodiscard]] FORCEINLINE u32 lr() const noexcept { return r_[14_u32]; }
    [[nodiscard]] FORCEINLINE u32& pc() noexcept { return r_[15_u32]; }
    [[nodiscard]] FORCEINLINE u32 pc() const noexcept { return r_[15_u32]; }

    [[nodiscard]] FORCEINLINE bool interrupt_available() const noexcept { return (if_ & ie_) != 0_u32; }

    void switch_mode(privilege_mode mode) noexcept;
    void schedule_update_irq_signal() noexcept;

private:
    template<instruction_mode Mode>
    void pipeline_flush() noexcept
    {
        if constexpr(Mode == instruction_mode::arm) {
            pipeline_.executing = bus_->read_32(pc(), mem_access::non_seq);
            pipeline_.decoding = bus_->read_32(pc() + 4_u32, mem_access::seq);
            pipeline_.fetch_type = mem_access::seq;
            pc() += 8_u32;
        } else {
            pipeline_.executing = bus_->read_16(pc(), mem_access::non_seq);
            pipeline_.decoding = bus_->read_16(pc() + 2_u32, mem_access::seq);
            pipeline_.fetch_type = mem_access::seq;
            pc() += 4_u32;
        }
    }

    // alu helpers
    enum class barrel_shift_type { lsl, lsr, asr, ror };

    static void alu_barrel_shift(barrel_shift_type shift_type, u32& operand,
      u8 shift_amount, bool& carry, bool imm) noexcept;
    static void alu_lsl(u32& operand, u8 shift_amount, bool& carry) noexcept;
    static void alu_lsr(u32& operand, u8 shift_amount, bool& carry, bool imm) noexcept;
    static void alu_asr(u32& operand, u8 shift_amount, bool& carry, bool imm) noexcept;
    static void alu_ror(u32& operand, u8 shift_amount, bool& carry, bool imm) noexcept;

    u32 alu_add(u32 first_op, u32 second_op, bool set_flags) noexcept;
    u32 alu_adc(u32 first_op, u32 second_op, bool set_flags) noexcept;
    u32 alu_sub(u32 first_op, u32 second_op, bool set_flags) noexcept;
    u32 alu_sbc(u32 first_op, u32 second_op, bool set_flags) noexcept;

    template<typename RsPredicate>
    void alu_multiply_internal(const u32 rs, RsPredicate&& rs_predicate) noexcept
    {
        u32 mask = 0xFFFF'FF00_u32;

        bus_->idle();
        for(u32 i = 0_u32; i < 3_u32; ++i, mask <<= 8_u32) {
            const u32 result = rs & mask;
            if(rs_predicate(result, mask)) {
                break;
            }
            bus_->idle();
        }
    }

    static constexpr lookup_table<function_ptr<arm7tdmi, void(u32)>, 12_u32, 17_u32> arm_table_{
      {"000xxxxxxxx0", function_ptr{&arm7tdmi::data_processing_imm_shifted_reg}},
      {"000xxxxx0xx1", function_ptr{&arm7tdmi::data_processing_reg_shifted_reg}},
      {"000xx0xx1xx1", function_ptr{&arm7tdmi::halfword_data_transfer_reg}},
      {"000xx1xx1xx1", function_ptr{&arm7tdmi::halfword_data_transfer_imm}},
      {"00001xxx1001", function_ptr{&arm7tdmi::multiply_long}},
      {"000000xx1001", function_ptr{&arm7tdmi::multiply}},
      {"00010xx00000", function_ptr{&arm7tdmi::psr_transfer_reg}},
      {"00010x001001", function_ptr{&arm7tdmi::single_data_swap}},
      {"000100100001", function_ptr{&arm7tdmi::branch_exchange}},
      {"001xxxxxxxxx", function_ptr{&arm7tdmi::data_processing_imm}},
      {"00110x10xxxx", function_ptr{&arm7tdmi::psr_transfer_imm}},
      {"010xxxxxxxxx", function_ptr{&arm7tdmi::single_data_transfer}},
      {"011xxxxxxxx0", function_ptr{&arm7tdmi::single_data_transfer}},
      {"011xxxxxxxx1", function_ptr{&arm7tdmi::undefined}},
      {"100xxxxxxxxx", function_ptr{&arm7tdmi::block_data_transfer}},
      {"101xxxxxxxxx", function_ptr{&arm7tdmi::branch_with_link}},
      {"1111xxxxxxxx", function_ptr{&arm7tdmi::swi_arm}},
    };

    static constexpr lookup_table<function_ptr<arm7tdmi, void(u16)>, 10_u32, 19_u32> thumb_table_{
      {"000xxxxxxx", function_ptr{&arm7tdmi::move_shifted_reg}},
      {"00011xxxxx", function_ptr{&arm7tdmi::add_subtract}},
      {"001xxxxxxx", function_ptr{&arm7tdmi::mov_cmp_add_sub_imm}},
      {"010000xxxx", function_ptr{&arm7tdmi::alu}},
      {"010001xxxx", function_ptr{&arm7tdmi::hireg_bx}},
      {"01001xxxxx", function_ptr{&arm7tdmi::pc_rel_load}},
      {"0101xx0xxx", function_ptr{&arm7tdmi::ld_str_reg}},
      {"0101xx1xxx", function_ptr{&arm7tdmi::ld_str_sign_extended_byte_hword}},
      {"011xxxxxxx", function_ptr{&arm7tdmi::ld_str_imm}},
      {"1000xxxxxx", function_ptr{&arm7tdmi::ld_str_hword}},
      {"1001xxxxxx", function_ptr{&arm7tdmi::ld_str_sp_relative}},
      {"1010xxxxxx", function_ptr{&arm7tdmi::ld_addr}},
      {"1011x10xxx", function_ptr{&arm7tdmi::push_pop}},
      {"10110000xx", function_ptr{&arm7tdmi::add_offset_to_sp}},
      {"1100xxxxxx", function_ptr{&arm7tdmi::ld_str_multiple}},
      {"1101xxxxxx", function_ptr{&arm7tdmi::branch_cond}},
      {"11011111xx", function_ptr{&arm7tdmi::swi_thumb}},
      {"11100xxxxx", function_ptr{&arm7tdmi::branch}},
      {"1111xxxxxx", function_ptr{&arm7tdmi::long_branch_link}},
    };
};

} // namespace gba::cpu

#endif //GAMEBOIADVANCE_ARM7TDMI_H
