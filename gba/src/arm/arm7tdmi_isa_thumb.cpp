/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba {

void arm7tdmi::move_shifted_reg(const u16 instr) noexcept
{
    const u16 opcode = (instr >> 11_u16) & 0b11_u16;
    const u8 offset = narrow<u8>((instr >> 6_u16) & 0x1F_u16);
    u32 rs = r(narrow<u8>((instr >> 3_u16) & 0x7_u16));
    u32& rd = r(narrow<u8>(instr & 0x7_u16));
    bool carry = cpsr().c;

    alu_barrel_shift(static_cast<barrel_shift_type>(opcode.get()), rs, offset, carry, true);
    rd = rs;

    cpsr().z = rs == 0_u32;
    cpsr().n = bit::test(rs, 31_u8);
    cpsr().c = carry;

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::add_subtract(const u16 instr) noexcept
{
    const bool is_sub = bit::test(instr, 9_u8);
    const bool is_imm = bit::test(instr, 10_u8);
    u32 data = (instr >> 6_u16) & 0x7_u16;
    const u32 rs = r(narrow<u8>((instr >> 3_u16) & 0x7_u16));
    u32& rd = r(narrow<u8>(instr & 0x7_u16));

    if(!is_imm) {
        data = r(narrow<u8>(data));
    }

    if(is_sub) {
        rd = alu_sub(rs, data, true);
    } else {
        rd = alu_add(rs, data, true);
    }

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::mov_cmp_add_sub_imm(const u16 instr) noexcept
{
    const u16 opcode = (instr >> 11_u16) & 0b11_u16;
    u32& rd = r(narrow<u8>((instr >> 8_u16) & 0x7_u16));
    const u16 offset = instr & 0xFF_u16;
    switch(opcode.get()) {
        case 0b00: { // MOV
            rd = offset;
            cpsr().n = false;
            cpsr().z = offset == 0_u16;
            break;
        }
        case 0b01: { // CMP
            alu_sub(rd, offset, true);
            break;
        }
        case 0b10: { // ADD
            rd = alu_add(rd, offset, true);
            break;
        }
        case 0b11: { // SUB
            rd = alu_sub(rd, offset, true);
            break;
        }
        default:
            UNREACHABLE();
    }

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::alu(const u16 instr) noexcept
{
    const u16 opcode = (instr >> 6_u16) & 0xF_u16;
    u32 rs = r(narrow<u8>((instr >> 3_u16) & 0x7_u16));
    u32& rd = r(narrow<u8>(instr & 0x7_u16));
    bool carry = cpsr().c;

    pipeline_.fetch_type = mem_access::seq;

    const auto evaluate_and_set_flags = [&](const u32 expression) {
        cpsr().n = bit::test(expression, 31_u8);
        cpsr().z = expression == 0_u32;
        return expression;
    };

    switch(opcode.get()) {
        case 0x0: { // AND
            rd = evaluate_and_set_flags(rd & rs);
            break;
        }
        case 0x1: { // EOR
            rd = evaluate_and_set_flags(rd ^ rs);
            break;
        }
        case 0x2: { // LSL
            alu_lsl(rd, narrow<u8>(rs), carry);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            tick_internal();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        }
        case 0x3: { // LSR
            alu_lsr(rd, narrow<u8>(rs), carry, false);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            tick_internal();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        }
        case 0x4: { // ASR
            alu_asr(rd, narrow<u8>(rs), carry, false);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            tick_internal();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        }
        case 0x5: { // ADC
            rd = alu_adc(rd, rs, bit::from_bool(carry), true);
            break;
        }
        case 0x6: { // SBC
            rd = alu_sbc(rd, rs, bit::from_bool(!carry), true);
            break;
        }
        case 0x7: { // ROR
            alu_ror(rd, narrow<u8>(rs), carry, false);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            tick_internal();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        }
        case 0x8: { // TST
            evaluate_and_set_flags(rd & rs);
            break;
        }
        case 0x9: { // NEG
            rd = alu_sub(0_u32, rs, true);
            break;
        }
        case 0xA: { // CMP
            alu_sub(rd, rs, true);
            break;
        }
        case 0xB: { // CMN
            alu_add(rd, rs, true);
            break;
        }
        case 0xC: { // ORR
            rd = evaluate_and_set_flags(rd | rs);
            break;
        }
        case 0xD: { // MUL
            alu_multiply_internal(rd, [](const u32 r, const u32 mask) {
                return r == 0_u32 || r == mask;
            });
            rd = evaluate_and_set_flags(rd * rs);
            cpsr().c = false;
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        }
        case 0xE: { // BIC
            rd = evaluate_and_set_flags(rd & ~rs);
            break;
        }
        case 0xF: { // MVN
            rd = evaluate_and_set_flags(~rs);
            break;
        }
        default:
            UNREACHABLE();
    }

    r(15_u8) += 2_u32;
}

void arm7tdmi::hireg_bx(const u16 instr) noexcept
{
    const u16 opcode = (instr >> 8_u16) & 0b11_u16;
    const u8 rs_reg = narrow<u8>((instr >> 3_u16) & 0xF_u16);
    u32 rs = r(rs_reg);
    u32& rd = r(narrow<u8>((instr & 0x7_u16) | ((instr >> 4_u16) & 0x8_u16)));

    if(rs_reg == 15_u8) {
        rs = bit::clear(rs, 0_u8);
    }

    const auto set_rd_and_flush = [&](const u32 expression) {
        rd = expression;
        if(rd == 15_u8) {
            rd = bit::clear(rd, 0_u8);
            pipeline_flush<instruction_mode::thumb>();
        } else {
            pipeline_.fetch_type = mem_access::seq;
            r(15_u8) += 2_u32;
        }
    };

    switch(opcode.get()) {
        case 0b00: { // ADD
            ASSERT(bit::test(instr, 6_u8) || bit::test(instr, 7_u8));
            set_rd_and_flush(rd + rs);
            break;
        }
        case 0b01: { // CMP
            ASSERT(bit::test(instr, 6_u8) || bit::test(instr, 7_u8));
            alu_sub(rd, rs, true);
            pipeline_.fetch_type = mem_access::seq;
            r(15_u8) += 2_u32;
            break;
        }
        case 0b10: { // MOV
            ASSERT(bit::test(instr, 6_u8) || bit::test(instr, 7_u8));
            set_rd_and_flush(rs);
            break;
        }
        case 0b11: { // BX
            ASSERT(!bit::test(instr, 7_u8));
            if(bit::test(rs, 0_u8)) {
                r(15_u8) = bit::clear(rs, 0_u8);
                pipeline_flush<instruction_mode::thumb>();
            } else {
                cpsr().t = false;
                r(15_u8) = mask::clear(rs, 0b11_u32);
                pipeline_flush<instruction_mode::arm>();
            }
            break;
        }
    }
}

void arm7tdmi::pc_rel_load(const u16 instr) noexcept
{
    u32& pc = r(15_u8);
    u32& rd = r(narrow<u8>((instr >> 8_u16) & 0x7_u16));
    const u16 offset = (instr & 0xFF_u16) << 2_u16;
    rd = read_32(bit::clear(pc, 1_u8) + offset, mem_access::non_seq);
    tick_internal();

    pipeline_.fetch_type = mem_access::non_seq;
    pc += 2_u32;
}

void arm7tdmi::ld_str_reg(const u16 instr) noexcept
{
    const bool is_ldr = bit::test(instr, 11_u8);
    const bool transfer_byte = bit::test(instr, 10_u8);
    const u32 ro = r(narrow<u8>((instr >> 6_u16) & 0x7_u16));
    const u32 rb = r(narrow<u8>((instr >> 3_u16) & 0x7_u16));
    u32& rd = r(narrow<u8>(instr & 0x7_u16));

    const u32 address = rb + ro;
    if(is_ldr) {
        if(transfer_byte) {
            rd = read_8(address, mem_access::non_seq);
        } else {
            rd = read_32_aligned(address, mem_access::non_seq);
        }
        tick_internal();
    } else {
        if(transfer_byte) {
            write_8(address, narrow<u8>(rd), mem_access::non_seq);
        } else {
            write_32(address, rd, mem_access::non_seq);
        }
    }

    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::ld_str_sign_extended_byte_hword(const u16 instr) noexcept
{
    const u16 opcode = (instr >> 10_u16) & 0b11_u16;
    const u32 ro = r(narrow<u8>((instr >> 6_u16) & 0x7_u16));
    const u32 rb = r(narrow<u8>((instr >> 3_u16) & 0x7_u16));
    u32& rd = r(narrow<u8>(instr & 0x7_u16));

    const u32 address = rb + ro;
    switch(opcode.get()) {
        case 0b00: {
            write_16(address, narrow<u16>(rd), mem_access::non_seq);
            break;
        }
        case 0b01: {
            rd = read_8_signed(address, mem_access::non_seq);
            tick_internal();
            break;
        }
        case 0b10: {
            rd = read_16_aligned(address, mem_access::non_seq);
            tick_internal();
            break;
        }
        case 0b11: {
            rd = read_16_signed(address, mem_access::non_seq);
            tick_internal();
            break;
        }
    }

    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::ld_str_imm(const u16 instr) noexcept
{
    const u16 opcode = (instr >> 11_u16) & 0b11_u16;
    const u16 imm = (instr >> 6_u16) & 0x1F_u16;
    const u32 rb = r(narrow<u8>((instr >> 3_u16) & 0x7_u16));
    u32& rd = r(narrow<u8>(instr & 0x7_u16));

    switch(opcode.get()) {
        case 0b00: {
            write_32(rb + (imm << 2_u8), rd, mem_access::non_seq);
            break;
        }
        case 0b01: {
            rd = read_32_aligned(rb + (imm << 2_u8), mem_access::non_seq);
            tick_internal();
            break;
        }
        case 0b10: {
            write_8(rb + imm, narrow<u8>(rd), mem_access::non_seq);
            break;
        }
        case 0b11: {
            rd = read_8(rb + imm, mem_access::non_seq);
            tick_internal();
            break;
        }
    }

    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::ld_str_hword(const u16 instr) noexcept
{
    const bool is_ldr = bit::test(instr, 11_u8);
    const u16 imm = ((instr >> 6_u16) & 0x1F_u16) << 1_u16;
    const u32 rb = r(narrow<u8>((instr >> 3_u16) & 0x7_u16));
    u32& rd = r(narrow<u8>(instr & 0x7_u16));

    const u32 address = rb + imm;
    if(is_ldr) {
        rd = read_16_aligned(address, mem_access::non_seq);
        tick_internal();
    } else {
        write_16(address, narrow<u16>(rd), mem_access::non_seq);
    }

    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::ld_str_sp_relative(const u16 instr) noexcept
{
    const bool is_ldr = bit::test(instr, 11_u8);
    u32& rd = r(narrow<u8>((instr >> 8_u16) & 0x7_u16));
    const u16 imm_offset = (instr & 0xFF_u16) << 2_u16;

    const u32 address = r(13_u8) + imm_offset;
    if(is_ldr) {
        rd = read_32_aligned(address, mem_access::non_seq);
        tick_internal();
    } else {
        write_32(address, rd, mem_access::non_seq);
    }

    pipeline_.fetch_type = mem_access::non_seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::ld_addr(const u16 instr) noexcept
{
    const bool use_sp = bit::test(instr, 11_u8);
    u32& rd = r(narrow<u8>((instr >> 8_u16) & 0x7_u16));
    const u16 imm_offset = (instr & 0xFF_u16) << 2_u16;

    if(use_sp) {
        rd = r(13_u8) + imm_offset;
    } else {
        rd = bit::clear(r(15_u8), 1_u8) + imm_offset;
    }

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::add_offset_to_sp(const u16 instr) noexcept
{
    const bool subtract = bit::test(instr, 7_u8);
    const u16 imm_offset = (instr & 0x7F_u16) << 2_u16;

    if(subtract) {
        r(13_u8) -= imm_offset;
    } else {
        r(13_u8) += imm_offset;
    }

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::push_pop(const u16 instr) noexcept
{
    const bool is_pop = bit::test(instr, 11_u8);
    const bool use_pc_lr = bit::test(instr, 8_u8);
    const auto rlist = generate_register_list(instr, 8_u8);
    u32& sp = r(13_u8);
    u32& pc = r(15_u8);
    auto access = mem_access::non_seq;

    if(rlist.empty() && !use_pc_lr) {
        if(is_pop) {
            pc = read_32(sp, access);
            pipeline_flush<instruction_mode::thumb>();
            sp += 0x40_u32;
        } else {
            sp -= 0x40_u32;
            pipeline_.fetch_type = mem_access::non_seq;
            pc += 2;
            write_32(sp, pc, access);
        }

        return;
    }

    u32 address = sp;

    if(is_pop) {
        for(const u8 reg : rlist) {
            r(reg) = read_32(address, access);
            access = mem_access::seq;
            address += 4_u32;
        }

        if(use_pc_lr) {
            pc = bit::clear(read_32(address, access), 0_u8);
            sp = address + 4_u8;
            tick_internal();
            pipeline_flush<instruction_mode::thumb>();
            return;
        }

        tick_internal();
        sp = address;
    } else {
        address -= 4_u32 * narrow<u32>(rlist.size());
        if(use_pc_lr) {
            address -= 4_u32;
        }

        const u32 new_sp = address;

        for(const u8 reg : rlist) {
            write_32(address, r(reg), access);
            access = mem_access::seq;
            address += 4_u32;
        }

        if(use_pc_lr) {
            write_32(address, r(14_u8), access);
        }

        sp = new_sp;
    }

    pipeline_.fetch_type = mem_access::non_seq;
    pc += 2_u32;
}

void arm7tdmi::ld_str_multiple(const u16 instr) noexcept
{
    const bool is_ldm = bit::test(instr, 11_u8);
    const u8 rb = narrow<u8>((instr >> 8_u16) & 0x7_u16);
    const auto rlist = generate_register_list(instr, 8_u8);
    u32& rb_reg = r(rb);
    u32& pc = r(15_u8);

    if(rlist.empty()) {
        if(is_ldm) {
            pc = read_32(rb_reg, mem_access::non_seq);
            pipeline_flush<instruction_mode::thumb>();
        } else {
            pipeline_.fetch_type = mem_access::seq;
            pc += 2_u32;
            write_32(rb_reg, pc, mem_access::non_seq);
        }

        rb_reg += 0x40_u32;
        return;
    }

    u32 address = rb_reg;
    if(is_ldm) {
        auto access = mem_access::non_seq;
        for(const u8 reg : rlist) {
            r(reg) = read_32(address, access);
            access = mem_access::seq;
            address += 4_u32;
        }

        tick_internal();

        if(!bit::test(instr, rb)) {
            rb_reg = address;
        }
    } else {
        const u32 final_addr = address + 4_u32 * narrow<u32>(rlist.size());
        write_32(address, rlist[0_usize], mem_access::non_seq);
        rb_reg = final_addr;
        address += 4_u32;

        for(u32 i = 1_u32; i < rlist.size(); ++i) {
            write_32(address, rlist[i], mem_access::seq);
            address += 4_u32;
        }
    }

    pipeline_.fetch_type = mem_access::non_seq;
    pc += 2_u32;
}

void arm7tdmi::branch_cond(const u16 instr) noexcept
{
    if(condition_met((instr >> 8_u16) & 0xF_u16)) {
        const i32 offset = math::sign_extend<8>(widen<u32>((instr & 0xFF_u16) << 1_u16));
        r(15_u8) += offset;
        pipeline_flush<instruction_mode::thumb>();
    } else {
        pipeline_.fetch_type = mem_access::seq;
        r(15_u8) += 2_u32;
    }
}

void arm7tdmi::swi_thumb(const u16 instr) noexcept
{

}

void arm7tdmi::branch(const u16 instr) noexcept
{
    const i32 offset = math::sign_extend<11>(widen<u32>((instr & 0x7FF_u16) << 1_u16));
    r(15_u8) += offset;
    pipeline_flush<instruction_mode::thumb>();
}

void arm7tdmi::long_branch_link(const u16 instr) noexcept
{

}


} // namespace gba
