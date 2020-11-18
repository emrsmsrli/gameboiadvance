#include <gba_debugger/disassembler.h>

#include <string_view>
#include <sstream>

#include <fmt/format.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <SFML/Window/Event.hpp>

#include <gba/core/math.h>
#include <gba_debugger/fmt_integer.h>

// https://www.reddit.com/r/EmuDev/comments/8gltfq/gbaarm_instruction_decoding_methods/

namespace gba::debugger {

namespace {

using namespace std::string_view_literals;

constexpr std::string_view unknown_instruction = "???"sv;

constexpr array shift_mnemonics{"LSL"sv, "LSR"sv, "ASR"sv, "ROR"sv};
constexpr array register_mnemonics{
  "r0"sv, "r1"sv, "r2"sv, "r3"sv, "r4"sv, "r5"sv, "r6"sv, "r7"sv, "r8"sv, "r9"sv,
  "r10"sv, "r11"sv, "r12"sv, "r13"sv, "r14"sv, "r15"sv, "cpsr"sv, "spsr"sv
};

std::string_view get_condition_mnemonic(const u32 instr) noexcept
{
    static constexpr array mnemonics{
      "EQ"sv, "NE"sv, "CS"sv, "CC"sv, "MI"sv, "PL"sv, "VS"sv, "VC"sv,
      "HI"sv, "LS"sv, "GE"sv, "LT"sv, "GT"sv, "LE"sv, ""sv, /*AL*/ "NV"sv,
    };
    return mnemonics[instr >> 28_u32];
}

/* arm disassemble */

std::string data_processing(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto cond_mnemonic = get_condition_mnemonic(instr);
    const auto cond_set_mnemonic = bit::test(instr, 20_u32) ? "S"sv : ""sv;
    const u32 opcode = (instr >> 21_u32) & 0xF_u32;
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;

    static constexpr auto op2 = [](const u32 instr) -> std::string {
        // immediate as 2nd operand
        if(bit::test(instr, 25_u32)) {
            const auto imm = instr & 0xFF_u32;
            const auto shift_amount = (instr >> 8_u32) & 0xF_u32;
            return fmt::format("0x{:X}", math::logical_rotate_right(imm, shift_amount * 2_u32).result);
        }

        // register as 2nd operand
        const u32 r2 = instr & 0xF_u32;
        const u32 shift_type = (instr >> 5_u32) & 0x3_u32;

        if(bit::test(instr, 4_u32)) {
            const u32 rshift = (instr >> 8_u32) & 0xF_u32;
            return fmt::format("{},{} {}",
              register_mnemonics[r2], shift_mnemonics[shift_type], register_mnemonics[rshift]);
        }

        u32 shift_amount = (instr >> 7_u32) & 0x1F_u32;
        if(shift_type == 0_u32 && shift_amount == 0_u32) {
            return std::string(register_mnemonics[r2]);
        }

        return fmt::format("{},{} 0x{:X}", register_mnemonics[r2], shift_mnemonics[shift_type], shift_amount);
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
    const auto psr = register_mnemonics[bit::extract(instr, 22_u32) + 16_u32];

    // mrs
    if(!bit::test(instr, 21_u32)) {
        return fmt::format("MOV{} {},{}", get_condition_mnemonic(instr),
          register_mnemonics[(instr >> 12_u32) & 0xF_u32], psr);
    }

    // msr
    std::string flags;
    if(((instr >> 16_u32) & 0b1111_u32) == 0b1111_u32) {
        flags = "all";
    } else {
        static constexpr array flag_mnemonics{"c"sv, "x"sv, "s"sv, "f"sv};

        for(u32 bit = 19_u32; bit >= 16_u32; --bit) {
            if(bit::test(instr, bit)) {
                flags += flag_mnemonics[bit - 16_u32];
            }
        }
    }

    // imm src
    if(bit::test(instr, 25_u32)) {
        const u32 imm = instr & 0xFF_u32;
        const u32 shift_amount = (instr >> 8_u32) & 0xF_u32;

        return fmt::format("MOV{} {}_{},0x{:08X}", get_condition_mnemonic(instr), psr, flags,
          math::logical_rotate_right(imm, shift_amount << 1_u32).result);
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
    const auto cond_set_mnemonic = bit::test(instr, 20_u32) ? "S"sv : ""sv;
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
      bit::test(instr, 22_u32) ? "B" : "",
      get_condition_mnemonic(instr),
      register_mnemonics[(instr >> 12_u32) & 0xF_u32],
      register_mnemonics[instr & 0xF_u32],
      register_mnemonics[(instr >> 16_u32) & 0xF_u32]);
}

std::string halfword_data_transfer_reg(const u32 /*addr*/, const u32 /*instr*/) noexcept
{
    return "halfword_data_transfer_reg";
}

std::string halfword_data_transfer_imm(const u32 /*addr*/, const u32 /*instr*/) noexcept
{
    return "halfword_data_transfer_imm";
}

std::string single_data_transfer(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto get_addr = [](const u32 i) -> std::string {
        const auto rn = register_mnemonics[(i >> 16_u32) & 0xF_u32];
        const auto updown = bit::test(i, 23_u32) ? ""sv : "-"sv;
        const u32 offset = i & 0xFFF_u32;

        // pre indexing
        if(bit::test(i, 24_u32)) {
            const auto write_back = bit::test(i, 21_u32) ? "!"sv : ""sv;

            if(offset == 0_u32) {
                return fmt::format("[{}]", rn);
            }

            // imm src
            if(!bit::test(i, 25_u32)) {
                return fmt::format("[{},{}0x{:02X}]{}", rn, updown, offset, write_back);
            }

            // reg src
            const u32 rm = offset & 0xF_u32;
            const u32 shift_amount = (offset >> 7_u32) & 0xF_u32;
            const u32 shift_type = (offset >> 5_u32) & 0x2_u32;

            if(shift_type == 0_u32 && shift_amount == 0_u32) {
                return fmt::format("[{},{}{}]{}", rn, updown, register_mnemonics[rm], write_back);
            }

            return fmt::format("[{},{}{},{} 0x{:X}]{}", rn, updown, register_mnemonics[rm],
              shift_mnemonics[shift_type], shift_amount, write_back);
        }

        // pre indexing

        // imm src
        if(!bit::test(i, 25_u32)) {
            return fmt::format("[{}],{}0x{:02X}", rn, updown, offset);
        }

        // reg src
        const u32 rm = offset & 0xF_u32;
        const u32 shift_amount = (offset >> 7_u32) & 0xF_u32;
        const u32 shift_type = (offset >> 5_u32) & 0x2_u32;

        if(shift_type == 0_u32 && shift_amount == 0_u32) {
            return fmt::format("[{}],{}{}", rn, updown, register_mnemonics[rm]);
        }

        return fmt::format("[{}],{}{},{} 0x{:X}", rn, updown, register_mnemonics[rm],
          shift_mnemonics[shift_type], shift_amount);
    };

    static constexpr array op_mnemonics{
      "STR"sv, "LDR"sv, "STRB"sv, "LDRB"sv,
      "STRT"sv, "LDRT"sv, "STRBT"sv, "LDRBT"sv,
    };

    const bool is_load = bit::test(instr, 20_u32);
    const u32 op = bit::from_bool(is_load)
      | (bit::extract(instr, 22_u32) << 1_u32)
      | (bit::from_bool(!bit::test(instr, 24_u32) && bit::test(instr, 21_u32)) << 2_u32);

    return fmt::format("{}{} {},{}", op_mnemonics[op], get_condition_mnemonic(instr),
      register_mnemonics[(instr >> 12_u32) & 0xF_u32], get_addr(instr));
}

std::string undefined(const u32 /*addr*/, const u32 /*instr*/) noexcept
{
    return "undefined";
}

std::string block_data_transfer(const u32 /*addr*/, const u32 instr) noexcept
{
    const auto get_target_reg_list = [](const u32 inst) -> std::string {
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
        for(u32 i = 0_u32; i < 16_u32; ++i) {
            if(bit::test(inst, i)) {
                if(range_count == 0_u32) { range_start = i; }
                ++range_count;
            } else {
                print_range(stream, range_count, range_start, i);
                range_count = 0_u32;
            }
        }

        print_range(stream, range_count, range_start, 16_u32);
        auto str = stream.str();
        if(!str.empty() && str.back() == ',') { str.pop_back(); }
        return str;
    };

    const u32 op = ((instr >> 23_u32) & 0b11_u32) | bit::extract(instr, 20) << 2_u32;
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
      bit::test(instr, 21_u32) ? "!" : "", // write back
      get_target_reg_list(instr), bit::test(instr, 22_u32) ? "^" : "");
}

std::string branch_link(const u32 addr, const u32 instr) noexcept
{
    const i32 sign_extended_offset = math::sign_extend<26>((instr & 0xFFFFFF_u32) << 2_u32);

    return fmt::format("B{}{} 0x{:0>8X}",
      bit::test(instr, 24_u32) ? "L" : "",
      get_condition_mnemonic(instr),
      addr + sign_extended_offset + 8_u32);
}

std::string swi(const u32 /*addr*/, const u32 instr) noexcept
{
    return fmt::format("SWI{} 0x{:0>8X}", get_condition_mnemonic(instr), instr & 0xFFFFFF_u32);
}

/* arm disassemble end */
/* thumb disassemble */

// todo

/* thumb disassemble end */

} // namespace

/*
 * Mnemonic Instruction                     Action                          See Section:
 * ----------------------------------------------------------------------------------------
 * ADC      Add with carry                  Rd := Rn + Op2 + Carry          4.5             x
 * ADD      Add                             Rd := Rn + Op                   24.5            x
 * AND      AND                             Rd := Rn AND Op                 24.5            x
 * B        Branch                          R15 := address                  4.4             x
 * BIC      Bit Clear                       Rd := Rn AND NOT Op             24.5            x
 * BL       Branch with Link                R14 := R15, R15 := address      4.4             x
 * BX       Branch and Exchange             R15 := Rn,T bit := Rn[0]        4.3             x
 * CDP      Coprocesor Data Processing      (Coprocessor-specific)          4.14
 * CMN      Compare Negative                CPSR flags := Rn + Op           24.5            x
 * CMP      Compare                         CPSR flags := Rn - Op           24.5            x
 * EOR      Exclusive OR                    Rd := (Rn AND NOT Op2)          4.5             x
                                            OR (op2 AND NOT Rn)
 * LDC      Load coprocessor from memory    Coprocessor load                4.15
 * LDM      Load multiple registers         Stack manipulation (Pop)        4.11            x
 * LDR      Load register from memory       Rd := (address)                 4.9, 4.10       x
 * MCR      Move CPU register to            cRn := rRn {<op>cRm}            4.16
            coprocessor register
 * MLA      Multiply Accumulate             Rd := (Rm * Rs) + Rn            4.7, 4.8        x
 * MOV      Move register or constant       Rd : = Op                       24.5            x
 * MRC      Move from coprocessor           Rn := cRn {<op>cRm}             4.16
            register to CPU register
 * MRS      Move PSR status/flags to        Rn := PSR                       4.6
            register
 * MSR      Move register to PSR            PSR := Rm                       4.6
            status/flags
 * MUL      Multiply                        Rd := Rm * Rs                   4.7, 4.8        x
 * MVN      Move negative register          Rd := 0xFFFFFFFF EOR Op         24.5            x
 * ORR      OR                              Rd := Rn OR Op                  24.5            x
 * RSB      Reverse Subtract                Rd := Op2 - Rn                  4.5             x
 * RSC      Reverse Subtract with Carry     Rd := Op2 - Rn - 1 + Carry      4.5             x
 * SBC      Subtract with Carry             Rd := Rn - Op2 - 1 + Carry      4.5             x
 * STC      Store coprocessor register to   address := CRn                  4.15
            memory
 * STM      Store Multiple                  Stack manipulation (Push)       4.11            x
 * STR      Store register to memory        <address> := Rd                 4.9, 4.10       x
 * SUB      Subtract                        Rd := Rn - Op                   24.5            x
 * SWI      Software Interrupt              OS call                         4.13            x
 * SWP      Swap register with memory       Rd := [Rn], [Rn] := Rm          4.12            x
 * TEQ      Test bitwise equality           CPSR flags := Rn EOR Op         24.5            x
 * TST      Test bits                       CPSR flags := Rn AND Op         24.5            x
 */

/**
  |..3 ..................2 ..................1 ..................0|
  |1_0_9_8_7_6_5_4_3_2_1_0_9_8_7_6_5_4_3_2_1_0_9_8_7_6_5_4_3_2_1_0|
  |_Cond__|0_0_0|___Op__|S|__Rn___|__Rd___|__Shift__|Typ|0|__Rm___| DataProc        x
  |_Cond__|0_0_0|___Op__|S|__Rn___|__Rd___|__Rs___|0|Typ|1|__Rm___| DataProc        x
  |_Cond__|0_0_1|___Op__|S|__Rn___|__Rd___|_Shift_|___Immediate___| DataProc        x
  |_Cond__|0_0_1_1_0|P|1|0|_Field_|__Rd___|_Shift_|___Immediate___| PSR Imm         x
  |_Cond__|0_0_0_1_0|P|L|0|_Field_|__Rd___|0_0_0_0|0_0_0_0|__Rm___| PSR Reg         x
  |_Cond__|0_0_0_1_0_0_1_0_1_1_1_1_1_1_1_1_1_1_1_1_0_0_0_1|__Rn___| BX              x
  |_Cond__|0_0_0_0_0_0|A|S|__Rd___|__Rn___|__Rs___|1_0_0_1|__Rm___| Multiply        x
  |_Cond__|0_0_0_0_1|U|A|S|_RdHi__|_RdLo__|__Rs___|1_0_0_1|__Rm___| MulLong         x
  |_Cond__|0_0_0_1_0|B|0_0|__Rn___|__Rd___|0_0_0_0|1_0_0_1|__Rm___| TransSwp12      x
  |_Cond__|0_0_0|P|U|0|W|L|__Rn___|__Rd___|0_0_0_0|1|S|H|1|__Rm___| TransReg10      x   // Halfword Data Transfer: register offset
  |_Cond__|0_0_0|P|U|1|W|L|__Rn___|__Rd___|OffsetH|1|S|H|1|OffsetL| TransImm10      x   // Halfword Data Transfer: immediate offset
  |_Cond__|0_1_0|P|U|B|W|L|__Rn___|__Rd___|_________Offset________| TransImm9       x   // single data transfer (imm)
  |_Cond__|0_1_1|P|U|B|W|L|__Rn___|__Rd___|__Shift__|Typ|0|__Rm___| TransReg9       x   // single data transfer (reg)
  |_Cond__|0_1_1|________________xxx____________________|1|__xxx__| Undefined       x
  |_Cond__|1_0_0|P|U|S|W|L|__Rn___|__________Register_List________| BlockTrans      x
  |_Cond__|1_0_1|L|___________________Offset______________________| B,BL,BLX        x
  |_Cond__|1_1_0|P|U|N|W|L|__Rn___|__CRd__|__CP#__|____Offset_____| CoDataTrans
  |_Cond__|1_1_1_0|_CPopc_|__CRn__|__CRd__|__CP#__|_CP__|0|__CRm__| CoDataOp
  |_Cond__|1_1_1_0|CPopc|L|__CRn__|__Rd___|__CP#__|_CP__|1|__CRm__| CoRegTrans
  |_Cond__|1_1_1_1|_____________Ignored_by_Processor______________| SWI             x
*/
disassembler::disassembler() noexcept
  : arm_table_{
      {"000xxxxxxxx0", function_ptr{&data_processing}},
      {"000xxxxx0xx1", function_ptr{&data_processing}},
      {"001xxxxxxxxx", function_ptr{&data_processing}},
      {"000100100001", function_ptr{&branch_exchange}},
      {"000xx0xx1xx1", function_ptr{&halfword_data_transfer_reg}},
      {"000xx1xx1xx1", function_ptr{&halfword_data_transfer_imm}},
      {"00110x10xxxx", function_ptr{&psr_transfer}},
      {"00010xx00000", function_ptr{&psr_transfer}},
      {"000000xx1001", function_ptr{&multiply}},
      {"00001xxx1001", function_ptr{&multiply}},
      {"00010x001001", function_ptr{&single_data_swap}},
      {"010xxxxxxxxx", function_ptr{&single_data_transfer}},
      {"011xxxxxxxx0", function_ptr{&single_data_transfer}},
      {"011xxxxxxxx1", function_ptr{&undefined}},
      {"100xxxxxxxxx", function_ptr{&block_data_transfer}},
      {"101xxxxxxxxx", function_ptr{&branch_link}},
      {"1111xxxxxxxx", function_ptr{&swi}},
    },
    thumb_table_{

    },
    window_{sf::VideoMode(500, 500), "GBA Debugger"}
{
    window_.resetGLStates();
    window_.setFramerateLimit(60);
    ImGui::SFML::Init(window_);

    LOG_INFO("arm table size: {}", sizeof(arm_table_));
    LOG_INFO("thumb table size: {}", sizeof(thumb_table_));
}

void disassembler::update(const vector<u8>& data)
{
    sf::Event e; // NOLINT
    while(window_.pollEvent(e)) {
        if(e.type == sf::Event::Closed) { std::exit(0); }
        ImGui::SFML::ProcessEvent(e);
    }

    ImGui::SFML::Update(window_, dt.restart());

    if(ImGui::Begin("Disassembly")) {
        static bool arm = true;
        ImGui::Checkbox("disassemble arm", &arm);

        if(ImGui::BeginChild("#ididid")) {
            ImGuiListClipper clipper(data.size().get() / (arm ? 4 : 2));
            while(clipper.Step()) {
                if(arm) {
                    for(auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto address = 4_u32 * i;
                        u32 inst;
                        std::memcpy(&inst, data.data() + address, sizeof(inst));
                        ImGui::Text("0x%08X|0x%08X %s", address, inst.get(), disassemble_arm(address, inst).c_str());
                    }
                } else {
                    for(auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto address = 2_u32 * i;
                        u16 inst;
                        std::memcpy(&inst, data.data() + address, sizeof(inst));
                        ImGui::Text("0x%08X|0x%04X %s", address, inst.get(), disassemble_thumb(address, inst).c_str());
                    }
                }
            }
        }

        ImGui::EndChild();
    }
    ImGui::End();

    if(ImGui::Begin("Single Disassembly")) {
        const u32::type i = 0x0329F107u;
        const u32::type a = 0x0329F107u;
        ImGui::Text("0x%08X|0x%08X %s", a, i, disassemble_arm(a, i).c_str());
    }
    ImGui::End();

    window_.clear(sf::Color::Black);
    ImGui::SFML::Render();
    window_.display();
}

std::string disassembler::disassemble_arm(const u32 address, const u32 instruction) const noexcept
{
    if(auto func = arm_table_[((instruction >> 16_u32) & 0xFF0_u32) | (instruction >> 4_u32) & 0xF_u32]) {
        return func(address, instruction);
    }

    return std::string{unknown_instruction};
}

std::string disassembler::disassemble_thumb(const u32 address, const u16 instruction) const noexcept
{
    if(auto func = thumb_table_[address >> 6_u32]) {
        return func(address, instruction);
    }

    return std::string{unknown_instruction};
}

} // namespace gba::debugger
