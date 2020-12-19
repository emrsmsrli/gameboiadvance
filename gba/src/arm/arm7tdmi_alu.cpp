/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba {

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

static void alu_lsl(u32& operand, const u8 shift_amount, bool& carry) noexcept
{
    if(shift_amount >= 32_u8) {
        operand = 0_u32;

        if(shift_amount == 32_u8) {
            carry = bit::test(operand, 0_u8);
        } else {
            carry = false;
        }
    } else if(shift_amount != 0_u8) {
        const auto lsl = math::logical_shift_left(operand, shift_amount);
        operand = lsl.result;
        carry = static_cast<bool>(lsl.carry.get());
    }
}

static void alu_lsr(u32& operand, const u8 shift_amount, bool& carry, const bool imm) noexcept
{
    if(shift_amount >= 32_u8) {
        operand = 0_u32;

        if(shift_amount == 32_u8) {
            carry = bit::test(operand, 31_u8);
        } else {
            carry = false;
        }
    } else if(shift_amount == 0_u8) {
        if(imm) {
            operand = 0_u32;
            carry = bit::test(operand, 31_u8);
        }
    } else {
        const auto lsr = math::logical_shift_right(operand, shift_amount);
        operand = lsr.result;
        carry = static_cast<bool>(lsr.carry.get());
    }
}

static void alu_asr(u32& operand, u8 shift_amount, bool& carry, const bool imm) noexcept
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

static void alu_ror(u32& operand, u8 shift_amount, bool& carry, const bool imm) noexcept
{
    if(shift_amount == 0_u32) {
        if(imm) {
            const auto rrx = math::logical_rotate_right_extended(operand, bit::from_bool(carry));
            operand = rrx.result;
            carry = static_cast<bool>(rrx.carry.get());
        }
    } else {
        shift_amount %= 32_u8;

        const auto asr = math::logical_rotate_right(operand, shift_amount);
        operand = asr.result;
        carry = static_cast<bool>(asr.carry.get());
    }
}

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
