/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM7TDMI_H
#define GAMEBOIADVANCE_ARM7TDMI_H

#include <gba/core/container.h>
#include <gba/core/math.h>
#include <gba/helper/lookup_table.h>
#include <gba/helper/function_ptr.h>

namespace gba {

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
        return u32(static_cast<u32::type>(mode))
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
        mode = static_cast<privilege_mode>((data & 0x1F_u32).get());
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

/*
 * Banks the mode registers as shown belown. FIQ is not included.
 *
 * System/User FIQ       Supervisor Abort     IRQ       Undefined
 * --------------------------------------------------------------
 * R0          R0        R0         R0        R0        R0
 * R1          R1        R1         R1        R1        R1
 * R2          R2        R2         R2        R2        R2
 * R3          R3        R3         R3        R3        R3
 * R4          R4        R4         R4        R4        R4
 * R5          R5        R5         R5        R5        R5
 * R6          R6        R6         R6        R6        R6
 * R7          R7        R7         R7        R7        R7
 * --------------------------------------------------------------
 * R8          R8_fiq    R8         R8        R8        R8
 * R9          R9_fiq    R9         R9        R9        R9
 * R10         R10_fiq   R10        R10       R10       R10
 * R11         R11_fiq   R11        R11       R11       R11
 * R12         R12_fiq   R12        R12       R12       R12
 * R13 (SP)    R13_fiq   R13_svc    R13_abt   R13_irq   R13_und
 * R14 (LR)    R14_fiq   R14_svc    R14_abt   R14_irq   R14_und
 * R15 (PC)    R15       R15        R15       R15       R15
 * --------------------------------------------------------------
 * CPSR        CPSR      CPSR       CPSR      CPSR      CPSR
 * --          SPSR_fiq  SPSR_svc   SPSR_abt  SPSR_irq  SPSR_und
 * --------------------------------------------------------------
 */
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

class arm7tdmi {
   enum class instruction_mode { arm, thumb };
   enum class mem_access { non_seq, seq };

    vector<u8> bios_{16_kb};
    vector<u8> wram_{256_kb};
    vector<u8> iwram_{32_kb};

    /*
     * Reading from BIOS Memory (00000000-00003FFF)
     * The BIOS memory is protected against reading, the GBA allows to read opcodes or data only if the program counter is
     * located inside of the BIOS area. If the program counter is not in the BIOS area, reading will
     * return the most recent successfully fetched BIOS opcode (eg. the opcode at [00DCh+8] after startup
     * and SoftReset, the opcode at [0134h+8] during IRQ execution, and opcode at [013Ch+8] after IRQ execution,
     * and opcode at [0188h+8] after SWI execution).
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

    u16 ie_;
    u16 if_;
    bool ime_ = false;

    struct {
        mem_access fetch_type;
        u32 executing;
        u32 decoding;
    } pipeline_;

public:
    arm7tdmi() = default;

    void tick() noexcept;

    u32& r(u8 index) noexcept;
    psr& cpsr() noexcept { return cpsr_; }
    psr& spsr() noexcept;

private:
    [[nodiscard]] u32 read_32_aligned(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_32(u32 addr, mem_access access) noexcept;
    void write_32(u32 addr, u32 data, mem_access access) noexcept;

    [[nodiscard]] u32 read_16_signed(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_16_aligned(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_16(u32 addr, mem_access access) noexcept;
    void write_16(u32 addr, u16 data, mem_access access) noexcept;

    [[nodiscard]] u32 read_8_signed(u32 addr, mem_access access) noexcept;
    [[nodiscard]] u32 read_8(u32 addr, mem_access access) noexcept;
    void write_8(u32 addr, u8 data, mem_access access) noexcept;

    void tick_internal() noexcept { /*todo used in internal cycles*/ }

    [[nodiscard]] bool in_privileged_mode() const noexcept { return cpsr_.mode != privilege_mode::usr; }
    [[nodiscard]] bool in_exception_mode() const noexcept { return in_privileged_mode() && cpsr_.mode != privilege_mode::sys; }

    [[nodiscard]] bool condition_met(u32 instruction) const noexcept;

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

    // ARM instructions
    void data_processing_imm_shifted_reg(u32 instr) noexcept;
    void data_processing_reg_shifted_reg(u32 instr) noexcept;
    void data_processing_imm(u32 instr) noexcept;
    void data_processing(u32 instr, u32 second_op, bool carry) noexcept;
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

    // alu helpers
    enum class barrel_shift_type { lsl, lsr, asr, ror };

    static void alu_barrel_shift(barrel_shift_type shift_type, u32& operand,
      u8 shift_amount, bool& carry, bool imm) noexcept;
    static void alu_lsl(u32& operand, u8 shift_amount, bool& carry) noexcept;
    static void alu_lsr(u32& operand, u8 shift_amount, bool& carry, bool imm) noexcept;
    static void alu_asr(u32& operand, u8 shift_amount, bool& carry, bool imm) noexcept;
    static void alu_ror(u32& operand, u8 shift_amount, bool& carry, bool imm) noexcept;

    u32 alu_add(u32 first_op, u32 second_op, bool set_flags) noexcept;
    u32 alu_adc(u32 first_op, u32 second_op, u32 carry, bool set_flags) noexcept;
    u32 alu_sub(u32 first_op, u32 second_op, bool set_flags) noexcept;
    u32 alu_sbc(u32 first_op, u32 second_op, u32 borrow, bool set_flags) noexcept;

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
      {"001xxxxxxxxx", function_ptr{&arm7tdmi::data_processing_imm}},
      {"000100100001", function_ptr{&arm7tdmi::branch_exchange}},
      {"000xx0xx1xx1", function_ptr{&arm7tdmi::halfword_data_transfer_reg}},
      {"000xx1xx1xx1", function_ptr{&arm7tdmi::halfword_data_transfer_imm}},
      {"00110x10xxxx", function_ptr{&arm7tdmi::psr_transfer_imm}},
      {"00010xx00000", function_ptr{&arm7tdmi::psr_transfer_reg}},
      {"000000xx1001", function_ptr{&arm7tdmi::multiply}},
      {"00001xxx1001", function_ptr{&arm7tdmi::multiply_long}},
      {"00010x001001", function_ptr{&arm7tdmi::single_data_swap}},
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
      {"10110000xx", function_ptr{&arm7tdmi::add_offset_to_sp}},
      {"1011x10xxx", function_ptr{&arm7tdmi::push_pop}},
      {"1100xxxxxx", function_ptr{&arm7tdmi::ld_str_multiple}},
      {"1101xxxxxx", function_ptr{&arm7tdmi::branch_cond}},
      {"11011111xx", function_ptr{&arm7tdmi::swi_thumb}},
      {"11100xxxxx", function_ptr{&arm7tdmi::branch}},
      {"1111xxxxxx", function_ptr{&arm7tdmi::long_branch_link}},
    };
};

} // namespace gba

#endif //GAMEBOIADVANCE_ARM7TDMI_H
