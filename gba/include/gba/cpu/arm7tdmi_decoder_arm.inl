/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */


#ifndef GAMEBOIADVANCE_ARM7TDMI_DECODER_ARM_H
#define GAMEBOIADVANCE_ARM7TDMI_DECODER_ARM_H

namespace gba::cpu {

namespace detail {

template<bool ShouldAdd>
FORCEINLINE constexpr u32 offset_addr(const u32 addr, const u32 offset) noexcept
{
    if constexpr(ShouldAdd) { return addr + offset; }
    else { return addr - offset; }
}

} // namespace detail

inline void arm7tdmi::branch_exchange(const u32 instr) noexcept
{
    const u32 addr = r_[instr & 0xF_u32];
    if(bit::test(addr, 0_u8)) {
        pc() = bit::clear(addr, 0_u8);
        cpsr().t = true;
        pipeline_flush<instruction_mode::thumb>();
    } else {
        pc() = mask::clear(addr, 0b11_u32);
        pipeline_flush<instruction_mode::arm>();
    }
}

template<bool WithLink>
void arm7tdmi::branch_with_link(const u32 instr) noexcept
{
    if constexpr(WithLink) {
        lr() = pc() - 4_u32;
    }

    pc() += math::sign_extend<26>((instr & 0x00FF'FFFF_u32) << 2_u32);
    pipeline_flush<instruction_mode::arm>();
}

template<bool HasImmediateOp2, arm7tdmi::arm_alu_opcode OpCode, bool ShouldSetCond, u32::type Op2>
void arm7tdmi::data_processing(const u32 instr) noexcept
{
    constexpr u32 op2 = Op2;
    constexpr auto shift_type = to_enum<barrel_shift_type>((op2 >> 1_u32) & 0b11_u32);
    constexpr bool shift_by_imm = !bit::test(op2, 0_u8);

    bool carry = cpsr().c;
    const u32 rn = (instr >> 16_u8) & 0xF_u8;
    u32 first_op;
    u32 second_op;

    pipeline_.fetch_type = mem_access::seq;

    if constexpr(HasImmediateOp2) {
        first_op = r_[rn];
        second_op = instr & 0xFF_u32;

        if(const u8 imm_shift = narrow<u8>((instr >> 8_u32) & 0xF_u32); imm_shift > 0_u8) {
            const auto ror = math::logical_rotate_right(second_op, imm_shift << 1_u8);
            second_op = ror.result;
            carry = static_cast<bool>(ror.carry.get());
        }
    } else {
        u8 shift_amount;

        if constexpr(shift_by_imm) {
            shift_amount = narrow<u8>((instr >> 7_u32) & 0x1F_u32);
        } else {
            shift_amount = narrow<u8>(r_[(instr >> 8_u32) & 0xF_u32]);
            pc() += 4_u32;
            bus_->idle();
        }

        first_op = r_[rn];
        second_op = r_[instr & 0xF_u32];
        alu_barrel_shift(shift_type, second_op, shift_amount, carry, shift_by_imm);
    }

    const u32 dest = (instr >> 12_u8) & 0xF_u8;
    u32& rd = r_[dest];

    const auto do_set_flags = [&](const u32 expression) {
        cpsr().n = bit::test(expression, 31_u8);
        cpsr().z = expression == 0_u32;
        cpsr().c = carry;
    };

    const auto evaluate_and_set_flags = [&](const u32 expression) {
        if constexpr(ShouldSetCond) {
            do_set_flags(expression);
        }
        return expression;
    };

    switch(OpCode) {
        case arm_alu_opcode::and_:
            rd = evaluate_and_set_flags(first_op & second_op);
            break;
        case arm_alu_opcode::eor:
            rd = evaluate_and_set_flags(first_op ^ second_op);
            break;
        case arm_alu_opcode::sub:
            rd = alu_sub(first_op, second_op, ShouldSetCond);
            break;
        case arm_alu_opcode::rsb:
            rd = alu_sub(second_op, first_op, ShouldSetCond);
            break;
        case arm_alu_opcode::add:
            rd = alu_add(first_op, second_op, ShouldSetCond);
            break;
        case arm_alu_opcode::adc:
            rd = alu_adc(first_op, second_op, ShouldSetCond);
            break;
        case arm_alu_opcode::sbc:
            rd = alu_sbc(first_op, second_op, ShouldSetCond);
            break;
        case arm_alu_opcode::rsc:
            rd = alu_sbc(second_op, first_op, ShouldSetCond);
            break;
        case arm_alu_opcode::tst: {
            const u32 result = first_op & second_op;
            do_set_flags(result);
            break;
        }
        case arm_alu_opcode::teq: {
            const u32 result = first_op ^ second_op;
            do_set_flags(result);
            break;
        }
        case arm_alu_opcode::cmp:
            alu_sub(first_op, second_op, true);
            break;
        case arm_alu_opcode::cmn:
            alu_add(first_op, second_op, true);
            break;
        case arm_alu_opcode::orr:
            rd = evaluate_and_set_flags(first_op | second_op);
            break;
        case arm_alu_opcode::mov:
            rd = evaluate_and_set_flags(second_op);
            break;
        case arm_alu_opcode::bic:
            rd = evaluate_and_set_flags(first_op & ~second_op);
            break;
        case arm_alu_opcode::mvn:
            rd = evaluate_and_set_flags(~second_op);
            break;
        default:
            UNREACHABLE();
    }

    if(dest == 15_u32) {
        if constexpr(ShouldSetCond) {
            if(in_exception_mode()) {
                cpsr().copy_without_mode(spsr());
                switch_mode(spsr().mode);
            }
        }

        if constexpr(OpCode < arm_alu_opcode::tst || arm_alu_opcode::cmn < OpCode) {
            if(cpsr().t) {
                pipeline_flush<instruction_mode::thumb>();
            } else {
                pipeline_flush<instruction_mode::arm>();
            }
        } else if constexpr(HasImmediateOp2 || shift_by_imm) {
            pc() += 4_u32;
        }
    } else if constexpr(HasImmediateOp2 || shift_by_imm) {
        pc() += 4_u32;
    }
}

template<bool HasImmediateSrc, bool UseSPSR, arm7tdmi::psr_transfer_opcode OpCode>
void arm7tdmi::psr_transfer(const u32 instr) noexcept
{
    switch(OpCode) {
        case psr_transfer_opcode::mrs: {
            const u32 rd = (instr >> 12_u32) & 0xF_u32;
            ASSERT(rd != 15_u32);

            if(UseSPSR && in_exception_mode()) {
                r_[rd] = static_cast<u32>(spsr());
            } else {
                r_[rd] = static_cast<u32>(cpsr());
            }
            break;
        }
        case psr_transfer_opcode::msr: {
            const u32 src = [&]() {
                if constexpr(HasImmediateSrc) {
                    return math::logical_rotate_right(instr & 0xFF_u32,
                      narrow<u8>(((instr >> 8_u32) & 0xF_u32) << 1_u32)).result;
                } else {
                    const u32 rm = instr & 0xF_u32;
                    ASSERT(rm != 15_u32);
                    return r_[rm];
                }
            }();

            u32 mask;
            if(bit::test(instr, 19_u8)) { mask |= 0xF000'0000_u32; }
            if(bit::test(instr, 16_u8) && (UseSPSR || in_privileged_mode())) { mask |= 0x0000'00FF_u32; }

            if constexpr(UseSPSR) {
                if(in_exception_mode()) {
                    psr& reg = spsr();
                    reg = mask::clear(static_cast<u32>(reg), mask) | (src & mask);
                }
            } else {
                if((src & 0xFF_u32) != 0x00_u32) {
                    switch_mode(to_enum<privilege_mode>(src & 0x1F_u32));
                }
                const u32 new_cpsr = mask::clear(static_cast<u32>(cpsr()), mask) | (src & mask);
                cpsr() = new_cpsr;
            }
            break;
        }
        default:
            UNREACHABLE();
    }

    pipeline_.fetch_type = mem_access::seq;
    pc() += 4_u32;
}

template<bool ShouldAccumulate, bool ShouldSetCond>
void arm7tdmi::multiply(const u32 instr) noexcept
{
    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u32;

    u32& rd = r_[(instr >> 16_u32) & 0xF_u32];
    const u32 rs = r_[(instr >> 8_u32) & 0xF_u32];
    const u32 rm = r_[instr & 0xF_u32];

    ASSERT(((instr >> 16_u32) & 0xF_u32) != 15_u32);
    ASSERT(((instr >> 8_u32) & 0xF_u32) != 15_u32);
    ASSERT((instr & 0xF_u32) != 15_u32);

    alu_multiply_internal(rs, [](const u32 r, const u32 mask) {
        return r == 0_u32 || r == mask;
    });

    u32 result = rm * rs;

    if constexpr(ShouldAccumulate) {
        const u32 rn = (instr >> 12_u32) & 0xF_u32;
        ASSERT(rn != 15_u32);
        result += r_[rn];
        bus_->idle();
    }

    if constexpr(ShouldSetCond) {
        cpsr().z = result == 0_u32;
        cpsr().n = bit::test(result, 31_u8);
    }

    rd = result;
}

template<bool IsSigned, bool ShouldAccumulate, bool ShouldSetCond>
void arm7tdmi::multiply_long(const u32 instr) noexcept
{
    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u32;

    u32& rdhi = r_[(instr >> 16_u32) & 0xF_u32];
    u32& rdlo = r_[(instr >> 12_u32) & 0xF_u32];
    const u32 rs = r_[(instr >> 8_u32) & 0xF_u32];
    const u32 rm = r_[instr & 0xF_u32];

    ASSERT(((instr >> 16_u32) & 0xF_u32) != (instr & 0xF_u32) &&
    ((instr >> 12_u32) & 0xF_u32) != (instr & 0xF_u32) &&
    ((instr >> 12_u32) & 0xF_u32) != ((instr >> 16_u32) & 0xF_u32));
    ASSERT(((instr >> 16_u32) & 0xF_u32) != 15_u32);
    ASSERT(((instr >> 12_u32) & 0xF_u32) != 15_u32);
    ASSERT(((instr >> 8_u32) & 0xF_u32) != 15_u32);
    ASSERT((instr & 0xF_u32) != 15_u32);

    bus_->idle();

    i64 result;
    if constexpr(IsSigned) {
        alu_multiply_internal(rs, [](const u64 r, const u64 mask) {
            return r == 0_u32 || r == mask;
        });

        result = math::sign_extend<32>(widen<u64>(rm)) * math::sign_extend<32>(widen<u64>(rs));
    } else {
        alu_multiply_internal(rs, [](const u32 r, const u32 /*mask*/) {
            return r == 0_u32;
        });

        result = make_signed(widen<u64>(rm) * rs);
    }

    if constexpr(ShouldAccumulate) {
        result += (widen<u64>(rdhi) << 32_u64) | rdlo;
        bus_->idle();
    }

    if constexpr(ShouldSetCond) {
        cpsr().z = result == 0_i64;
        cpsr().n = result < 0_i64;
    }

    rdhi = narrow<u32>(make_unsigned(result) >> 32_u64);
    rdlo = narrow<u32>(make_unsigned(result));
}

template<bool HasImmediateOffset, bool HasPreIndexing,
  bool ShouldAddToBase, bool IsByteTransfer, bool ShouldWriteback, bool IsLoad>
void arm7tdmi::single_data_transfer(const u32 instr) noexcept
{
    constexpr bool writeback_enabled = ShouldWriteback || !HasPreIndexing;
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;

    u32 rn_addr = r_[rn];

    const u32 offset = [=]() {
        if constexpr(HasImmediateOffset) {
            return instr & 0xFFF_u32;
        } else {
            const auto shift_type = static_cast<barrel_shift_type>(((instr >> 5_u32) & 0b11_u32).get());
            const u8 shift_amount = narrow<u8>((instr >> 7_u32) & 0x1F_u32);
            u32 rm = r_[instr & 0xF_u32];
            bool dummy = cpsr().c;
            alu_barrel_shift(shift_type, rm, shift_amount, dummy, true);
            return rm;
        }
    }();

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u32;

    if constexpr(HasPreIndexing) {
        rn_addr = detail::offset_addr<ShouldAddToBase>(rn_addr, offset);
    }

    if constexpr(IsLoad) {
        u32 data;
        if constexpr(IsByteTransfer) {
            data = bus_->read_8(rn_addr, mem_access::non_seq);
        } else {
            data = read_32_aligned(rn_addr, mem_access::non_seq);
        }

        if constexpr(writeback_enabled) {
            r_[rn] = detail::offset_addr<ShouldAddToBase>(r_[rn], offset);
        }

        bus_->idle();
        r_[rd] = data;
    } else {
        if constexpr(IsByteTransfer) {
            bus_->write_8(rn_addr, narrow<u8>(r_[rd]), mem_access::non_seq);
        } else {
            bus_->write_32(rn_addr, r_[rd], mem_access::non_seq);
        }

        if constexpr(writeback_enabled) {
            r_[rn] = detail::offset_addr<ShouldAddToBase>(r_[rn], offset);
        }
    }

    if constexpr(IsLoad) {
        if(rd == 15_u32) {
            pipeline_flush<instruction_mode::arm>();
        }
    }
}

template<bool HasPreIndexing, bool ShouldAddToBase,
  bool HasImmediateOffset, bool ShouldWriteback, bool IsLoad, arm7tdmi::halfword_data_transfer_opcode OpCode>
void arm7tdmi::halfword_data_transfer(const u32 instr) noexcept
{
    static_assert(from_enum<u32>(OpCode) != 0_u32); // this is reserved
    constexpr bool writeback_enabled = ShouldWriteback || !HasPreIndexing;

    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;

    u32 rn_addr = r_[rn];

    const u32 offset = [=]() {
        if constexpr(HasImmediateOffset) {
            return ((instr >> 4_u32) & 0xF0_u32) | (instr & 0xF_u32);
        } else {
            const u32 rm = instr & 0xF_u32;
            ASSERT(rm != 15_u32);
            return r_[rm];
        }
    }();

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u32;

    if constexpr(HasPreIndexing) {
        rn_addr = detail::offset_addr<ShouldAddToBase>(rn_addr, offset);
    }

    if constexpr(IsLoad) {
        u32 data;
        switch(OpCode) {
            case halfword_data_transfer_opcode::ldrh:
                data = read_16_aligned(rn_addr, mem_access::non_seq);
                break;
            case halfword_data_transfer_opcode::ldrsb:
                data = read_8_signed(rn_addr, mem_access::non_seq);
                break;
            case halfword_data_transfer_opcode::ldrsh:
                data = read_16_signed(rn_addr, mem_access::non_seq);
                break;
            default:
                UNREACHABLE();
        }

        if constexpr(writeback_enabled) {
            r_[rn] = detail::offset_addr<ShouldAddToBase>(r_[rn], offset);
        }

        bus_->idle();
        r_[rd] = data;
    } else {
        switch(OpCode) {
            case halfword_data_transfer_opcode::strh:
                bus_->write_16(rn_addr, narrow<u16>(r_[rd]), mem_access::non_seq);
                if constexpr(writeback_enabled) {
                    r_[rn] = detail::offset_addr<ShouldAddToBase>(r_[rn], offset);
                }
                break;
            case halfword_data_transfer_opcode::ldrd:
                bus_->idle();
                if constexpr(writeback_enabled) {
                    r_[rn] = detail::offset_addr<ShouldAddToBase>(r_[rn], offset);
                }
                bus_->idle();
                break;
            case halfword_data_transfer_opcode::strd:
                bus_->idle();
                if constexpr(writeback_enabled) {
                    r_[rn] = detail::offset_addr<ShouldAddToBase>(r_[rn], offset);
                }
                break;
            default:
                UNREACHABLE();
        }
    }

    if constexpr(IsLoad) {
        if(rd == 15_u32) {
            pipeline_flush<instruction_mode::arm>();
        }
    }
}

template<bool HasPreIndexing, bool ShouldAddToBase, bool LoadPSR_ForceUser, bool ShouldWriteback, bool IsLoad>
void arm7tdmi::block_data_transfer(const u32 instr) noexcept
{
    bool has_pre_indexing = HasPreIndexing;
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    ASSERT(rn != 15_u32);

    bool transfer_pc = bit::test(instr, 15_u8);
    auto rlist = generate_register_list<16>(instr);
    u32 offset = narrow<u32>(rlist.size()) * 4_u32;

    // Empty Rlist: R15 loaded/stored, and Rb=Rb+/-40h.
    if(rlist.empty()) {
        rlist.push_back(15_u8);
        offset = 0x40_u32;
        transfer_pc = true;
    }

    u32 rn_addr = r_[rn];
    u32 rn_addr_writeback = rn_addr;

    const bool should_switch_mode = LoadPSR_ForceUser && (!IsLoad || !transfer_pc);

    const privilege_mode old_mode = cpsr().mode;
    if(should_switch_mode) {
        switch_mode(privilege_mode::usr);
    }

    // for DECREASING addressing modes, the CPU does first calculate the lowest address,
    // and does then process rlist with increasing addresses
    if constexpr(!ShouldAddToBase) {
        has_pre_indexing = !has_pre_indexing;
        rn_addr -= offset;
        rn_addr_writeback -= offset;
    } else {
        rn_addr_writeback += offset;
    }

    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u32;

    auto access_type = mem_access::non_seq;
    enumerate(rlist, [&](const usize idx, const u32 reg) {
        if(has_pre_indexing) {
            rn_addr += 4_u32;
        }

        if constexpr(IsLoad) {
            const u32 data = bus_->read_32(rn_addr, access_type);
            if constexpr(ShouldWriteback) {
                if(idx == 0_usize) {
                    r_[rn] = rn_addr_writeback;
                }
            }
            r_[reg] = data;
        } else {
            bus_->write_32(rn_addr, r_[reg], access_type);
            if constexpr(ShouldWriteback) {
                if(idx == 0_usize) {
                    r_[rn] = rn_addr_writeback;
                }
            }
        }

        if(!has_pre_indexing) {
            rn_addr += 4_u32;
        }

        access_type = mem_access::seq;
    });

    if constexpr(IsLoad) {
        bus_->idle();

        if(transfer_pc) {
            if constexpr(LoadPSR_ForceUser) {
                if(in_exception_mode()) {
                    cpsr().copy_without_mode(spsr());
                    switch_mode(spsr().mode);
                }
            }

            if(cpsr().t) {
                pipeline_flush<instruction_mode::thumb>();
            } else {
                pipeline_flush<instruction_mode::arm>();
            }
        }
    }

    if(should_switch_mode) {
        switch_mode(old_mode);
    }
}

template<bool IsByteTransfer>
void arm7tdmi::single_data_swap(const u32 instr) noexcept
{
    pipeline_.fetch_type = mem_access::non_seq;
    pc() += 4_u8;

    const u32 rm = instr & 0xF_u32;
    const u32 rd = (instr >> 12_u32) & 0xF_u32;
    const u32 rn = (instr >> 16_u32) & 0xF_u32;
    const u32 rn_addr = r_[rn];

    ASSERT(rm != 15_u32);
    ASSERT(rd != 15_u32);
    ASSERT(rn != 15_u32);

    u32 data;
    if constexpr(IsByteTransfer) {
        data = bus_->read_8(rn_addr, mem_access::non_seq);
        bus_->write_8(rn_addr, narrow<u8>(r_[rm]), mem_access::non_seq);
    } else {
        data = read_32_aligned(rn_addr, mem_access::non_seq);
        bus_->write_32(rn_addr, r_[rm], mem_access::non_seq);
    }

    bus_->idle();
    r_[rd] = data;
    if(rd == 15_u32) {
        pipeline_flush<instruction_mode::arm>();
    }
}

inline void arm7tdmi::swi_arm(const u32 /*instr*/) noexcept
{
    spsr_banks_[register_bank::svc] = cpsr();
    switch_mode(privilege_mode::svc);
    cpsr().i = true;
    lr() = pc() - 4_u32;
    pc() = 0x0000'0008_u32;
    pipeline_flush<instruction_mode::arm>();
}

inline void arm7tdmi::undefined(const u32 /*instr*/) noexcept
{
    spsr_banks_[register_bank::und] = cpsr();
    switch_mode(privilege_mode::und);
    cpsr().i = true;
    lr() = pc() - 4_u32;
    pc() = 0x0000'0004_u32;
    pipeline_flush<instruction_mode::arm>();
}

} // namespace gba::cpu

#endif //GAMEBOIADVANCE_ARM7TDMI_DECODER_ARM_H
