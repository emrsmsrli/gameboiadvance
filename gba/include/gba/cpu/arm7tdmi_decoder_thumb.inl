/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM7TDMI_DECODER_THUMB_H
#define GAMEBOIADVANCE_ARM7TDMI_DECODER_THUMB_H

namespace gba::cpu {

template<arm7tdmi::move_shifted_reg_opcode OpCode>
void arm7tdmi::move_shifted_reg(const u16 instr) noexcept
{
    const u8 offset = narrow<u8>((instr >> 6_u16) & 0x1F_u16);
    u32 rs = r_[(instr >> 3_u16) & 0x7_u16];
    u32& rd = r_[instr & 0x7_u16];
    bool carry = cpsr().c;

    alu_barrel_shift(static_cast<barrel_shift_type>(OpCode), rs, offset, carry, true);
    rd = rs;

    cpsr().z = rs == 0_u32;
    cpsr().n = bit::test(rs, 31_u8);
    cpsr().c = carry;

    pipeline_.fetch_type = mem_access::seq;
    pc() += 2_u32;
}

template<bool HasImmediateOperand, bool IsSub, u16::type Operand>
void arm7tdmi::add_subtract(const u16 instr) noexcept
{
    const u32 data = HasImmediateOperand ? Operand : r_[Operand];
    const u32 rs = r_[(instr >> 3_u16) & 0x7_u16];
    u32& rd = r_[instr & 0x7_u16];

    if constexpr(IsSub) {
        rd = alu_sub(rs, data, true);
    } else {
        rd = alu_add(rs, data, true);
    }

    pipeline_.fetch_type = mem_access::seq;
    pc() += 2_u32;
}

template<arm7tdmi::imm_op_opcode OpCode, u16::type Rd>
void arm7tdmi::mov_cmp_add_sub_imm(const u16 instr) noexcept
{
    const u16 offset = instr & 0xFF_u16;
    switch(OpCode) {
        case imm_op_opcode::mov:
            r_[Rd] = offset;
            cpsr().n = false;
            cpsr().z = offset == 0_u16;
            break;
        case imm_op_opcode::cmp:
            alu_sub(r_[Rd], offset, true);
            break;
        case imm_op_opcode::add:
            r_[Rd] = alu_add(r_[Rd], offset, true);
            break;
        case imm_op_opcode::sub:
            r_[Rd] = alu_sub(r_[Rd], offset, true);
            break;
        default:
            UNREACHABLE();
    }

    pipeline_.fetch_type = mem_access::seq;
    pc() += 2_u32;
}

template<arm7tdmi::thumb_alu_opcode OpCode>
void arm7tdmi::alu(const u16 instr) noexcept
{
    u32 rs = r_[(instr >> 3_u16) & 0x7_u16];
    u32& rd = r_[instr & 0x7_u16];
    bool carry = cpsr().c;

    pipeline_.fetch_type = mem_access::seq;
    pc() += 2_u32;

    const auto evaluate_and_set_flags = [&](const u32 expression) {
        cpsr().n = bit::test(expression, 31_u8);
        cpsr().z = expression == 0_u32;
        return expression;
    };

    switch(OpCode) {
        case thumb_alu_opcode::and_:
            rd = evaluate_and_set_flags(rd & rs);
            break;
        case thumb_alu_opcode::eor:
            rd = evaluate_and_set_flags(rd ^ rs);
            break;
        case thumb_alu_opcode::lsl:
            alu_lsl(rd, narrow<u8>(rs), carry);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            bus_->idle();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        case thumb_alu_opcode::lsr:
            alu_lsr(rd, narrow<u8>(rs), carry, false);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            bus_->idle();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        case thumb_alu_opcode::asr:
            alu_asr(rd, narrow<u8>(rs), carry, false);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            bus_->idle();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        case thumb_alu_opcode::adc:
            rd = alu_adc(rd, rs, true);
            break;
        case thumb_alu_opcode::sbc:
            rd = alu_sbc(rd, rs, true);
            break;
        case thumb_alu_opcode::ror:
            alu_ror(rd, narrow<u8>(rs), carry, false);
            rd = evaluate_and_set_flags(rd);
            cpsr().c = carry;

            bus_->idle();
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        case thumb_alu_opcode::tst:
            evaluate_and_set_flags(rd & rs);
            break;
        case thumb_alu_opcode::neg:
            rd = alu_sub(0_u32, rs, true);
            break;
        case thumb_alu_opcode::cmp:
            alu_sub(rd, rs, true);
            break;
        case thumb_alu_opcode::cmn:
            alu_add(rd, rs, true);
            break;
        case thumb_alu_opcode::orr:
            rd = evaluate_and_set_flags(rd | rs);
            break;
        case thumb_alu_opcode::mul:
            alu_multiply_internal(rd, [](const u32 r, const u32 mask) {
                return r == 0_u32 || r == mask;
            });
            rd = evaluate_and_set_flags(rd * rs);
            cpsr().c = false;
            pipeline_.fetch_type = mem_access::non_seq;
            break;
        case thumb_alu_opcode::bic:
            rd = evaluate_and_set_flags(rd & ~rs);
            break;
        case thumb_alu_opcode::mvn:
            rd = evaluate_and_set_flags(~rs);
            break;
        default:
            UNREACHABLE();
    }
}

template<arm7tdmi::hireg_bx_opcode OpCode>
void arm7tdmi::hireg_bx(const u16 instr) noexcept
{
    const u32 rs_reg = (instr >> 3_u16) & 0xF_u16;
    u32 rs = r_[rs_reg];
    const u32 rd_reg = (instr & 0x7_u16) | ((instr >> 4_u16) & 0x8_u16);
    u32& rd = r_[rd_reg];

    if(rs_reg == 15_u32) {
        rs = bit::clear(rs, 0_u8);
    }

    const auto set_rd_and_flush = [&](const u32 expression) {
        rd = expression;
        if(rd_reg == 15_u32) {
            rd = bit::clear(rd, 0_u8);
            pipeline_flush<instruction_mode::thumb>();
        } else {
            pipeline_.fetch_type = mem_access::seq;
            pc() += 2_u32;
        }
    };

    switch(OpCode) {
        case hireg_bx_opcode::add:
            ASSERT(bit::test(instr, 6_u8) || bit::test(instr, 7_u8));
            set_rd_and_flush(rd + rs);
            break;
        case hireg_bx_opcode::cmp:
            ASSERT(bit::test(instr, 6_u8) || bit::test(instr, 7_u8));
            alu_sub(rd, rs, true);
            pipeline_.fetch_type = mem_access::seq;
            pc() += 2_u32;
            break;
        case hireg_bx_opcode::mov:
            ASSERT(bit::test(instr, 6_u8) || bit::test(instr, 7_u8));
            set_rd_and_flush(rs);
            break;
        case hireg_bx_opcode::bx:
            ASSERT(!bit::test(instr, 7_u8));
            if(bit::test(rs, 0_u8)) {
                pc() = bit::clear(rs, 0_u8);
                pipeline_flush<instruction_mode::thumb>();
            } else {
                cpsr().t = false;
                pc() = mask::clear(rs, 0b11_u32);
                pipeline_flush<instruction_mode::arm>();
            }
            break;
        default:
            UNREACHABLE();
    }
}

inline void arm7tdmi::pc_rel_load(const u16 instr) noexcept
{
    u32& rd = r_[(instr >> 8_u16) & 0x7_u16];
    const u32 addr = bit::clear(pc(), 1_u8) + ((instr & 0xFF_u16) << 2_u16);

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    rd = bus_->read_32(addr, mem_access::non_seq);
    bus_->idle();
}

template<arm7tdmi::ld_str_reg_opcode OpCode>
void arm7tdmi::ld_str_reg(const u16 instr) noexcept
{
    const u32 ro = r_[(instr >> 6_u16) & 0x7_u16];
    const u32 rb = r_[(instr >> 3_u16) & 0x7_u16];
    u32& rd = r_[instr & 0x7_u16];

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    const u32 address = rb + ro;
    switch(OpCode) {
        case ld_str_reg_opcode::str:
            bus_->write_32(address, rd, mem_access::non_seq);
            break;
        case ld_str_reg_opcode::strb:
            bus_->write_8(address, narrow<u8>(rd), mem_access::non_seq);
            break;
        case ld_str_reg_opcode::ldr:
            rd = read_32_aligned(address, mem_access::non_seq);
            bus_->idle();
            break;
        case ld_str_reg_opcode::ldrb:
            rd = bus_->read_8(address, mem_access::non_seq);
            bus_->idle();
            break;
        default:
            UNREACHABLE();
    }
}

template<arm7tdmi::ld_str_sign_extended_byte_hword_opcode OpCode>
void arm7tdmi::ld_str_sign_extended_byte_hword(const u16 instr) noexcept
{
    const u32 ro = r_[(instr >> 6_u16) & 0x7_u16];
    const u32 rb = r_[(instr >> 3_u16) & 0x7_u16];
    u32& rd = r_[instr & 0x7_u16];

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    const u32 address = rb + ro;
    switch(OpCode) {
        case ld_str_sign_extended_byte_hword_opcode::strh:
            bus_->write_16(address, narrow<u16>(rd), mem_access::non_seq);
            break;
        case ld_str_sign_extended_byte_hword_opcode::ldsb:
            rd = read_8_signed(address, mem_access::non_seq);
            bus_->idle();
            break;
        case ld_str_sign_extended_byte_hword_opcode::ldrh:
            rd = read_16_aligned(address, mem_access::non_seq);
            bus_->idle();
            break;
        case ld_str_sign_extended_byte_hword_opcode::ldsh:
            rd = read_16_signed(address, mem_access::non_seq);
            bus_->idle();
            break;
        default:
            UNREACHABLE();
    }
}

template<arm7tdmi::ld_str_imm_opcode OpCode>
void arm7tdmi::ld_str_imm(const u16 instr) noexcept
{
    const u32 imm = (instr >> 6_u16) & 0x1F_u16;
    const u32 rb = r_[(instr >> 3_u16) & 0x7_u16];
    u32& rd = r_[instr & 0x7_u16];

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    switch(OpCode) {
        case ld_str_imm_opcode::str:
            bus_->write_32(rb + (imm << 2_u32), rd, mem_access::non_seq);
            break;
        case ld_str_imm_opcode::ldr:
            rd = read_32_aligned(rb + (imm << 2_u32), mem_access::non_seq);
            bus_->idle();
            break;
        case ld_str_imm_opcode::strb:
            bus_->write_8(rb + imm, narrow<u8>(rd), mem_access::non_seq);
            break;
        case ld_str_imm_opcode::ldrb:
            rd = bus_->read_8(rb + imm, mem_access::non_seq);
            bus_->idle();
            break;
        default:
            UNREACHABLE();
    }
}

template<bool IsLoad>
void arm7tdmi::ld_str_hword(const u16 instr) noexcept
{
    const u16 imm = ((instr >> 6_u16) & 0x1F_u16) << 1_u16;
    const u32 rb = r_[(instr >> 3_u16) & 0x7_u16];
    u32& rd = r_[instr & 0x7_u16];

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    const u32 address = rb + imm;
    if constexpr(IsLoad) {
        rd = read_16_aligned(address, mem_access::non_seq);
        bus_->idle();
    } else {
        bus_->write_16(address, narrow<u16>(rd), mem_access::non_seq);
    }
}

template<bool IsLoad>
void arm7tdmi::ld_str_sp_relative(const u16 instr) noexcept
{
    u32& rd = r_[(instr >> 8_u16) & 0x7_u16];
    const u16 imm_offset = (instr & 0xFF_u16) << 2_u16;

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    const u32 address = sp() + imm_offset;
    if constexpr(IsLoad) {
        rd = read_32_aligned(address, mem_access::non_seq);
        bus_->idle();
    } else {
        bus_->write_32(address, rd, mem_access::non_seq);
    }
}

template<bool UseSP>
void arm7tdmi::ld_addr(const u16 instr) noexcept
{
    u32& rd = r_[(instr >> 8_u16) & 0x7_u16];
    const u16 imm_offset = (instr & 0xFF_u16) << 2_u16;

    if constexpr(UseSP) {
        rd = sp() + imm_offset;
    } else {
        rd = bit::clear(pc(), 1_u8) + imm_offset;
    }

    pipeline_.fetch_type = mem_access::seq;
    pc() += 2_u32;
}

template<bool ShouldSubtract>
void arm7tdmi::add_offset_to_sp(const u16 instr) noexcept
{
    const u16 imm_offset = (instr & 0x7F_u16) << 2_u16;

    if(ShouldSubtract) {
        sp() -= imm_offset;
    } else {
        sp() += imm_offset;
    }

    pipeline_.fetch_type = mem_access::seq;
    pc() += 2_u32;
}

template<bool IsPOP, bool UsePC_LR>
void arm7tdmi::push_pop(const u16 instr) noexcept
{
    const auto rlist = generate_register_list<8>(instr);
    auto access = mem_access::non_seq;

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    if constexpr(!UsePC_LR) {
        if(rlist.empty()) {
            if constexpr(IsPOP) {
                pc() = bus_->read_32(sp(), access);
                pipeline_flush<instruction_mode::thumb>();
                sp() += 0x40_u32;
            } else {
                sp() -= 0x40_u32;
                pipeline_.fetch_type = mem_access::seq;
            }

            return;
        }
    }

    u32 address = sp();

    if constexpr(IsPOP) {
        for(const u8 reg : rlist) {
            r_[reg] = bus_->read_32(address, access);
            access = mem_access::seq;
            address += 4_u32;
        }

        if constexpr(UsePC_LR) {
            pc() = bit::clear(bus_->read_32(address, access), 0_u8);
            sp() = address + 4_u32;
            bus_->idle();
            pipeline_flush<instruction_mode::thumb>();
            return;
        }

        bus_->idle();
        sp() = address;
    } else {
        address -= 4_u32 * narrow<u32>(rlist.size());
        if constexpr(UsePC_LR) {
            address -= 4_u32;
        }

        sp() = address;

        for(const u8 reg : rlist) {
            bus_->write_32(address, r_[reg], access);
            access = mem_access::seq;
            address += 4_u32;
        }

        if constexpr(UsePC_LR) {
            bus_->write_32(address, lr(), access);
        }
    }
}

template<bool IsLoad>
void arm7tdmi::ld_str_multiple(const u16 instr) noexcept
{
    const u32 rb = (instr >> 8_u16) & 0x7_u16;
    const auto rlist = generate_register_list<8>(instr);
    u32& rb_reg = r_[rb];

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 2_u32;

    if(rlist.empty()) {
        if constexpr(IsLoad) {
            pc() = bus_->read_32(rb_reg, mem_access::non_seq);
            pipeline_flush<instruction_mode::thumb>();
        } else {
            pipeline_.fetch_type = mem_access::seq;
            pc() += 2_u32;
            bus_->write_32(rb_reg, pc(), mem_access::non_seq);
        }

        rb_reg += 0x40_u32;
        return;
    }

    u32 address = rb_reg;
    if constexpr(IsLoad) {
        auto access = mem_access::non_seq;
        for(const u8 reg : rlist) {
            r_[reg] = bus_->read_32(address, access);
            access = mem_access::seq;
            address += 4_u32;
        }

        bus_->idle();

        if(!bit::test(instr, narrow<u8>(rb))) {
            rb_reg = address;
        }
    } else {
        const u32 final_addr = address + 4_u32 * narrow<u32>(rlist.size());
        bus_->write_32(address, r_[rlist.front()], mem_access::non_seq);
        rb_reg = final_addr;
        address += 4_u32;

        for(u32 i = 1_u32; i < rlist.size(); ++i) {
            bus_->write_32(address, r_[rlist[i]], mem_access::seq);
            address += 4_u32;
        }
    }
}

template<u32::type Condition>
void arm7tdmi::branch_cond(const u16 instr) noexcept
{
    if(condition_met(Condition)) {
        const i32 offset = math::sign_extend<9>(widen<u32>((instr & 0xFF_u16)) << 1_u32);
        pc() += offset;
        pipeline_flush<instruction_mode::thumb>();
    } else {
        pipeline_.fetch_type = mem_access::seq;
        pc() += 2_u32;
    }
}

inline void arm7tdmi::swi_thumb(const u16 /*instr*/) noexcept
{
    spsr_banks_[register_bank::svc] = cpsr();
    switch_mode(privilege_mode::svc);
    cpsr().i = true;
    cpsr().t = false;
    lr() = pc() - 2_u32;
    pc() = 0x0000'0008_u32;
    pipeline_flush<instruction_mode::arm>();
}

inline void arm7tdmi::branch(const u16 instr) noexcept
{
    const i32 offset = math::sign_extend<12>(widen<u32>((instr & 0x7FF_u16)) << 1_u32);
    pc() += offset;
    pipeline_flush<instruction_mode::thumb>();
}

template<bool IsSecondIntruction>
void arm7tdmi::long_branch_link(const u16 instr) noexcept
{
    const u32 offset = instr & 0x7FF_u16;
    if constexpr(IsSecondIntruction) {
        const u32 temp = pc() - 2_u32;
        pc() = bit::clear(lr() + (offset << 1_u32), 0_u8);
        lr() = bit::set(temp, 0_u8);
        pipeline_flush<instruction_mode::thumb>();
    } else {
        lr() = pc() + math::sign_extend<23>(offset << 12_u32);
        pipeline_.fetch_type = mem_access::seq;
        pc() += 2_u32;
    }
}

} // namespace gba::cpu

#endif //GAMEBOIADVANCE_ARM7TDMI_DECODER_THUMB_H
