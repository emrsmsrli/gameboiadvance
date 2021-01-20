/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba::arm {

void arm7tdmi::alu_barrel_shift(const barrel_shift_type shift_type, u32& operand,
  u8 shift_amount, bool& carry, const bool imm) noexcept
{
    switch(shift_type) {
        case barrel_shift_type::lsl: {
            alu_lsl(operand, shift_amount, carry);
            break;
        }
        case barrel_shift_type::lsr: {
            alu_lsr(operand, shift_amount, carry, imm);
            break;
        }
        case barrel_shift_type::asr: {
            alu_asr(operand, shift_amount, carry, imm);
            break;
        }
        case barrel_shift_type::ror: {
            alu_ror(operand, shift_amount, carry, imm);
            break;
        }
        default:
            UNREACHABLE();
    }
}

void arm7tdmi::alu_lsl(u32& operand, const u8 shift_amount, bool& carry) noexcept
{
    if(shift_amount >= 32_u8) {
        if(shift_amount == 32_u8) {
            carry = bit::test(operand, 0_u8);
        } else {
            carry = false;
        }
        operand = 0_u32;
    } else if(shift_amount != 0_u8) {
        const auto lsl = math::logical_shift_left(operand, shift_amount);
        operand = lsl.result;
        carry = static_cast<bool>(lsl.carry.get());
    }
}

void arm7tdmi::alu_lsr(u32& operand, const u8 shift_amount, bool& carry, const bool imm) noexcept
{
    if(shift_amount >= 32_u8) {
        if(shift_amount == 32_u8) {
            carry = bit::test(operand, 31_u8);
        } else {
            carry = false;
        }
        operand = 0_u32;
    } else if(shift_amount == 0_u8) {
        if(imm) {
            carry = bit::test(operand, 31_u8);
            operand = 0_u32;
        }
    } else {
        const auto lsr = math::logical_shift_right(operand, shift_amount);
        operand = lsr.result;
        carry = static_cast<bool>(lsr.carry.get());
    }
}

void arm7tdmi::alu_asr(u32& operand, u8 shift_amount, bool& carry, const bool imm) noexcept
{
    if(shift_amount == 0_u8) {
        if(imm) {
            shift_amount = 32_u8;
        } else {
            return;
        }
    }

    if(shift_amount >= 32_u8) {
        const u32 msb = bit::extract(operand, 31_u8);
        operand = 0xFFFF'FFFF_u32 * msb;
        carry = static_cast<bool>(msb.get());
    } else {
        const auto asr = math::arithmetic_shift_right(operand, shift_amount);
        operand = asr.result;
        carry = static_cast<bool>(asr.carry.get());
    }
}

void arm7tdmi::alu_ror(u32& operand, u8 shift_amount, bool& carry, const bool imm) noexcept
{
    if(shift_amount == 0_u32) {
        if(imm) {
            const auto rrx = math::logical_rotate_right_extended(operand, bit::from_bool(carry));
            operand = rrx.result;
            carry = static_cast<bool>(rrx.carry.get());
        }
    } else {
        shift_amount %= 32_u8;

        const auto ror = math::logical_rotate_right(operand, shift_amount);
        operand = ror.result;
        carry = static_cast<bool>(ror.carry.get());
    }
}

u32 arm7tdmi::alu_add(const u32 first_op, const u32 second_op, const bool set_flags) noexcept
{
    if(set_flags) {
        const u64 result = widen<u64>(first_op) + second_op;
        const u32 result32 = narrow<u32>(result);

        cpsr().n = bit::test(result32, 31_u8);
        cpsr().z = result32 == 0_u32;
        cpsr().c = bit::test(result, 32_u8);
        cpsr().v = bit::test(~(first_op ^ second_op) & (second_op ^ result32), 31_u8);
        return result32;
    }

    return first_op + second_op;
}

u32 arm7tdmi::alu_adc(const u32 first_op, const u32 second_op, const bool set_flags) noexcept
{
    if(set_flags) {
        const u64 result = widen<u64>(first_op) + second_op + bit::from_bool(cpsr().c);
        const u32 result32 = narrow<u32>(result);

        cpsr().n = bit::test(result32, 31_u8);
        cpsr().z = result32 == 0_u32;
        cpsr().c = bit::test(result, 32_u8);
        cpsr().v = bit::test(~(first_op ^ second_op) & (second_op ^ result32), 31_u8);
        return result32;
    }

    return first_op + second_op + bit::from_bool(cpsr().c);
}

u32 arm7tdmi::alu_sub(const u32 first_op, const u32 second_op, const bool set_flags) noexcept
{
    const u32 result = first_op - second_op;
    if(set_flags) {
        cpsr().n = bit::test(result, 31_u8);
        cpsr().z = result == 0_u32;
        cpsr().c = first_op >= second_op;
        cpsr().v = bit::test((first_op ^ second_op) & (first_op ^ result), 31_u8);
    }
    return result;
}

u32 arm7tdmi::alu_sbc(const u32 first_op, const u32 second_op, const bool set_flags) noexcept
{
    const u32 result = first_op - second_op - bit::from_bool(!cpsr().c);
    if(set_flags) {
        cpsr().n = bit::test(result, 31_u8);
        cpsr().z = result == 0_u32;
        cpsr().c = first_op >= widen<u64>(second_op) + bit::from_bool(!cpsr().c);
        cpsr().v = bit::test((first_op ^ second_op) & (first_op ^ result), 31_u8);
    }
    return result;
}

} // namespace gba::arm
