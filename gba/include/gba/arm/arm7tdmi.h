/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM7TDMI_H
#define GAMEBOIADVANCE_ARM7TDMI_H

#include <algorithm>

#include <gba/arm/dma_controller.h>
#include <gba/arm/timer.h>
#include <gba/arm/irq_controller_handle.h>
#include <gba/core/scheduler.h>
#include <gba/core/container.h>
#include <gba/core/math.h>
#include <gba/core/fwd.h>
#include <gba/helper/lookup_table.h>
#include <gba/helper/function_ptr.h>
#include <gba/helper/bitflags.h>

namespace gba::arm {

#if WITH_DEBUGGER
enum class debugger_access_width : u32::type { byte, hword, word, any };
#endif // WITH_DEBUGGER

enum class privilege_mode : u8::type {
    usr = 0x10,  // user
    fiq = 0x11,  // fast interrupt
    irq = 0x12,  // interrupt
    svc = 0x13,  // service
    abt = 0x17,  // abort
    und = 0x1b,  // undefined
    sys = 0x1f,  // system
};

struct psr {
    bool n = false;  // signed flag
    bool z = false;  // zero flag
    bool c = false;  // carry flag
    bool v = false;  // overflow flag
    bool i = false;  // irq disabled flag
    bool f = false;  // fiq disabled flag
    bool t = false;  // thumb mode flag
    privilege_mode mode{privilege_mode::sys};

    explicit operator u32() const noexcept
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

    psr& operator=(const u32 data) noexcept
    {
        mode = to_enum<privilege_mode>(data & 0x1F_u32);
        t = bit::test(data, 5_u8);
        f = bit::test(data, 6_u8);
        i = bit::test(data, 7_u8);

        v = bit::test(data, 28_u8);
        c = bit::test(data, 29_u8);
        z = bit::test(data, 30_u8);
        n = bit::test(data, 31_u8);
        return *this;
    }
};

struct banked_mode_regs {
    u32 r13;
    u32 r14;

    psr spsr;
};

struct banked_fiq_regs {
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
    u32 r13;
    u32 r14;

    psr spsr;
};

struct waitstate_control {
    u8 sram;
    u8 ws0_nonseq;
    u8 ws0_seq;
    u8 ws1_nonseq;
    u8 ws1_seq;
    u8 ws2_nonseq;
    u8 ws2_seq;
    u8 phi;
    bool prefetch_buffer_enable = false;
};

enum class halt_control {
    halted,
    stopped,
    running,
};

enum class instruction_mode { arm, thumb };
enum class mem_access : u32::type {
    none = 0,
    non_seq = 1,
    seq = 2,
    pak_prefetch = 4, // todo implement gamepak fetch with this
    dma = 8,
    dry_run = 16
};

struct pipeline {
    mem_access fetch_type{mem_access::non_seq};
    u32 executing;
    u32 decoding;
};

class arm7tdmi {
    friend timer;
    friend dma::controller;

    core* core_;

    vector<u8> bios_{16_kb};
    vector<u8> wram_{256_kb};
    vector<u8> iwram_{32_kb};

    /*
     * The BIOS memory is protected against reading,
     * the GBA allows to read opcodes or data only if the program counter is
     * located inside of the BIOS area. If the program counter is not in the BIOS area,
     * reading will return the most recent successfully fetched BIOS opcode.
     */
    u32 bios_last_read_;

    u32 r0_; u32 r1_;
    u32 r2_; u32 r3_;
    u32 r4_; u32 r5_;
    u32 r6_; u32 r7_;
    u32 r8_; u32 r9_;
    u32 r10_; u32 r11_;
    u32 r12_; u32 r13_;
    u32 r14_; u32 r15_;

    psr cpsr_;

    banked_fiq_regs fiq_;
    banked_mode_regs svc_;
    banked_mode_regs abt_;
    banked_mode_regs irq_;
    banked_mode_regs und_;

    u8 post_boot_;
    halt_control haltcnt_{halt_control::running};
    u16 ie_;
    u16 if_;
    bool ime_ = false;
    bool irq_signal_ = false;
    bool scheduled_irq_signal_ = false;
    scheduler::event::handle irq_signal_delay_handle_;

    array<timer, 4> timers_;
    dma::controller dma_controller_{this};

    pipeline pipeline_;

