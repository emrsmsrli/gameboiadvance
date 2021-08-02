/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM7TDMI_DECODER_TABLE_GEN_H
#define GAMEBOIADVANCE_ARM7TDMI_DECODER_TABLE_GEN_H

#include <gba/cpu/arm7tdmi.h>
#include <gba/helper/function_ptr.h>
#include <gba/helper/static_for.h>

namespace gba::cpu {

class decoder_table_generator {
public:
    static constexpr usize::type arm_max_decoders = 4096;
    static constexpr usize::type thumb_max_decoders = 1024;

    using arm_decoder = function_ptr<arm7tdmi, void(u32)>;
    using thumb_decoder = function_ptr<arm7tdmi, void(u16)>;
    template<typename T> using decoder_func = typename T::type;

    using arm_decoder_table = array<arm_decoder, arm_max_decoders>;
    using thumb_decoder_table = array<thumb_decoder, thumb_max_decoders>;

private:
    template<auto Instr>
    static constexpr decoder_func<arm_decoder> generate_arm() noexcept
    {
        constexpr u32 instruction = Instr & 0x0FFF'FFFF_u32;
        constexpr bool has_pre_indexing = bit::test(instruction, 24_u8);
        constexpr bool should_add_to_base = bit::test(instruction, 23_u8);
        constexpr bool should_writeback = bit::test(instruction, 21_u8);
        constexpr bool is_load = bit::test(instruction, 20_u8);

        switch((instruction >> 26_u32).get()) {
            case 0b00: {
                if constexpr(bit::test(instruction, 25_u8)) {
                    constexpr bool set_flags = bit::test(instruction, 20_u8);
                    constexpr u32 alu_opcode = (instruction >> 21_u32) & 0xF_u32;

                    if constexpr(!set_flags && alu_opcode >= 0b1000_u32 && alu_opcode <= 0b1011_u32) {
                        constexpr bool use_spsr = bit::test(instruction, 22_u8);
                        constexpr auto opcode = to_enum<arm7tdmi::psr_transfer_opcode>(bit::extract(instruction, 21_u8));
                        return &arm7tdmi::psr_transfer<true, use_spsr, opcode>;
                    }

                    constexpr u32 op2 = (instruction >> 4_u32) & 0xF_u32;
                    constexpr auto opcode = to_enum<arm7tdmi::arm_alu_opcode>(alu_opcode);
                    return &arm7tdmi::data_processing<true, opcode, set_flags, op2.get()>;
                }

                if constexpr((instruction & 0x0FF0'00F0_u32) == 0x0120'0010_u32) {
                    return &arm7tdmi::branch_exchange;
                }

                if constexpr((instruction & 0x0100'00F0_u32) == 0x0000'0090_u32) {
                    constexpr bool should_accumulate = bit::test(instruction, 21_u8);
                    constexpr bool should_set_flags = bit::test(instruction, 20_u8);

                    if constexpr(bit::test(instruction, 23_u8)) {
                        constexpr bool is_signed = bit::test(instruction, 22_u8);
                        return &arm7tdmi::multiply_long<is_signed, should_accumulate, should_set_flags>;
                    }

                    return &arm7tdmi::multiply<should_accumulate, should_set_flags>;
                }

                if constexpr((instruction & 0x0100'00F0_u32) == 0x0100'0090_u32) {
                    constexpr bool is_byte_transfer = bit::test(instruction, 22_u8);
                    return &arm7tdmi::single_data_swap<is_byte_transfer>;
                }

                if constexpr(
                  (instruction & 0x0000'00F0_u32) == 0x0000'00B0_u32 ||
                  (instruction & 0x0000'00D0_u32) == 0x0000'00D0_u32) {
                    constexpr bool has_immediate_offset = bit::test(instruction, 22_u8);
                    constexpr auto opcode = to_enum<arm7tdmi::halfword_data_transfer_opcode>(
                      (instruction >> 5_u32) & 0b11_u32);
                    return &arm7tdmi::halfword_data_transfer<has_pre_indexing,
                      should_add_to_base, has_immediate_offset, should_writeback, is_load, opcode>;
                }

                constexpr bool set_flags = bit::test(instruction, 20_u8);
                constexpr u32 alu_opcode = (instruction >> 21_u32) & 0xF_u32;

                if constexpr(!set_flags && alu_opcode >= 0b1000_u32 && alu_opcode <= 0b1011_u32) {
                    constexpr bool use_spsr = bit::test(instruction, 22_u8);
                    constexpr auto opcode = to_enum<arm7tdmi::psr_transfer_opcode>(bit::extract(instruction, 21_u8));
                    return &arm7tdmi::psr_transfer<false, use_spsr, opcode>;
                }

                constexpr u32 op2 = (instruction >> 4_u32) & 0xF_u32;
                constexpr auto opcode = to_enum<arm7tdmi::arm_alu_opcode>(alu_opcode);
                return &arm7tdmi::data_processing<false, opcode, set_flags, op2.get()>;
            }
            case 0b01: {
                if constexpr((instruction & 0x0200'0010_u32) == 0x0200'0010_u32) {
                    return &arm7tdmi::undefined;
                }

                constexpr bool has_immediate_offset = !bit::test(instruction, 25_u8);
                constexpr bool is_byte_transfer = bit::test(instruction, 22_u8);
                return &arm7tdmi::single_data_transfer<has_immediate_offset, has_pre_indexing,
                  should_add_to_base, is_byte_transfer, should_writeback, is_load>;
            }
            case 0b10: {
                if constexpr(bit::test(instruction, 25_u8)) {
                    constexpr bool with_link = bit::test(instruction, 24_u8);
                    return &arm7tdmi::branch_with_link<with_link>;
                }

                constexpr bool load_psr_force_user = bit::test(instruction, 22_u8);
                return &arm7tdmi::block_data_transfer<has_pre_indexing, should_add_to_base,
                  load_psr_force_user, should_writeback, is_load>;
            }
            case 0b11: {
                if constexpr((instruction & 0x0300'0000_u32) == 0x0300'0000_u32) {
                    return &arm7tdmi::swi_arm;
                }
            }
            default:
                break;
        }

        return nullptr;
    }

    template<auto Instr>
    static constexpr decoder_func<thumb_decoder> generate_thumb() noexcept
    {
        constexpr u32 instruction = Instr;
        if constexpr((instruction & 0xF800_u32) < 0x1800_u32) {
            constexpr auto opcode = to_enum<arm7tdmi::move_shifted_reg_opcode>((instruction >> 11_u32) & 0b11_u32);
            return &arm7tdmi::move_shifted_reg<opcode>;
        }

        if constexpr((instruction & 0xF800_u32) == 0x1800_u32) {
            constexpr bool has_immediate_operand = bit::test(instruction, 10_u8);
            constexpr bool is_sub = bit::test(instruction, 9_u8);
            constexpr u16 operand = narrow<u16>((instruction >> 6_u32) & 0b111_u32);
            return &arm7tdmi::add_subtract<has_immediate_operand, is_sub, operand.get()>;
        }

        if constexpr((instruction & 0xE000_u32) == 0x2000_u32) {
            constexpr auto opcode = to_enum<arm7tdmi::imm_op_opcode>((instruction >> 11_u32) & 0b11_u32);
            constexpr u32 rd = narrow<u16>((instruction >> 8_u32) & 0b111_u32);
            return &arm7tdmi::mov_cmp_add_sub_imm<opcode, rd.get()>;
        }

        if constexpr((instruction & 0xFC00_u32) == 0x4000_u32) {
            constexpr auto opcode = to_enum<arm7tdmi::thumb_alu_opcode>((instruction >> 6_u32) & 0xF_u32);
            return &arm7tdmi::alu<opcode>;
        }

        if constexpr((instruction & 0xFC00_u32) == 0x4400_u32) {
            constexpr auto opcode = to_enum<arm7tdmi::hireg_bx_opcode>((instruction >> 8_u32) & 0b11_u32);
            return &arm7tdmi::hireg_bx<opcode>;
        }

        if constexpr((instruction & 0xF800_u32) == 0x4800_u32) {
            return &arm7tdmi::pc_rel_load;
        }

        if constexpr((instruction & 0xF200_u32) == 0x5000_u32) {
            constexpr auto opcode = to_enum<arm7tdmi::ld_str_reg_opcode>((instruction >> 10_u32) & 0b11_u32);
            return &arm7tdmi::ld_str_reg<opcode>;
        }

        if constexpr((instruction & 0xF200_u32) == 0x5200_u32) {
            constexpr auto opcode = to_enum<arm7tdmi::ld_str_sign_extended_byte_hword_opcode>(
              (instruction >> 10_u32) & 0b11_u32);
            return &arm7tdmi::ld_str_sign_extended_byte_hword<opcode>;
        }

        if constexpr((instruction & 0xE000_u32) == 0x6000_u32) {
            constexpr auto opcode = to_enum<arm7tdmi::ld_str_imm_opcode>((instruction >> 11_u32) & 0b11_u32);
            return &arm7tdmi::ld_str_imm<opcode>;
        }

        if constexpr((instruction & 0xF000_u32) == 0x8000_u32) {
            constexpr bool is_load = bit::test(instruction, 11_u8);
            return &arm7tdmi::ld_str_hword<is_load>;
        }

        if constexpr((instruction & 0xF000_u32) == 0x9000_u32) {
            constexpr bool is_load = bit::test(instruction, 11_u8);
            return &arm7tdmi::ld_str_sp_relative<is_load>;
        }

        if constexpr((instruction & 0xF000_u32) == 0xA000_u32) {
            constexpr bool use_sp = bit::test(instruction, 11_u8);
            return &arm7tdmi::ld_addr<use_sp>;
        }

        if constexpr((instruction & 0xFF00_u32) == 0xB000_u32) {
            constexpr bool should_subtract = bit::test(instruction, 7_u8);
            return &arm7tdmi::add_offset_to_sp<should_subtract>;
        }

        if constexpr((instruction & 0xF600_u32) == 0xB400_u32) {
            constexpr bool is_pop = bit::test(instruction, 11_u8);
            constexpr bool use_pc_lr = bit::test(instruction, 8_u8);
            return &arm7tdmi::push_pop<is_pop, use_pc_lr>;
        }

        if constexpr((instruction & 0xF000_u32) == 0xC000_u32) {
            constexpr bool is_load = bit::test(instruction, 11_u8);
            return &arm7tdmi::ld_str_multiple<is_load>;
        }

        if constexpr((instruction & 0xFF00_u32) < 0xDF00_u32) {
            constexpr u32 condition = (instruction >> 8_u32) & 0xF_u32;
            return &arm7tdmi::branch_cond<condition.get()>;
        }

        if constexpr((instruction & 0xFF00_u32) == 0xDF00_u32) {
            return &arm7tdmi::swi_thumb;
        }

        if constexpr((instruction & 0xF800_u32) == 0xE000_u32) {
            return &arm7tdmi::branch;
        }

        if constexpr((instruction & 0xF000_u32) == 0xF000_u32) {
            constexpr bool is_second_intruction = bit::test(instruction, 11_u8);
            return &arm7tdmi::long_branch_link<is_second_intruction>;
        }

        return nullptr;
    }

    template<u32::type MinIdx, u32::type MaxIdx>
    static constexpr void generate_arm(arm_decoder_table& table) noexcept
    {
        static_for<u32::type, MinIdx, MaxIdx>([&](const auto intr_bits) {
            table[static_cast<u32::type>(intr_bits)] = arm_decoder{
              generate_arm<((intr_bits & 0xFF0) << 16) | ((intr_bits & 0xF) << 4)>()
            };
        });
    }

public:
    static constexpr arm_decoder_table generate_arm() noexcept
    {
        arm_decoder_table table;
        // break in half so compiler have easier time with AST generation
        generate_arm<0, arm_max_decoders / 2>(table);
        generate_arm<arm_max_decoders / 2, arm_max_decoders>(table);
        return table;
    }

    static constexpr thumb_decoder_table generate_thumb() noexcept
    {
        thumb_decoder_table table;
        static_for<u32::type, 0, thumb_max_decoders>([&](const auto intr_bits) {
            table[static_cast<u32::type>(intr_bits)] = thumb_decoder{generate_thumb<intr_bits << 6>()};
        });
        return table;
    }
};

} // namespace gba::cpu

#endif //GAMEBOIADVANCE_ARM7TDMI_DECODER_TABLE_GEN_H
