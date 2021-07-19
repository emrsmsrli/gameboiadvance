/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/disassembler.h>

#include <sstream>
#include <string_view>

#include <gba/core/math.h>
#include <gba/helper/function_ptr.h>
#include <gba/helper/lookup_table.h>

namespace gba::debugger {

namespace {

using namespace std::string_view_literals;

constexpr std::string_view unknown_instruction = "???"sv;

constexpr array shift_mnemonics{"LSL"sv, "LSR"sv, "ASR"sv, "ROR"sv};
constexpr array register_mnemonics{
  "r0"sv, "r1"sv, "r2"sv, "r3"sv, "r4"sv, "r5"sv, "r6"sv, "r7"sv, "r8"sv, "r9"sv,
  "r10"sv, "r11"sv, "r12"sv, "sp"sv, "lr"sv, "pc"sv, "cpsr"sv, "spsr"sv
};

std::string_view get_condition_mnemonic(const u32 instr, const u32 offset = 28_u32) noexcept
{
    static constexpr array mnemonics{
      "EQ"sv, "NE"sv, "CS"sv, "CC"sv, "MI"sv, "PL"sv, "VS"sv, "VC"sv,
      "HI"sv, "LS"sv, "GE"sv, "LT"sv, "GT"sv, "LE"sv, ""sv, /*AL*/ "NV"sv,
    };
    return mnemonics[instr >> offset];
}

std::string generate_register_list(const u32 instruction, const u32 reg_count) noexcept
{
    const auto print_range = [](std::stringstream& stream, auto count, auto start, auto i) {
        if(count == 1_u32) {
            stream << register_mnemonics[start] << ',';
        } else if(count == 2_u32) {
            stream << register_mnemonics[start] << ',' << register_mnemonics[start + 1] << ',';
        } else if(count > 2_u32) {
            stream << register_mnemonics[start] << '-' << register_mnemonics[i - 1_u32] << ',';
        }
    };

    std::stringstream stream;
    u32 range_count = 0_u32;
    u32 range_start = 0_u32;
    for(u8 i = 0_u8; i < reg_count; ++i) {
        if(bit::test(instruction, i)) {
            if(range_count == 0_u32) { range_start = i; }
            ++range_count;
        } else {
            print_range(stream, range_count, range_start, i);
            range_count = 0_u32;
        }
    }

    print_range(stream, range_count, range_start, reg_count);
    auto str = stream.str();
    if(!str.empty() && str.back() == ',') { str.pop_back(); }
    return str;
}

namespace arm {

std::string data_processing(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto cond_mnemonic = get_condition_mnemonic(instr);
    const auto cond_set_mnemonic = bit::test(instr, 20_u8) ? "S"sv : ""sv;
    const u32 opcode = (instr >> 21_u32) & 0xF_u32;
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;

    static constexpr auto op2 = [](const u32 instr) -> std::string {
        // immediate as 2nd operand
        if(bit::test(instr, 25_u8)) {
            const u32 imm = instr & 0xFF_u32;
            const u8 shift_amount = narrow<u8>((instr >> 8_u32) & 0xF_u32);
            return fmt::format("{:02X}", math::logical_rotate_right(imm, shift_amount << 1_u8).result);
        }

        // register as 2nd operand
        const u32 r2 = instr & 0xF_u32;
        const u32 shift_type = (instr >> 5_u32) & 0x3_u32;

        if(bit::test(instr, 4_u8)) {
            const u32 rshift = (instr >> 8_u32) & 0xF_u32;
            return fmt::format("{},{} {}",
              register_mnemonics[r2], shift_mnemonics[shift_type], register_mnemonics[rshift]);
        }

        u32 shift_amount = (instr >> 7_u32) & 0x1F_u32;
        if(shift_type == 0_u32 && shift_amount == 0_u32) {
            return std::string{register_mnemonics[r2]};
        }

        return fmt::format("{},{} {:X}", register_mnemonics[r2], shift_mnemonics[shift_type], shift_amount);
    };

    switch(opcode.get()) {
        case 0x0:
            return fmt::format("AND{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x1:
            return fmt::format("EOR{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x2:
            return fmt::format("SUB{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x3:
            return fmt::format("RSB{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x4:
            return fmt::format("ADD{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x5:
            return fmt::format("ADC{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x6:
            return fmt::format("SBC{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x7:
            return fmt::format("RSC{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0x8: return fmt::format("TST{} {},{}", cond_mnemonic, register_mnemonics[rn], op2(instr));
        case 0x9: return fmt::format("TEQ{} {},{}", cond_mnemonic, register_mnemonics[rn], op2(instr));
        case 0xA: return fmt::format("CMP{} {},{}", cond_mnemonic, register_mnemonics[rn], op2(instr));
        case 0xB: return fmt::format("CMN{} {},{}", cond_mnemonic, register_mnemonics[rn], op2(instr));
        case 0xC:
            return fmt::format("ORR{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0xD:
            return fmt::format("MOV{}{} {},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd], op2(instr));
        case 0xE:
            return fmt::format("BIC{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd],
              register_mnemonics[rn], op2(instr));
        case 0xF:
            return fmt::format("MVN{}{} {},{}", cond_set_mnemonic, cond_mnemonic, register_mnemonics[rd], op2(instr));
        default: UNREACHABLE();
    }
}

std::string psr_transfer(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto psr = register_mnemonics[bit::extract(instr, 22_u8) + 16_u32];

    // mrs
    if(!bit::test(instr, 21_u8)) {
        return fmt::format("MOV{} {},{}", get_condition_mnemonic(instr),
          register_mnemonics[(instr >> 12_u32) & 0xF_u32], psr);
    }

    // msr
    std::string flags;
    if(((instr >> 16_u32) & 0xF_u32) == 0xF_u32) {
        flags = "all";
    } else {
        static constexpr array flag_mnemonics{"c"sv, "x"sv, "s"sv, "f"sv};

        for(u8 bit = 19_u8; bit >= 16_u8; --bit) {
            if(bit::test(instr, bit)) {
                flags += flag_mnemonics[bit - 16_u8];
            }
        }
    }

    // imm src
    if(bit::test(instr, 25_u8)) {
        const u32 imm = instr & 0xFF_u32;
        const u8 shift_amount = narrow<u8>((instr >> 8_u32) & 0xF_u32);

        return fmt::format("MOV{} {}_{},{:08X}", get_condition_mnemonic(instr), psr, flags,
          math::logical_rotate_right(imm, shift_amount << 1_u8).result);
    }

    // reg src
    return fmt::format("MOV{} {}_{},{}", get_condition_mnemonic(instr), psr, flags,
      register_mnemonics[instr & 0xF_u32]);
}

std::string branch_exchange(const u32 /*addr*/, const u32 instr) noexcept
{
    return fmt::format("BX {}", register_mnemonics[instr & 0xF_u32]);
}

std::string multiply(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto cond_mnemonic = get_condition_mnemonic(instr);
    const auto cond_set_mnemonic = bit::test(instr, 20_u8) ? "S"sv : ""sv;
    const u32 opcode = (instr >> 21_u32) & 0xF_u32;
    const u32 rd = (instr >> 16_u32) & 0xF_u32;
    const u32 rn = (instr >> 12_u32) & 0xF_u32;
    const u32 rs = (instr >> 8_u32) & 0xF_u32;
    const u32 rm = instr & 0xF_u32;

    switch(opcode.get()) {
        case 0b0000:
            return fmt::format("MUL{}{} {},{},{}", cond_set_mnemonic, cond_mnemonic,
              register_mnemonics[rd], register_mnemonics[rm], register_mnemonics[rs]);
        case 0b0001:
            return fmt::format("MLA{}{} {},{},{},{}", cond_set_mnemonic, cond_mnemonic,
              register_mnemonics[rd], register_mnemonics[rm], register_mnemonics[rs], register_mnemonics[rn]);
        case 0b0100:
            return fmt::format("UMULL{}{} {},{},{},{}", cond_set_mnemonic, cond_mnemonic,
              register_mnemonics[rn], register_mnemonics[rd], register_mnemonics[rm], register_mnemonics[rs]);
        case 0b0101:
            return fmt::format("UMLAL{}{} {},{},{},{}", cond_set_mnemonic, cond_mnemonic,
              register_mnemonics[rn], register_mnemonics[rd], register_mnemonics[rm], register_mnemonics[rs]);
        case 0b0110:
            return fmt::format("SMULL{}{} {},{},{},{}", cond_set_mnemonic, cond_mnemonic,
              register_mnemonics[rn], register_mnemonics[rd], register_mnemonics[rm], register_mnemonics[rs]);
        case 0b0111:
            return fmt::format("SMLAL{}{} {},{},{},{}", cond_set_mnemonic, cond_mnemonic,
              register_mnemonics[rn], register_mnemonics[rd], register_mnemonics[rm], register_mnemonics[rs]);
        default: UNREACHABLE();
    }
}

std::string single_data_swap(const u32 /*addr*/, const u32 instr) noexcept
{
    return fmt::format("SWP{}{} {},{},[{}]",
      bit::test(instr, 22_u8) ? "B" : "",
      get_condition_mnemonic(instr),
      register_mnemonics[(instr >> 12_u32) & 0xF_u32],
      register_mnemonics[instr & 0xF_u32],
      register_mnemonics[(instr >> 16_u32) & 0xF_u32]);
}

std::string halfword_data_transfer(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto get_addr = [](const u32 i) -> std::string {
        const auto get_imm_offset = [](const u32 i) { return (i & 0xF_u32) | ((i >> 4_u32) & 0xF0_u32); };

        const auto rn = register_mnemonics[(i >> 16_u32) & 0xF_u32];
        const auto updown = bit::test(i, 23_u8) ? ""sv : "-"sv;

        // pre indexing
        if(bit::test(i, 24_u8)) {
            const auto write_back = bit::test(i, 21_u8) ? "!"sv : ""sv;

            // imm src
            if(bit::test(i, 22_u8)) {
                if(const u32 offset = get_imm_offset(i); offset != 0_u32) {
                    return fmt::format("[{},{}{:02X}]{}", rn, updown, offset, write_back);
                }

                return fmt::format("[{}]", rn);
            }

            // reg src
            return fmt::format("[{},{}{}]{}", rn, updown, register_mnemonics[i & 0xF_u32], write_back);
        }

        // post indexing

        // imm src
        if(bit::test(i, 22_u8)) {
            return fmt::format("[{}],{}{:02X}", rn, updown, get_imm_offset(i));
        }

        // reg src
        return fmt::format("[{}],{}{}", rn, updown, register_mnemonics[i & 0xF_u32]);
    };

    static constexpr array op_mnemonics{
      "STRH"sv, "LDRH"sv, unknown_instruction,
      "LDRSB"sv, unknown_instruction, "LDRSH"sv
    };

    const u32 opcode = (instr >> 5_u32) & 0b11_u32;
    if(opcode == 0_u32) {
        return std::string{unknown_instruction};
    }

    const u32 op = bit::extract(instr, 20_u8) | (opcode - 1_u32);
    if(const auto mnemonic = op_mnemonics[op]; mnemonic != unknown_instruction) {
        return fmt::format("{}{} {},{}", mnemonic, get_condition_mnemonic(instr),
          register_mnemonics[(instr >> 12_u32) & 0xF_u32], get_addr(instr));
    }

    return std::string{unknown_instruction};
}

std::string single_data_transfer(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto get_addr = [](const u32 i) -> std::string {
        const auto rn = register_mnemonics[(i >> 16_u32) & 0xF_u32];
        const auto updown = bit::test(i, 23_u8) ? ""sv : "-"sv;
        const u32 offset = i & 0xFFF_u32;

        // pre indexing
        if(bit::test(i, 24_u8)) {
            const auto write_back = bit::test(i, 21_u8) ? "!"sv : ""sv;

            if(offset == 0_u32) {
                return fmt::format("[{}]", rn);
            }

            // imm src
            if(!bit::test(i, 25_u8)) {
                return fmt::format("[{},{}{:02X}]{}", rn, updown, offset, write_back);
            }

            // reg src
            const u32 rm = offset & 0xF_u32;
            const u32 shift_amount = (offset >> 7_u32) & 0xF_u32;
            const u32 shift_type = (offset >> 5_u32) & 0x2_u32;

            if(shift_type == 0_u32 && shift_amount == 0_u32) {
                return fmt::format("[{},{}{}]{}", rn, updown, register_mnemonics[rm], write_back);
            }

            return fmt::format("[{},{}{},{} {:X}]{}", rn, updown, register_mnemonics[rm],
              shift_mnemonics[shift_type], shift_amount, write_back);
        }

        // post indexing

        // imm src
        if(!bit::test(i, 25_u8)) {
            return fmt::format("[{}],{}{:02X}", rn, updown, offset);
        }

        // reg src
        const u32 rm = offset & 0xF_u32;
        const u32 shift_amount = (offset >> 7_u32) & 0xF_u32;
        const u32 shift_type = (offset >> 5_u32) & 0x2_u32;

        if(shift_type == 0_u32 && shift_amount == 0_u32) {
            return fmt::format("[{}],{}{}", rn, updown, register_mnemonics[rm]);
        }

        return fmt::format("[{}],{}{},{} {:X}", rn, updown, register_mnemonics[rm],
          shift_mnemonics[shift_type], shift_amount);
    };

    static constexpr array op_mnemonics{
      "STR"sv, "LDR"sv, "STRB"sv, "LDRB"sv,
      "STRT"sv, "LDRT"sv, "STRBT"sv, "LDRBT"sv,
    };

    const bool is_load = bit::test(instr, 20_u8);
    const u32 op = bit::from_bool(is_load)
      | (bit::extract(instr, 22_u8) << 1_u32)
      | (bit::from_bool(!bit::test(instr, 24_u8) && bit::test(instr, 21_u8)) << 2_u32);

    return fmt::format("{}{} {},{}", op_mnemonics[op], get_condition_mnemonic(instr),
      register_mnemonics[(instr >> 12_u32) & 0xF_u32], get_addr(instr));
}

std::string undefined(const u32 /*addr*/, const u32 /*instr*/) noexcept
{
    return "undefined";
}

std::string block_data_transfer(const u32 /*addr*/, const u32 instr) noexcept
{
    const u32 op = ((instr >> 23_u32) & 0b11_u32) | bit::extract(instr, 20_u8) << 2_u32;
    const u32 reg = (instr >> 16_u32) & 0xF_u32;
    static constexpr array op_mnemonics{
      "STMDA"sv, "STMIA"sv, "STMDB"sv, "STMIB"sv,
      "LDMDA"sv, "LDMIA"sv, "LDMDB"sv, "LDMIB"sv,
    };
    static constexpr array op_mnemonics_r13{
      "STMED"sv, "STMEA"sv, "STMFD"sv, "STMFA"sv,
      "LDMFA"sv, "LDMFD"sv, "LDMEA"sv, "LDMED"sv,
    };
    return fmt::format("{}{} [{}]{},{{{}}}{}",
      reg == 13_u32 ? op_mnemonics_r13[op] : op_mnemonics[op],
      get_condition_mnemonic(instr),
      register_mnemonics[reg],
      bit::test(instr, 21_u8) ? "!" : "", // write back
      generate_register_list(instr, 16_u32), bit::test(instr, 22_u8) ? "^" : "");
}

std::string branch_link(const u32 addr, const u32 instr) noexcept
{
    const i32 sign_extended_offset = math::sign_extend<26>((instr & 0xFFFFFF_u32) << 2_u32);

    return fmt::format("B{}{} {:0>8X}",
      bit::test(instr, 24_u8) ? "L" : "",
      get_condition_mnemonic(instr),
      addr + sign_extended_offset + 8_u32);
}

std::string swi(const u32 /*addr*/, const u32 instr) noexcept
{
    return fmt::format("SWI{} {:0>8X}", get_condition_mnemonic(instr), instr & 0xFFFFFF_u32);
}

} // namespace arm

namespace thumb {

std::string move_shifted_reg(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 op = (instr >> 11_u16) & 0b11_u16;
    const u16 rd = instr & 0b111_u16;
    const u16 rs = (instr >> 3_u16) & 0b111_u16;
    u16 offset = (instr >> 6_u16) & 0x1F_u16;

    if(offset == 0_u16) {
        if(op == 0_u16) {
            return fmt::format("MOVS {},{}", register_mnemonics[rd], register_mnemonics[rs]);
        }
        offset = 32_u16;
    }

    static constexpr array op_mnemonics{"LSL"sv, "LSR"sv, "ASR"sv};
    return fmt::format("{}S {},{},{:02X}", op_mnemonics[op], register_mnemonics[rd], register_mnemonics[rs], offset);
}

std::string add_subtract(const u32 /*addr*/, const u16 instr) noexcept
{
    static constexpr array op_mnemonics{"ADD"sv, "SUB"sv};
    const u16 rd = instr & 0b111_u16;
    const u16 rs = (instr >> 3_u16) & 0b111_u16;

    // imm
    if(bit::test(instr, 10_u8)) {
        return fmt::format("{}S {},{},{:X}", op_mnemonics[bit::extract(instr, 9_u8)], register_mnemonics[rd],
          register_mnemonics[rs], (instr >> 6_u16) & 0b111_u16);
    }

    // reg
    return fmt::format("{}S {},{},{}", op_mnemonics[bit::extract(instr, 9_u8)], register_mnemonics[rd],
      register_mnemonics[rs], register_mnemonics[(instr >> 6_u16) & 0b111_u16]);
}

std::string mov_cmp_add_sub_imm(const u32 /*addr*/, const u16 instr) noexcept
{
    static constexpr array op_mnemonics{"MOV"sv, "CMP"sv, "ADD"sv, "SUB"sv};
    const u16 imm = instr & 0xFF_u16;
    const u16 rd = (instr >> 8_u16) & 0b111_u16;
    const u16 op = (instr >> 11_u16) & 0b11_u16;
    return fmt::format("{}S {},{:02X}", op_mnemonics[op], register_mnemonics[rd], imm);
}

std::string alu(const u32 /*addr*/, const u16 instr) noexcept
{
    static constexpr array op_mnemonics{
      "ANDS"sv, "EORS"sv, "LSLS"sv, "LSRS"sv, "ASRS"sv, "ADCS"sv, "SBCS"sv, "RORS"sv,
      "TST"sv, "NEGS"sv, "CMP"sv, "CMN"sv, "ORRS"sv, "MULS"sv, "BICS"sv, "MVNS"sv,
    };
    const u16 op = (instr >> 6_u16) & 0xF_u16;
    const u16 rs = (instr >> 3_u16) & 0b111_u16;
    const u16 rd = instr & 0b111_u16;
    return fmt::format("{} {},{}", op_mnemonics[op], register_mnemonics[rd], register_mnemonics[rs]);
}

std::string hireg_bx(const u32 /*addr*/, const u16 instr) noexcept
{
    if(instr == 0x46C0_u16) { return "NOP"; }

    const u16 op = (instr >> 8_u16) & 0b11_u16;
    const u16 rs = (instr >> 3_u16) & 0xF_u16;

    if(op == 3_u16) {
        return fmt::format("BX {}", register_mnemonics[rs]);
    }

    static constexpr array op_mnemonics{"ADD"sv, "CMP"sv, "MOV"sv};

    const u16 rd = (bit::extract(instr, 7_u8) << 3_u16) | (instr & 0b111_u16);
    return fmt::format("{} {},{}", op_mnemonics[op], register_mnemonics[rd], register_mnemonics[rs]);
}

std::string pc_rel_load(const u32 addr, const u16 instr) noexcept
{
    const u16 rd = (instr >> 8_u16) & 0b111_u16;
    const u32 jump_addr = ((instr & 0xFF_u16) << 2_u16) + bit::clear(addr + 4_u16, 1_u8);
    return fmt::format("LDR {},[{:08X}]", register_mnemonics[rd], jump_addr);
}

std::string ld_str_reg(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 rd = instr & 0b111_u16;
    const u16 rb = (instr >> 3_u16) & 0b111_u16;
    const u16 ro = (instr >> 6_u16) & 0b111_u16;
    const u16 op = (instr >> 10_u16) & 0b11_u16;

    static constexpr array op_mnemonics{"STR"sv, "STRB"sv, "LDR"sv, "LDRB"sv};
    return fmt::format("{} {},[{},{}]", op_mnemonics[op], register_mnemonics[rd], register_mnemonics[rb],
      register_mnemonics[ro]);
}

std::string ld_str_sign_extended_byte_hword(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 rd = instr & 0b111_u16;
    const u16 rb = (instr >> 3_u16) & 0b111_u16;
    const u16 ro = (instr >> 6_u16) & 0b111_u16;
    const u16 op = (instr >> 10_u16) & 0b11_u16;

    static constexpr array op_mnemonics{"STRH"sv, "LDSB"sv, "LDRH"sv, "LDSH"sv};
    return fmt::format("{} {},[{},{}]", op_mnemonics[op], register_mnemonics[rd], register_mnemonics[rb],
      register_mnemonics[ro]);
}

std::string ld_str_imm(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 rd = instr & 0b111_u16;
    const u16 rb = (instr >> 3_u16) & 0b111_u16;
    u16 imm = (instr >> 6_u16) & 0x1F_u16;
    const u16 op = (instr >> 11_u16) & 0b11_u16;

    if(!bit::test(op, 1_u8)) {
        imm <<= 2_u16;
    }

    static constexpr array op_mnemonics{"STR"sv, "LDR"sv, "STRB"sv, "LDRB"sv};
    return fmt::format("{} {},[{},{:02X}]", op_mnemonics[op], register_mnemonics[rd], register_mnemonics[rb], imm);
}

std::string ld_str_hword(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 rd = instr & 0b111_u16;
    const u16 rb = (instr >> 3_u16) & 0b111_u16;
    const u16 imm = ((instr >> 6_u16) & 0x1F_u16) << 1_u16;
    static constexpr array op_mnemonics{"STRH"sv, "LDRH"sv};
    return fmt::format("{} {},[{},{:02X}]", op_mnemonics[bit::extract(instr, 11_u8)], register_mnemonics[rd],
      register_mnemonics[rb], imm);
}

std::string ld_str_sp_relative(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 rd = (instr >> 8_u16) & 0b111_u16;
    const u16 offset = (instr & 0xFF_u16) << 2_u16;
    static constexpr array op_mnemonics{"STR"sv, "LDR"sv};
    return fmt::format("{} {},[sp,{:02X}]", op_mnemonics[bit::extract(instr, 11_u8)], register_mnemonics[rd],
      offset);
}

std::string ld_addr(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 rd = (instr >> 8_u16) & 0b111_u16;
    const u16 offset = (instr & 0xFF_u16) << 2_u16;
    return fmt::format("ADD {},{},{:02X}", register_mnemonics[rd], bit::test(instr, 11_u8) ? "sp" : "pc", offset);
}

std::string add_offset_to_sp(const u32 /*addr*/, const u16 instr) noexcept
{
    return fmt::format("ADD sp, {}{:02X}", bit::test(instr, 7_u8) ? "-" : "", (instr & 0x7F_u16) << 2_u16);
}

std::string push_pop(const u32 /*addr*/, const u16 instr) noexcept
{
    const u16 op = bit::extract(instr, 11_u8);
    auto reg_list = generate_register_list(instr, 8_u32);

    if(bit::test(instr, 8_u8)) {
        if(!reg_list.empty()) { reg_list.push_back(','); }
        reg_list += register_mnemonics[14_u32 + op];
    }

    static constexpr array op_mnemonics{"PUSH"sv, "POP"sv};
    return fmt::format("{} {{{}}}", op_mnemonics[op], reg_list);
}

std::string ld_str_multiple(const u32 /*addr*/, const u16 instr) noexcept
{
    auto reg_list = generate_register_list(instr, 8_u32);
    if(reg_list.empty()) { reg_list = register_mnemonics[15_u32]; }

    static constexpr array op_mnemonics{"STMIA"sv, "LDMIA"sv};
    return fmt::format("{} {}!,{{{}}}", op_mnemonics[bit::extract(instr, 11_u8)],
      register_mnemonics[(instr >> 8_u16) & 0b111_u16], reg_list);
}

std::string branch_cond(const u32 addr, const u16 instr) noexcept
{
    const i32 offset = math::sign_extend<9>(widen<u32>((instr & 0xFF_u16)) << 1_u32);
    return fmt::format("B{} {:08X}", get_condition_mnemonic(instr & 0x0FFF_u16, 8_u32),
      addr + 4_u32 + offset);
}

std::string swi(const u32 /*addr*/, const u16 instr) noexcept
{
    return fmt::format("SWI {:02X}", instr & 0xFF_u16);
}

std::string branch(const u32 addr, const u16 instr) noexcept
{
    const i32 offset = math::sign_extend<12>(widen<u32>((instr & 0x7FF_u16)) << 1_u32);
    return fmt::format("B {:08X}", addr + 4_u32 + offset);
}

std::string long_branch_link(const u32 /*addr*/, const u16 instr) noexcept
{
    // first part of instruction has no mnemonic
    if(!bit::test(instr, 11_u8)) { return std::string{unknown_instruction}; }
    return fmt::format("BL lr+{:03X}", math::logical_shift_left(instr & 0x7FF_u16, 1_u8).result);
}

} // namespace thumb

constexpr lookup_table<function_ptr<std::string(u32, u32)>, 12_u32, 17_u32> arm_table_{
  {"000xxxxxxxx0", function_ptr{&arm::data_processing}},
  {"000xxxxx0xx1", function_ptr{&arm::data_processing}},
  {"000xx0xx1xx1", function_ptr{&arm::halfword_data_transfer}},
  {"000xx1xx1xx1", function_ptr{&arm::halfword_data_transfer}},
  {"00001xxx1001", function_ptr{&arm::multiply}},
  {"000000xx1001", function_ptr{&arm::multiply}},
  {"00010xx00000", function_ptr{&arm::psr_transfer}},
  {"00010x001001", function_ptr{&arm::single_data_swap}},
  {"000100100001", function_ptr{&arm::branch_exchange}},
  {"001xxxxxxxxx", function_ptr{&arm::data_processing}},
  {"00110x10xxxx", function_ptr{&arm::psr_transfer}},
  {"010xxxxxxxxx", function_ptr{&arm::single_data_transfer}},
  {"011xxxxxxxx0", function_ptr{&arm::single_data_transfer}},
  {"011xxxxxxxx1", function_ptr{&arm::undefined}},
  {"100xxxxxxxxx", function_ptr{&arm::block_data_transfer}},
  {"101xxxxxxxxx", function_ptr{&arm::branch_link}},
  {"1111xxxxxxxx", function_ptr{&arm::swi}},
};

constexpr lookup_table<function_ptr<std::string(u32, u16)>, 10_u32, 19_u32> thumb_table_{
  {"000xxxxxxx", function_ptr{&thumb::move_shifted_reg}},
  {"00011xxxxx", function_ptr{&thumb::add_subtract}},
  {"001xxxxxxx", function_ptr{&thumb::mov_cmp_add_sub_imm}},
  {"010000xxxx", function_ptr{&thumb::alu}},
  {"010001xxxx", function_ptr{&thumb::hireg_bx}},
  {"01001xxxxx", function_ptr{&thumb::pc_rel_load}},
  {"0101xx0xxx", function_ptr{&thumb::ld_str_reg}},
  {"0101xx1xxx", function_ptr{&thumb::ld_str_sign_extended_byte_hword}},
  {"011xxxxxxx", function_ptr{&thumb::ld_str_imm}},
  {"1000xxxxxx", function_ptr{&thumb::ld_str_hword}},
  {"1001xxxxxx", function_ptr{&thumb::ld_str_sp_relative}},
  {"1010xxxxxx", function_ptr{&thumb::ld_addr}},
  {"1011x10xxx", function_ptr{&thumb::push_pop}},
  {"10110000xx", function_ptr{&thumb::add_offset_to_sp}},
  {"1100xxxxxx", function_ptr{&thumb::ld_str_multiple}},
  {"1101xxxxxx", function_ptr{&thumb::branch_cond}},
  {"11011111xx", function_ptr{&thumb::swi}},
  {"11100xxxxx", function_ptr{&thumb::branch}},
  {"1111xxxxxx", function_ptr{&thumb::long_branch_link}},
};

} // namespace

std::string disassembler::disassemble_arm(const u32 address, const u32 instruction) noexcept
{
    if(auto func = arm_table_[((instruction >> 16_u32) & 0xFF0_u32) | (instruction >> 4_u32) & 0xF_u32]) {
        return func(address, instruction);
    }

    return std::string{unknown_instruction};
}

std::string disassembler::disassemble_thumb(const u32 address, const u16 instruction) noexcept
{
    if(auto func = thumb_table_[instruction >> 6_u16]) {
        return func(address, instruction);
    }

    return std::string{unknown_instruction};
}

} // namespace gba::debugger
