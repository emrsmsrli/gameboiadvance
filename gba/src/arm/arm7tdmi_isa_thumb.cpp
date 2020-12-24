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
    }

    pipeline_.fetch_type = mem_access::seq;
    r(15_u8) += 2_u32;
}

void arm7tdmi::alu(const u16 instr) noexcept
{

}

void arm7tdmi::hireg_bx(const u16 instr) noexcept
{

}

void arm7tdmi::pc_rel_load(const u16 instr) noexcept
{

}

void arm7tdmi::ld_str_reg(const u16 instr) noexcept
{

}

void arm7tdmi::ld_str_sign_extended_byte_hword(const u16 instr) noexcept
{

}

void arm7tdmi::ld_str_imm(const u16 instr) noexcept
{

}

void arm7tdmi::ld_str_hword(const u16 instr) noexcept
{

}

void arm7tdmi::ld_str_sp_relative(const u16 instr) noexcept
{

}

void arm7tdmi::ld_addr(const u16 instr) noexcept
{

}

void arm7tdmi::add_offset_to_sp(const u16 instr) noexcept
{

}

void arm7tdmi::push_pop(const u16 instr) noexcept
{

}

void arm7tdmi::ld_str_multiple(const u16 instr) noexcept
{

}

void arm7tdmi::branch_cond(const u16 instr) noexcept
{

}

void arm7tdmi::swi_thumb(const u16 instr) noexcept
{

}

void arm7tdmi::branch(const u16 instr) noexcept
{

}

void arm7tdmi::long_branch_link(const u16 instr) noexcept
{

}


} // namespace gba
