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

}

void arm7tdmi::data_processing_reg_shifted_reg(const u32 instr) noexcept
{

}

void arm7tdmi::data_processing_imm(const u32 instr) noexcept
{
    
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

}

void arm7tdmi::psr_transfer_imm(const u32 instr) noexcept
{

}

void arm7tdmi::multiply(const u32 instr) noexcept
{
    
}

void arm7tdmi::multiply_long(const u32 instr) noexcept
{

}

void arm7tdmi::single_data_swap(const u32 instr) noexcept
{
    
}

void arm7tdmi::single_data_transfer_imm(const u32 instr) noexcept
{
    
}

void arm7tdmi::single_data_transfer_reg(const u32 instr) noexcept
{
    
}

void arm7tdmi::undefined(const u32 instr) noexcept
{
    
}

void arm7tdmi::block_data_transfer(const u32 instr) noexcept
{
    
}

void arm7tdmi::branch_with_link(const u32 instr) noexcept
{
    // todo 1N

    // link
    if(bit::test(instr, 24_u8)) {
        r(14_u8) = r(15_u8) - 4_u32;
    }

    r(15_u8) += math::sign_extend<26>(instr & 0x00FFFFFF_u32 << 2_u32);

    // todo 2S
}

void arm7tdmi::swi_arm(const u32 instr) noexcept
{
    
}

} // namespace gba
