/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba::arm {

namespace {

FORCEINLINE u32 addresing_offset(const bool add_to_base, const u32 rn, const u32 offset) noexcept
{
    if(add_to_base) {
        return rn + offset;
    }
    return rn - offset;
}

} // namespace

void arm7tdmi::data_processing_imm_shifted_reg(const u32 instr) noexcept
{
    u32 reg_op = r_[instr & 0xF_u32];
    const u32 shift_type = (instr >> 5_u32) & 0b11_u32;
    const u8 shift_amount = narrow<u8>((instr >> 7_u32) & 0x1F_u32);
    bool carry = cpsr().c;

    alu_barrel_shift(static_cast<barrel_shift_type>(shift_type.get()), reg_op, shift_amount, carry, true);
    data_processing(instr, r_[(instr >> 16_u8) & 0xF_u8], reg_op, carry);
}

void arm7tdmi::data_processing_reg_shifted_reg(const u32 instr) noexcept
{
    const u32 rm = instr & 0xF_u32;
    u32 reg_op = r_[rm];
    if(rm == 15_u32) {
        reg_op += 4_u32;
    }

    const u32 rn =(instr >> 16_u32) & 0xF_u32;
    u32 first_op = r_[rn];
    if(rn == 15_u32) {
        first_op += 4_u32;
    }

    const u32 shift_type = (instr >> 5_u32) & 0b11_u32;
    const u8 shift_amount = narrow<u8>(r_[(instr >> 8_u32) & 0xF_u32]);
    bool carry = cpsr().c;

    tick_internal();

    alu_barrel_shift(static_cast<barrel_shift_type>(shift_type.get()), reg_op, shift_amount, carry, false);
    data_processing(instr, first_op, reg_op, carry);
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

    data_processing(instr, r_[(instr >> 16_u8) & 0xF_u8], imm_op, carry);
}

void arm7tdmi::data_processing(const u32 instr, const u32 first_op, const u32 second_op, const bool carry) noexcept
{
    const bool set_flags = bit::test(instr, 20_u8);
    const u32 opcode = (instr >> 21_u8) & 0xF_u8;

    const u32 dest = (instr >> 12_u8) & 0xF_u8;
    u32& rd = r_[dest];

    const auto do_set_flags = [&](const u32 expression) {
        cpsr().n = bit::test(expression, 31_u8);
        cpsr().z = expression == 0_u32;
        cpsr().c = carry;
    };

    const auto evaluate_and_set_flags = [&](const u32 expression) {
        if(set_flags) {
            do_set_flags(expression);
        }
        return expression;
    };

    switch(opcode.get()) {
        case 0x0: // AND
            rd = evaluate_and_set_flags(first_op & second_op);
            break;
        case 0x1: // EOR
            rd = evaluate_and_set_flags(first_op ^ second_op);
            break;
        case 0x2: // SUB
            rd = alu_sub(first_op, second_op, set_flags);
            break;
        case 0x3: // RSB
            rd = alu_sub(second_op, first_op, set_flags);
            break;
        case 0x4: // ADD
            rd = alu_add(first_op, second_op, set_flags);
            break;
        case 0x5: // ADDC
            rd = alu_adc(first_op, second_op, set_flags);
            break;
        case 0x6: // SBC
            rd = alu_sbc(first_op, second_op, set_flags);
            break;
        case 0x7: // RSC
            rd = alu_sbc(second_op, first_op, set_flags);
            break;
        case 0x8: { // TST
            const u32 result = first_op & second_op;
            do_set_flags(result);
            break;
        }
        case 0x9: { // TEQ
            const u32 result = first_op ^ second_op;
            do_set_flags(result);
            break;
        }
        case 0xA: // CMP
            alu_sub(first_op, second_op, true);
            break;
        case 0xB: // CMN
            alu_add(first_op, second_op, true);
            break;
        case 0xC: // ORR
            rd = evaluate_and_set_flags(first_op | second_op);
            break;
        case 0xD: // MOV
            rd = evaluate_and_set_flags(second_op);
            break;
        case 0xE: // BIC
            rd = evaluate_and_set_flags(first_op & ~second_op);
            break;
        case 0xF: // MVN
            rd = evaluate_and_set_flags(~second_op);
            break;
        default:
            UNREACHABLE();
    }

    pipeline_.fetch_type = mem_access::seq;
    if(dest == 15_u32) {
        if(set_flags && in_exception_mode()) {
            cpsr().copy_without_mode(spsr());
            switch_mode(spsr().mode);
        }

        if((opcode < 0x8_u32 /*TST*/ || 0xB_u32 /*CMN*/ < opcode)) {
            if(cpsr().t) {
                pipeline_flush<instruction_mode::thumb>();
            } else {
                pipeline_flush<instruction_mode::arm>();
            }
        }
    } else {
        pc() += 4_u32;
    }
}

void arm7tdmi::branch_exchange(const u32 instr) noexcept
{
    const u32 addr = r_[instr & 0xF_u32];
    if(bit::test(addr, 0_u8)) {
        pc() = bit::clear(addr, 0_u8);
        cpsr().t = true;
        pipeline_flush<instruction_mode::thumb>();
    } else {
        pc() = mask::clear(addr, 0b11_u32);
        pipeline_flush<instruction_mode::arm>();
    }
}

void arm7tdmi::halfword_data_transfer_reg(const u32 instr) noexcept
{
    const u32 rm = instr & 0xF_u32;
    ASSERT(rm != 15_u32);
    halfword_data_transfer(instr, r_[rm]);
}

void arm7tdmi::halfword_data_transfer_imm(const u32 instr) noexcept
{
    halfword_data_transfer(instr, ((instr >> 4_u32) & 0xF0_u32) | (instr & 0xF_u32));
}

void arm7tdmi::halfword_data_transfer(const u32 instr, const u32 offset) noexcept
{
    const bool pre_indexing = bit::test(instr, 24_u8);
    const bool add_to_base = bit::test(instr, 23_u8);
    const bool write_back = bit::test(instr, 21_u8);
    const bool is_ldr = bit::test(instr, 20_u8);
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;

    u32 rn_addr = r_[rn];

    if(pre_indexing) {
        rn_addr = addresing_offset(add_to_base, rn_addr, offset);
    }

    if(is_ldr) {
        switch(((instr >> 5_u32) & 0b11_u32).get()) {
            case 1: { // LDRH
                r_[rd] = read_16_aligned(rn_addr, mem_access::non_seq);
                break;
            }
            case 2: { // LDRSB
                r_[rd] = read_8_signed(rn_addr, mem_access::non_seq);
                break;
            }
            case 3: { // LDRSH
                r_[rd] = read_16_signed(rn_addr, mem_access::non_seq);
                break;
            }
            default:
                UNREACHABLE();
        }

        tick_internal();
    } else {
        // STRH
        ASSERT(((instr >> 5_u32) & 0b11_u32) == 1_u32);

        u32 src = r_[rd];
        if(rd == 15_u8) {
            src += 4_u32;
        }

        write_16(rn_addr, narrow<u16>(src), mem_access::non_seq);
    }

    if(!is_ldr || rn != rd) {
        if(!pre_indexing) {
            rn_addr = addresing_offset(add_to_base, rn_addr, offset);
            r_[rn] = rn_addr;
        } else if(write_back) {
            r_[rn] = rn_addr;
        }
    }

    if(is_ldr && rd == 15_u32) {
        pipeline_flush<instruction_mode::arm>();
    } else {
        pipeline_.fetch_type = mem_access::non_seq;
        pc() += 4_u32;
    }
}

void arm7tdmi::psr_transfer_reg(const u32 instr) noexcept
{
    const bool use_spsr = bit::test(instr, 22_u8);
    switch(bit::extract(instr, 21_u8).get()) {
        case 0: { // MRS
            const u32 rd = (instr >> 12_u32) & 0xF_u32;
            ASSERT(rd != 15_u32);

            if(use_spsr && in_exception_mode()) {
                r_[rd] = static_cast<u32>(spsr());
            } else {
                r_[rd] = static_cast<u32>(cpsr());
            }
            break;
        }
        case 1: { // MSR
            const u32 rm = instr & 0xF_u32;
            ASSERT(rm != 15_u32);
            psr_transfer_msr(instr, r_[rm], use_spsr);
            break;
        }
        default:
            UNREACHABLE();
    }

    pipeline_.fetch_type = mem_access::seq;
    pc() += 4_u32;
}

void arm7tdmi::psr_transfer_imm(const u32 instr) noexcept
{
    const bool use_spsr = bit::test(instr, 22_u8);
    const u32 imm = math::logical_rotate_right(instr & 0xFF_u32, narrow<u8>((instr >> 8_u32) & 0xF_u32) << 1_u8).result;
    psr_transfer_msr(instr, imm, use_spsr);

    pipeline_.fetch_type = mem_access::seq;
    pc() += 4_u32;
}

void arm7tdmi::psr_transfer_msr(const u32 instr, const u32 operand, const bool use_spsr) noexcept
{
    u32 mask;
    if(bit::test(instr, 19_u8)) { mask |= 0xF000'0000_u32; }
    if(bit::test(instr, 16_u8) && (use_spsr || in_privileged_mode())) { mask |= 0x0000'00FF_u32; }

    if(use_spsr) {
        if(in_exception_mode()) {
            psr& reg = spsr();
            reg = mask::clear(static_cast<u32>(reg), mask) | (operand & mask);
        }
    } else {
        if((operand & 0xFF_u32) != 0x00_u32) {
            switch_mode(to_enum<privilege_mode>(operand & 0x1F_u32));
        }
        const u32 new_cpsr = mask::clear(static_cast<u32>(cpsr()), mask) | (operand & mask);
        cpsr() = new_cpsr;
    }
}

void arm7tdmi::multiply(const u32 instr) noexcept
{
    u32& rd = r_[(instr >> 16_u32) & 0xF_u32];
    const u32 rs = r_[(instr >> 8_u32) & 0xF_u32];
    const u32 rm = r_[instr & 0xF_u32];

    ASSERT(((instr >> 16_u32) & 0xF_u32) != 15_u32);
    ASSERT(((instr >> 8_u32) & 0xF_u32) != 15_u32);
    ASSERT((instr & 0xF_u32) != 15_u32);

    alu_multiply_internal(rs, [](const u32 r, const u32 mask) {
        return r == 0_u32 || r == mask;
    });

    u32 result = rm * rs;

    if(bit::test(instr, 21_u8)) {
        ASSERT(((instr >> 12_u32) & 0xF_u32) != 15_u32);
        result += r_[(instr >> 12_u32) & 0xF_u32];
        tick_internal();
    }

    if(bit::test(instr, 20_u8)) {
        cpsr().z = result == 0_u32;
        cpsr().n = bit::test(result, 31_u8);
    }

    rd = result;
    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u32;
}

void arm7tdmi::multiply_long(const u32 instr) noexcept
{
    u32& rdhi = r_[(instr >> 16_u32) & 0xF_u32];
    u32& rdlo = r_[(instr >> 12_u32) & 0xF_u32];
    const u32 rs = r_[(instr >> 8_u32) & 0xF_u32];
    const u32 rm = r_[instr & 0xF_u32];

    ASSERT(
      ((instr >> 16_u32) & 0xF_u32) != (instr & 0xF_u32) &&
      ((instr >> 12_u32) & 0xF_u32) != (instr & 0xF_u32) &&
      ((instr >> 12_u32) & 0xF_u32) != ((instr >> 16_u32) & 0xF_u32));
    ASSERT(((instr >> 16_u32) & 0xF_u32) != 15_u32);
    ASSERT(((instr >> 12_u32) & 0xF_u32) != 15_u32);
    ASSERT(((instr >> 8_u32) & 0xF_u32) != 15_u32);
    ASSERT((instr & 0xF_u32) != 15_u32);

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
    pc() += 4_u32;
}

void arm7tdmi::single_data_swap(const u32 instr) noexcept
{
    const u32 rm = instr & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rn_addr = r_[rn];

    ASSERT(rm != 15_u32);
    ASSERT(rd != 15_u32);
    ASSERT(rn != 15_u32);

    u32 data;
    if(bit::test(instr, 22_u8)) {  // byte
        data = read_8(rn_addr, mem_access::non_seq);
        write_8(rn_addr, narrow<u8>(r_[rm]), mem_access::non_seq);
    } else {  // word
        data = read_32_aligned(rn_addr, mem_access::non_seq);
        write_32(rn_addr, r_[rm], mem_access::non_seq);
    }

    r_[rd] = data;
    tick_internal();

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u8;
}

void arm7tdmi::single_data_transfer(const u32 instr) noexcept
{
    const bool pre_indexing = bit::test(instr, 24_u8);
    const bool add_to_base = bit::test(instr, 23_u8);
    const bool transfer_byte = bit::test(instr, 22_u8);
    const bool write_back = bit::test(instr, 21_u8);
    const bool is_ldr = bit::test(instr, 20_u8);
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;

    u32 rn_addr = r_[rn];

    const u32 offset = [=]() {
        // reg
        if(bit::test(instr, 25_u8)) {
            const auto shift_type = static_cast<barrel_shift_type>(((instr >> 5_u32) & 0b11_u32).get());
            const u8 shift_amount = narrow<u8>((instr >> 7_u32) & 0x1F_u32);
            u32 rm = r_[instr & 0xF_u32];
            bool dummy = cpsr().c;
            alu_barrel_shift(shift_type, rm, shift_amount, dummy, true);
            return rm;
        }
        // imm
        return instr & 0xFFF_u32;
    }();

    if(pre_indexing) {
        rn_addr = addresing_offset(add_to_base, rn_addr, offset);
    }

    if(is_ldr) {
        if(transfer_byte) {
            r_[rd] = read_8(rn_addr, mem_access::non_seq);
        } else {
            r_[rd] = read_32_aligned(rn_addr, mem_access::non_seq);
        }

        tick_internal();
    } else {
        u32 src = r_[rd];
        if(rd == 15_u8) {
            src += 4_u32;
        }

        if(transfer_byte) {
            write_8(rn_addr, narrow<u8>(src), mem_access::non_seq);
        } else {
            write_32(rn_addr, src, mem_access::non_seq);
        }
    }

    // write back
    if(!is_ldr || rn != rd) {
        if(!pre_indexing) {
            r_[rn] = addresing_offset(add_to_base, rn_addr, offset);
        } else if(write_back) {
            r_[rn] = rn_addr;
        }
    }

    if(is_ldr && rd == 15_u8) {
        pipeline_flush<instruction_mode::arm>();
    } else {
        pipeline_.fetch_type = mem_access::non_seq;
        pc() += 4_u32;
    }
}

void arm7tdmi::undefined(const u32 /*instr*/) noexcept
{
    spsr_banks_[register_bank::und] = cpsr();
    switch_mode(privilege_mode::und);
    cpsr().i = true;
    lr() = pc() - 4_u32;
    pc() = 0x0000'0004_u32;
    pipeline_flush<instruction_mode::arm>();
}

void arm7tdmi::block_data_transfer(const u32 instr) noexcept
{
    bool pre_indexing = bit::test(instr, 24_u8);
    const bool add_to_base = bit::test(instr, 23_u8);
    const bool load_psr = bit::test(instr, 22_u8);
    const bool write_back = bit::test(instr, 21_u8);
    const bool is_ldm = bit::test(instr, 20_u8);
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    ASSERT(rn != 15_u32);

    bool transfer_pc = bit::test(instr, 15_u8);
    auto rlist = generate_register_list<16>(instr);
    u32 offset = narrow<u32>(rlist.size()) * 4_u32;

    const bool should_switch_mode = load_psr && (!is_ldm || !transfer_pc);

    // Empty Rlist: R15 loaded/stored, and Rb=Rb+/-40h.
    if(rlist.empty()) {
        rlist.push_back(15_u8);
        offset = 0x40_u32;
        transfer_pc = true;
    }

    u32 rn_addr = r_[rn];
    u32 rn_addr_old = rn_addr;
    u32 rn_addr_new = rn_addr;

    privilege_mode old_mode;
    if(should_switch_mode) {
        old_mode = cpsr().mode;
        switch_mode(privilege_mode::usr);
    }

    // for DECREASING addressing modes, the CPU does first calculate the lowest address,
    // and does then process rlist with increasing addresses
    if(!add_to_base) {
        pre_indexing = !pre_indexing;
        rn_addr -= offset;
        rn_addr_new -= offset;
    } else {
        rn_addr_new += offset;
    }

    auto access_type = mem_access::non_seq;
    for(const u32 reg : rlist) {
        if(pre_indexing) {
            rn_addr += 4_u32;
        }

        if(is_ldm) {
            r_[reg] = read_32(rn_addr, access_type);
            if(reg == 15_u32 && load_psr && in_exception_mode()) {
                cpsr().copy_without_mode(spsr());
                switch_mode(spsr().mode);
            }
        } else if(reg == rn) {
            write_32(rn_addr, rlist[0_usize] == rn ? rn_addr_old : rn_addr_new, access_type);
        } else if(reg == 15_u32) {
            write_32(rn_addr, pc() + 4_u32, access_type);
        } else {
            write_32(rn_addr, r_[reg], access_type);
        }

        if(!pre_indexing) {
            rn_addr += 4_u32;
        }

        access_type = mem_access::seq;
    }

    if(should_switch_mode) {
        switch_mode(old_mode);
    }

    if(write_back && (!is_ldm || !bit::test(instr, narrow<u8>(rn)))) {
        r_[rn] = rn_addr_new;
    }

    if(is_ldm) {
        tick_internal();
    }

    if(is_ldm && transfer_pc) {
        if(cpsr().t) {
            pipeline_flush<instruction_mode::thumb>();
        } else {
            pipeline_flush<instruction_mode::arm>();
        }
    } else {
        pipeline_.fetch_type = mem_access::non_seq;
        pc() += 4_u32;
    }
}

void arm7tdmi::branch_with_link(const u32 instr) noexcept
{
    // link
    if(bit::test(instr, 24_u8)) {
        lr() = pc() - 4_u32;
    }

    pc() += math::sign_extend<26>((instr & 0x00FF'FFFF_u32) << 2_u32);
    pipeline_flush<instruction_mode::arm>();
}

void arm7tdmi::swi_arm(const u32 /*instr*/) noexcept
{
    spsr_banks_[register_bank::svc] = cpsr();
    switch_mode(privilege_mode::svc);
    cpsr().i = true;
    lr() = pc() - 4_u32;
    pc() = 0x0000'0008_u32;
    pipeline_flush<instruction_mode::arm>();
}

} // namespace gba::arm
