/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba {

void arm7tdmi::move_shifted_reg(u16 instr) noexcept
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

void arm7tdmi::add_subtract(u16 instr) noexcept
{

}

void arm7tdmi::mov_cmp_add_sub_imm(u16 instr) noexcept
{

}

void arm7tdmi::alu(u16 instr) noexcept
{

}

void arm7tdmi::hireg_bx(u16 instr) noexcept
{

}

void arm7tdmi::pc_rel_load(u16 instr) noexcept
{

}

void arm7tdmi::ld_str_reg(u16 instr) noexcept
{

}

void arm7tdmi::ld_str_sign_extended_byte_hword(u16 instr) noexcept
{

}

void arm7tdmi::ld_str_imm(u16 instr) noexcept
{

}

void arm7tdmi::ld_str_hword(u16 instr) noexcept
{

}

void arm7tdmi::ld_str_sp_relative(u16 instr) noexcept
{

}

void arm7tdmi::ld_addr(u16 instr) noexcept
{

}

void arm7tdmi::add_offset_to_sp(u16 instr) noexcept
{

}

void arm7tdmi::push_pop(u16 instr) noexcept
{

}

void arm7tdmi::ld_str_multiple(u16 instr) noexcept
{

}

void arm7tdmi::branch_cond(u16 instr) noexcept
{

}

void arm7tdmi::swi_thumb(u16 instr) noexcept
{

}

void arm7tdmi::branch(u16 instr) noexcept
{

}

void arm7tdmi::long_branch_link(u16 instr) noexcept
{

}


} // namespace gba
