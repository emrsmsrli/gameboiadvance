/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba {

void arm7tdmi::data_processing_imm_shifted_reg(const u32 instr) noexcept
{
    u32 reg_op = r(narrow<u8>(instr & 0xF_u32));
    const u32 shift_type = (instr >> 5_u32) & 0b11_u32;
    const u8 shift_amount = narrow<u8>((instr >> 7_u32) & 0x1F_u32);
    bool carry = cpsr().c;

    alu_barrel_shift(static_cast<barrel_shift_type>(shift_type.get()), reg_op, shift_amount, carry, true);
    data_processing(instr, reg_op, carry);
}

void arm7tdmi::data_processing_reg_shifted_reg(const u32 instr) noexcept
{
    u32 reg_op = r(narrow<u8>(instr & 0xF_u32));
    const u32 shift_type = (instr >> 5_u32) & 0b11_u32;
    const u8 shift_amount = narrow<u8>(r(narrow<u8>((instr >> 8_u32) & 0xF_u32)));
    bool carry = cpsr().c;

    tick_internal();

    alu_barrel_shift(static_cast<barrel_shift_type>(shift_type.get()), reg_op, shift_amount, carry, false);
    data_processing(instr, reg_op, carry);
}

void arm7tdmi::data_processing_imm(const u32 instr) noexcept
{
    u32 imm_op = instr & 0xFF_u32;
    bool carry = cpsr().c;

    if(const u8 imm_shift = narrow<u8>((instr >> 8_u32) & 0xF_u32); imm_shift > 0_u8) {
        const auto ror = math::logical_rotate_right(imm_op, imm_shift << 1_u8);
        imm_op = ror.result;
        carry = static_cast<bool>(ror.carry.get());
    }

    data_processing(instr, imm_op, carry);
}

void arm7tdmi::data_processing(const u32 instr, const u32 second_op, const bool carry) noexcept
{
    pipeline_.fetch_type = mem_access::seq;

    const bool set_flags = bit::test(instr, 20_u8);
    const u32 opcode = (instr >> 21_u8) & 0xF_u8;
    const u32 rn = r(narrow<u8>((instr >> 16_u8) & 0xF_u8));

    const u8 dest = narrow<u8>((instr >> 12_u8) & 0xF_u8);
    u32& rd = r(dest);

    const auto evaluate_and_set_flags = [&](const u32 expression) {
        if(set_flags) {
            cpsr().n = bit::test(expression, 31_u8);
            cpsr().z = expression == 0_u32;
            cpsr().c = carry;
        }
        return expression;
    };

    switch(opcode.get()) {
        case 0x0: // AND
            rd = evaluate_and_set_flags(rn & second_op);
            break;
        case 0x1: // EOR
            rd = evaluate_and_set_flags(rn ^ second_op);
            break;
        case 0x2: // SUB
            rd = alu_sub(rn, second_op, set_flags);
            break;
        case 0x3: // RSB
            rd = alu_sub(second_op, rn, set_flags);
            break;
        case 0x4: // ADD
            rd = alu_add(rn, second_op, set_flags);
            break;
        case 0x5: // ADDC
            rd = alu_adc(rn, second_op, bit::from_bool(cpsr().c), set_flags);
            break;
        case 0x6: // SBC
            rd = alu_sbc(rn, second_op, bit::from_bool(!cpsr().c), set_flags);
            break;
        case 0x7: // RSC
            rd = alu_sbc(second_op, rn, bit::from_bool(!cpsr().c), set_flags);
            break;
        case 0x8: { // TST
            const u32 result = rn & second_op;
            cpsr().n = bit::test(result, 31_u8);
            cpsr().z = result == 0_u32;
            cpsr().c = carry;
            break;
        }
        case 0x9: { // TEQ
            const u32 result = rn ^ second_op;
            cpsr().n = bit::test(result, 31_u8);
            cpsr().z = result == 0_u32;
            cpsr().c = carry;
            break;
        }
        case 0xA: // CMP
            alu_sub(rn, second_op, true);
            break;
        case 0xB: // CMN
            alu_add(rn, second_op, true);
            break;
        case 0xC: // ORR
            rd = evaluate_and_set_flags(rn | second_op);
            break;
        case 0xD: // MOV
            rd = evaluate_and_set_flags(second_op);
            break;
        case 0xE: // BIC
            rd = evaluate_and_set_flags(rn & ~second_op);
            break;
        case 0xF: // MVN
            rd = evaluate_and_set_flags(~second_op);
            break;
        default:
            UNREACHABLE();
    }

    // todo insert likely/unlikely
    if((0x8_u32 > opcode || opcode > 0xB_u32) && dest == 15_u8) {
        ASSERT(in_privileged_mode());

        if(set_flags) {
            cpsr() = spsr();
        }

        if(cpsr().t) {
            pipeline_flush<instruction_mode::thumb>();
        } else {
            pipeline_flush<instruction_mode::arm>();
        }
    }
}

void arm7tdmi::branch_exchange(const u32 instr) noexcept
{
    const u32 addr = r(narrow<u8>(instr & 0xF_u32));
    if(bit::test(addr, 0_u8)) {
        r(15_u8) = mask::clear(addr, 0b01_u8);
        cpsr().t = true;
        pipeline_flush<instruction_mode::thumb>();
    } else {
        r(15_u8) = mask::clear(addr, 0b11_u32);
        cpsr().t = false;
        pipeline_flush<instruction_mode::arm>();
    }
}

void arm7tdmi::halfword_data_transfer_reg(const u32 instr) noexcept
{

}

void arm7tdmi::halfword_data_transfer_imm(const u32 instr) noexcept
{

}

void arm7tdmi::psr_transfer_reg(const u32 instr) noexcept
{
    const bool use_spsr = bit::test(instr, 22_u8);
    switch(bit::extract(instr, 21_u8).get()) {
        case 0: { // MRS
            const u8 rd = narrow<u8>((instr >> 12_u32) & 0xF_u32);
            ASSERT(rd != 15_u8);

            if(in_exception_mode() && use_spsr) {
                r(rd) = static_cast<u32>(spsr());
            } else {
                r(rd) = static_cast<u32>(cpsr());
            }
            break;
        }
        case 1: { // MSR
            const u8 rm = narrow<u8>(instr & 0xF_u32);
            ASSERT(rm != 15_u8);
            psr_transfer_msr(instr, r(rm), use_spsr);
            break;
        }
        default:
            UNREACHABLE();
    }

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 4_u32;
}

void arm7tdmi::psr_transfer_imm(const u32 instr) noexcept
{
    const bool use_spsr = bit::test(instr, 22_u8);
    const u32 imm = math::logical_rotate_right(instr & 0xFF_u32, narrow<u8>((instr >> 8_u32) & 0xF_u32)).result;
    psr_transfer_msr(instr, imm, use_spsr);

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 4_u32;
}

void arm7tdmi::psr_transfer_msr(u32 instr, u32 operand, bool use_spsr) noexcept
{
    u32 mask;
    if(bit::test(instr, 19_u8)) { mask |= 0xF000'0000_u32; }
    if(bit::test(instr, 16_u8) && (use_spsr || !in_privileged_mode())) { mask |= 0x0000'00FF_u32; }

    if(use_spsr) {
        auto& reg = spsr();
        if(in_exception_mode()) {
            reg = mask::clear(static_cast<u32>(reg), mask) | operand;
        }
    } else {
        auto& reg = cpsr();
        reg = mask::clear(static_cast<u32>(reg), mask) | operand;
    }
}

void arm7tdmi::multiply(const u32 instr) noexcept
{
    u32& rd = r(narrow<u8>((instr >> 16_u32) & 0xF_u32));
    const u32 rs = r(narrow<u8>((instr >> 8_u32) & 0xF_u32));
    const u32 rm = r(narrow<u8>(instr & 0xF_u32));

    ASSERT(rd != rm);
    ASSERT(rd != r15_);
    ASSERT(rs != r15_);
    ASSERT(rm != r15_);

    alu_multiply_internal(rs, [](const u32 r, const u32 mask) {
        return r == 0_u32 || r == mask;
    });

    u32 result = rm * rs;

    if(bit::test(instr, 21_u8)) {
        const u32 rn = r(narrow<u8>((instr >> 12_u32) & 0xF_u32));
        ASSERT(rn != r15_);

        result += rn;
        tick_internal();
    }

    if(bit::test(instr, 20_u8)) {
        cpsr().z = result == 0_u32;
        cpsr().n = bit::test(result, 31_u8);
    }

    rd = result;
    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 4_u32;
}

void arm7tdmi::multiply_long(const u32 instr) noexcept
{
    u32& rdhi = r(narrow<u8>((instr >> 16_u32) & 0xF_u32));
    u32& rdlo = r(narrow<u8>((instr >> 12_u32) & 0xF_u32));
    const u32 rs = r(narrow<u8>((instr >> 8_u32) & 0xF_u32));
    const u32 rm = r(narrow<u8>(instr & 0xF_u32));

    ASSERT(rdhi != rm && rdlo != rm && rdlo != rdhi);
    ASSERT(rdhi != r15_);
    ASSERT(rdlo != r15_);
    ASSERT(rs != r15_);
    ASSERT(rm != r15_);

    tick_internal();

    i64 result;
    if(bit::test(instr, 22_u8)) { // signed mul
        alu_multiply_internal(rs, [](const u64 r, const u64 mask) {
            return r == 0_u32 || r == mask;
        });

        result = math::sign_extend<32>(widen<u64>(rm)) * math::sign_extend<32>(widen<u64>(rs));
    } else {
        alu_multiply_internal(rs, [](const u32 r, const u32 /*mask*/) {
            return r == 0_u32;
        });

        result = make_signed(widen<u64>(rm) * rs);
    }

    if(bit::test(instr, 21_u8)) {
        result += (widen<u64>(rdhi) << 32_u64) | rdlo;
        tick_internal();
    }

    if(bit::test(instr, 20_u8)) {
        cpsr().z = result == 0_i64;
        cpsr().n = result < 0_i64;
    }

    rdhi = narrow<u32>(make_unsigned(result) >> 32_u64);
    rdlo = narrow<u32>(make_unsigned(result));
    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 4_u32;
}

void arm7tdmi::single_data_swap(const u32 instr) noexcept
{
    const u8 rm = narrow<u8>(instr & 0xF_u8);
    const u8 rd = narrow<u8>((instr >> 12_u8) & 0xF_u8);
    const u8 rn = narrow<u8>((instr >> 16_u8) & 0xF_u8);
    const u32 rn_addr = r(rn);

    ASSERT(rm != 15_u8);
    ASSERT(rd != 15_u8);
    ASSERT(rn != 15_u8);

    u32 data;
    if(bit::test(instr, 22_u8)) {  // byte
        data = read_8(rn_addr, mem_access::non_seq);
        write_8(rn_addr, narrow<u8>(r(rm)), mem_access::non_seq);
    } else {  // word
        data = read_32_rotated(rn_addr, mem_access::non_seq);
        write_32(rn_addr, narrow<u8>(r(rm)), mem_access::non_seq);
    }

    r(rd) = data;
    tick_internal();

    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 4_u8;
}

void arm7tdmi::single_data_transfer(const u32 instr) noexcept
{
    const bool pre_indexing = bit::test(instr, 24_u8);
    const bool add_to_base = bit::test(instr, 23_u8);
    const bool transfer_byte = bit::test(instr, 22_u8);
    const bool write_back = bit::test(instr, 21_u8);
    const bool is_ldr = bit::test(instr, 20_u8);
    const u8 rn = narrow<u8>((instr >> 16_u32) & 0xF_u32);
    const u8 rd = narrow<u8>((instr >> 12_u32) & 0xF_u32);

    u32 rn_addr = r(rn);

    privilege_mode old_mode;
    if(!pre_indexing && write_back) {
        old_mode = cpsr().mode;
        cpsr().mode = privilege_mode::usr;
    }

    const u32 offset = [=]() {
        // reg
        if(bit::test(instr, 25_u8)) {
            const auto shift_type = static_cast<barrel_shift_type>(((instr >> 5_u32) & 0b11_u32).get());
            const u8 shift_amount = narrow<u8>((instr >> 7_u32) & 0x1F_u32);
            u32 rm = r(narrow<u8>(instr & 0xF_u32));
            bool dummy = cpsr().c;
            alu_barrel_shift(shift_type, rm, shift_amount, dummy, true);
            return rm;
        }
        // imm
        return instr & 0xFFF_u32;
    }();

    const auto add_rn_offset = [&]() {
        if(add_to_base) {
            rn_addr += offset;
        } else {
            rn_addr -= offset;
        }
    };

    if(pre_indexing) {
        add_rn_offset();
    }

    if(is_ldr) {
        if(transfer_byte) {
            r(rd) = read_8(rn_addr, mem_access::non_seq);
        } else {
            r(rd) = read_32_rotated(rn_addr, mem_access::non_seq);
        }

        tick_internal();
    } else {
        u32 dest = r(rd);
        if(rd == 15_u8) {
            dest += 4_u32;
        }

        if(transfer_byte) {
            write_8(rn_addr, narrow<u8>(dest), mem_access::non_seq);
        } else {
            write_32(rn_addr, dest, mem_access::non_seq);
        }
    }

    // change mode before, so we don't write back rn to usr regs
    if(!pre_indexing && write_back) {
        cpsr().mode = old_mode;
    }

    // write back
    if(!is_ldr || rn != rd) {
        if(!pre_indexing) {
            add_rn_offset();
            r(rn) = rn_addr;
        } else if(write_back) {
            r(rn) = rn_addr;
        }
    }

    if(is_ldr && rd == 15_u8) {
        pipeline_flush<instruction_mode::arm>();
    } else {
        pipeline_.fetch_type = mem_access::non_seq;
        r(15_u8) += 4_u32;
    }
}

void arm7tdmi::undefined(const u32 /*instr*/) noexcept
{
    und_.r14 = r15_ - 4_u32;
    und_.spsr = cpsr();
    cpsr().mode = privilege_mode::und;
    cpsr().i = true;
    r15_ = 0x0000'0004_u32;
    pipeline_flush<instruction_mode::arm>();
}

void arm7tdmi::block_data_transfer(const u32 instr) noexcept
{
    
}

void arm7tdmi::branch_with_link(const u32 instr) noexcept
{
    u32& pc = r(15_u8);

    // link
    if(bit::test(instr, 24_u8)) {
        r(14_u8) = pc - 4_u32;
    }

    pc += math::sign_extend<26>((instr & 0x00FF'FFFF_u32) << 2_u32);
    pipeline_flush<instruction_mode::arm>();
}

void arm7tdmi::swi_arm(const u32 instr) noexcept
{
    
}

} // namespace gba