    waitstate_control waitcnt_;
    array<u8, 32> wait_16{ // cycle counts for 16bit r/w, nonseq and seq access
      1_u8, 1_u8, 3_u8, 1_u8, 1_u8, 1_u8, 1_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8,
      1_u8, 1_u8, 3_u8, 1_u8, 1_u8, 1_u8, 1_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8
    };
    array<u8, 32> wait_32{ // cycle counts for 32bit r/w, nonseq and seq access
      1_u8, 1_u8, 6_u8, 1_u8, 1_u8, 2_u8, 2_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8,
      1_u8, 1_u8, 6_u8, 1_u8, 1_u8, 2_u8, 2_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8
    };

public:
#if WITH_DEBUGGER
    using timers_debugger = array<timer, 4>;

    delegate<bool(u32)> on_instruction_execute;
    delegate<void(u32, debugger_access_width)> on_io_read;
    delegate<void(u32, u32, debugger_access_width)> on_io_write;
#endif // WITH_DEBUGGER

    static constexpr u32 clock_speed = 1_u32 << 24_u32; // 16.78 MHz

    explicit arm7tdmi(core* core) noexcept : arm7tdmi(core, {}) {}
    arm7tdmi(core* core, vector<u8> bios) noexcept;

    void tick() noexcept;

    void request_interrupt(const interrupt_source irq) noexcept
    {
        if_ |= from_enum<u16>(irq);
        schedule_update_irq_signal();
    }

    irq_controller_handle get_interrupt_handle() noexcept { return irq_controller_handle{this}; }
    dma::controller_handle get_dma_cnt_handle() noexcept { return dma::controller_handle{&dma_controller_}; }

    u32& r(u8 index) noexcept;
    psr& cpsr() noexcept { return cpsr_; }
    psr& spsr() noexcept;

private:
    [[nodiscard]] u32 read_32_aligned(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_32(u32 addr, mem_access access) noexcept;
    void write_32(u32 addr, u32 data, mem_access access) noexcept;

    [[nodiscard]] u32 read_16_signed(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_16_aligned(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u16 read_16(u32 addr, mem_access access) noexcept;
    void write_16(u32 addr, u16 data, mem_access access) noexcept;

    [[nodiscard]] u32 read_8_signed(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u8 read_8(u32 addr, mem_access access) noexcept;
    void write_8(u32 addr, u8 data, mem_access access) noexcept;

    [[nodiscard]] u32 read_bios(u32 addr) noexcept;
    [[nodiscard]] u32 read_unused(u32 addr, mem_access access) noexcept;

    [[nodiscard]] u8 read_io(u32 addr, mem_access access) noexcept;
    void write_io(u32 addr, u8 data) noexcept;

    void update_waitstate_table() noexcept;

    [[nodiscard]] bool interrupt_available() const noexcept { return (if_ & ie_) != 0_u32; }
    void schedule_update_irq_signal() noexcept;
    void update_irq_signal(u64 /*late_cycles*/) noexcept;
    void process_interrupts() noexcept;
    void tick_internal() noexcept;
    void tick_components(u64 cycles) noexcept;

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

    [[nodiscard]] bool in_privileged_mode() const noexcept { return cpsr_.mode != privilege_mode::usr; }
    [[nodiscard]] bool in_exception_mode() const noexcept { return in_privileged_mode() && cpsr_.mode != privilege_mode::sys; }

    template<usize::type Count>
    static_vector<u8, Count> generate_register_list(const u32 instr) noexcept
    {
        static_vector<u8, Count> regs;
        for(u8 i = 0_u8; i < Count; ++i) {
            if(bit::test(instr, i)) {
                regs.push_back(i);
            }
        }
        return regs;
    }

    template<instruction_mode Mode>
    void pipeline_flush() noexcept
    {
        u32& pc = r(15_u8);
        if constexpr(Mode == instruction_mode::arm) {
            pipeline_.executing = read_32(pc, mem_access::non_seq);
            pipeline_.decoding = read_32(pc + 4_u32, mem_access::seq);
            pipeline_.fetch_type = mem_access::seq;
            pc += 8_u32;
        } else {
            pipeline_.executing = read_16(pc, mem_access::non_seq);
            pipeline_.decoding = read_16(pc + 2_u32, mem_access::seq);
            pipeline_.fetch_type = mem_access::seq;
            pc += 4_u32;
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

        tick_internal();
        for(u32 i = 0_u32; i < 3_u32; ++i, mask <<= 8_u32) {
            const u32 result = rs & mask;
            if(rs_predicate(result, mask)) {
                break;
            }
            tick_internal();
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

} // namespace gba::arm

ENABLE_BITFLAG_OPS(gba::arm::mem_access);

#endif //GAMEBOIADVANCE_ARM7TDMI_H
