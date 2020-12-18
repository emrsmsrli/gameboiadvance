/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba {

u32 arm7tdmi::alu_add(const u32 first_op, const u32 second_op, const bool set_conds) noexcept
{
    if(set_conds) {
        u64 result = first_op;
        result += second_op;

        const u32 result32 = narrow<u32>(result);
        cpsr().n = bit::test(result32, 31_u8);
        cpsr().z = result32 == 0_u32;
        cpsr().c = bit::test(result, 32_u8);
        cpsr().v = bit::test(~(first_op ^ second_op) & (second_op ^ result32), 31_u8);
        return result32;
    }

    return first_op + second_op;
}

u32 arm7tdmi::alu_adc(const u32 first_op, const u32 second_op, const u32 carry, const bool set_conds) noexcept
{
    if(set_conds) {
        u64 result = first_op;
        result += second_op;
        result += carry;

        const u32 result32 = narrow<u32>(result);
        cpsr().n = bit::test(result32, 31_u8);
        cpsr().z = result32 == 0_u32;
        cpsr().c = bit::test(result, 32_u8);
        cpsr().v = bit::test(~(first_op ^ second_op) & (second_op ^ result32), 31_u8);
        return result32;
    }

    return first_op + second_op + carry;
}

u32 arm7tdmi::alu_sub(const u32 first_op, const u32 second_op, const bool set_conds) noexcept
{
    const u32 result = first_op - second_op;
    if(set_conds) {
        cpsr().n = bit::test(result, 31_u8);
        cpsr().z = result == 0_u32;
        cpsr().c = first_op >= second_op;
        cpsr().v = bit::test((first_op ^ second_op) & (first_op ^ result), 31_u8);
    }
    return result;
}

u32 arm7tdmi::alu_sbc(const u32 first_op, const u32 second_op, const u32 borrow, const bool set_conds) noexcept
{
    const u32 result = first_op - second_op - borrow;
    if(set_conds) {
        cpsr().n = bit::test(result, 31_u8);
        cpsr().z = result == 0_u32;

        const u64 r64 = second_op;
        cpsr().c = first_op >= r64 + borrow;
        cpsr().v = bit::test((first_op ^ second_op) & (first_op ^ result), 31_u8);
    }
    return result;
}

} // namespace gba
