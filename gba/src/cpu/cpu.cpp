/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cpu/cpu.h>

namespace gba::cpu {

namespace {

} // namespace

void cpu::cpu::skip_bios() noexcept
{
    if(pc() != 0x0000'0000_u32) {
        LOG_DEBUG(cpu, "skipping bios");
        return;
    }

    reg_banks_[register_bank::none].named.r13 = 0x0300'7F00_u32;
    reg_banks_[register_bank::irq].named.r13 = 0x0300'7FA0_u32;
    reg_banks_[register_bank::svc].named.r13 = 0x0300'7FE0_u32;
    sp() = 0x0300'7F00_u32;
    lr() = 0x0800'0000_u32;
    pc() = 0x0800'0000_u32;
    cpsr().i = false;
    cpsr().f = false;
    cpsr().mode = privilege_mode::sys;
}

void cpu::tick() noexcept
{
    if(UNLIKELY(haltcnt_ == halt_control::halted && interrupt_available())) {
        haltcnt_ = halt_control::running;
    }

    if(LIKELY(haltcnt_ == halt_control::running)) {
        execute_instruction();
    } else {
        bus_->tick_components(scheduler_->remaining_cycles_to_next_event());
    }
}

u32 cpu::read_bios(u32 addr) noexcept
{
    const u32 shift = (addr & 0b11_u32) << 3_u32;
    addr = mask::clear(addr, 0b11_u32);

    if(UNLIKELY(addr >= 0x0000'4000_u32)) {
        return read_unused(addr, mem_access::none) >> shift; // dma can't access bios
    }

    if(pc() < 0x0000'4000_u32) {
        bios_last_read_ = memcpy<u32>(bios_, addr);
    }
    return bios_last_read_ >> shift;
}

u32 cpu::read_unused(const u32 addr, const mem_access access) noexcept
{
    const u32 alignment = (addr & 0b11_u32) << 3_u32;
    if(UNLIKELY(bitflags::is_set(access, mem_access::dma))) {
        return dma_controller_.latch() >> alignment;
    }

    u32 data;
    if(cpsr().t) {
        const auto current_page = to_enum<memory_page>(pc() >> 24_u32);
        switch(current_page) {
            case memory_page::ewram:
            case memory_page::palette_ram: case memory_page::vram:
            case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
            case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
            case memory_page::pak_ws2_lower: case memory_page::pak_ws2_upper:
                data = pipeline_.decoding * 0x0001'0001_u32;
                break;
            case memory_page::bios:
            case memory_page::oam_ram:
                if((pc() & 0b11_u32) != 0_u32) {
                    data = pipeline_.executing | (pipeline_.decoding << 16_u32);
                } else {
                    // LSW = [$+4], MSW = [$+6]   ;for opcodes at 4-byte aligned locations
                    data = (widen<u32>(bus_->read_16(pc() + 2_u32, mem_access::dry_run)) << 16_u32) | pipeline_.decoding;
                }
                break;
            case memory_page::iwram:
                if((pc() & 0b11_u32) != 0_u32) {
                    data = pipeline_.executing | (pipeline_.decoding << 16_u32);
                } else {
                    data = pipeline_.decoding | (pipeline_.executing << 16_u32);
                }
                break;
            default:
                return 0_u32;
        }
    } else {
        data = pipeline_.decoding;
    }

    return data >> alignment;
}

void cpu::update_waitstate_table() noexcept
{
    static constexpr array<u8, 4> ws_nonseq{4_u8, 3_u8, 2_u8, 8_u8};
    static constexpr array<u8, 2> ws0_seq{2_u8, 1_u8};
    static constexpr array<u8, 2> ws1_seq{4_u8, 1_u8};
    static constexpr array<u8, 2> ws2_seq{8_u8, 1_u8};

    const u8 w_sram = 1_u8 + ws_nonseq[waitcnt_.sram];
    get_wait_cycles(wait_16, memory_page::pak_sram_1, mem_access::non_seq) = w_sram;
    get_wait_cycles(wait_16, memory_page::pak_sram_1, mem_access::seq) = w_sram;
    get_wait_cycles(wait_32, memory_page::pak_sram_1, mem_access::non_seq) = w_sram;
    get_wait_cycles(wait_32, memory_page::pak_sram_1, mem_access::seq) = w_sram;

    get_wait_cycles(wait_16, memory_page::pak_ws0_lower, mem_access::non_seq) = 1_u8 + ws_nonseq[waitcnt_.ws0_nonseq];
    get_wait_cycles(wait_16, memory_page::pak_ws0_upper, mem_access::non_seq) = 1_u8 + ws_nonseq[waitcnt_.ws0_nonseq];
    get_wait_cycles(wait_16, memory_page::pak_ws1_lower, mem_access::non_seq) = 1_u8 + ws_nonseq[waitcnt_.ws1_nonseq];
    get_wait_cycles(wait_16, memory_page::pak_ws1_upper, mem_access::non_seq) = 1_u8 + ws_nonseq[waitcnt_.ws1_nonseq];
    get_wait_cycles(wait_16, memory_page::pak_ws2_lower, mem_access::non_seq) = 1_u8 + ws_nonseq[waitcnt_.ws2_nonseq];
    get_wait_cycles(wait_16, memory_page::pak_ws2_upper, mem_access::non_seq) = 1_u8 + ws_nonseq[waitcnt_.ws2_nonseq];

    get_wait_cycles(wait_16, memory_page::pak_ws0_lower, mem_access::seq) = 1_u8 + ws0_seq[waitcnt_.ws0_seq];
    get_wait_cycles(wait_16, memory_page::pak_ws0_upper, mem_access::seq) = 1_u8 + ws0_seq[waitcnt_.ws0_seq];
    get_wait_cycles(wait_16, memory_page::pak_ws1_lower, mem_access::seq) = 1_u8 + ws1_seq[waitcnt_.ws1_seq];
    get_wait_cycles(wait_16, memory_page::pak_ws1_upper, mem_access::seq) = 1_u8 + ws1_seq[waitcnt_.ws1_seq];
    get_wait_cycles(wait_16, memory_page::pak_ws2_lower, mem_access::seq) = 1_u8 + ws2_seq[waitcnt_.ws2_seq];
    get_wait_cycles(wait_16, memory_page::pak_ws2_upper, mem_access::seq) = 1_u8 + ws2_seq[waitcnt_.ws2_seq];

    get_wait_cycles(wait_32, memory_page::pak_ws0_lower, mem_access::non_seq) = 2_u8 + ws_nonseq[waitcnt_.ws0_nonseq] + ws0_seq[waitcnt_.ws0_seq];
    get_wait_cycles(wait_32, memory_page::pak_ws0_upper, mem_access::non_seq) = 2_u8 + ws_nonseq[waitcnt_.ws0_nonseq] + ws0_seq[waitcnt_.ws0_seq];
    get_wait_cycles(wait_32, memory_page::pak_ws1_lower, mem_access::non_seq) = 2_u8 + ws_nonseq[waitcnt_.ws1_nonseq] + ws1_seq[waitcnt_.ws1_seq];
    get_wait_cycles(wait_32, memory_page::pak_ws1_upper, mem_access::non_seq) = 2_u8 + ws_nonseq[waitcnt_.ws1_nonseq] + ws1_seq[waitcnt_.ws1_seq];
    get_wait_cycles(wait_32, memory_page::pak_ws2_lower, mem_access::non_seq) = 2_u8 + ws_nonseq[waitcnt_.ws2_nonseq] + ws2_seq[waitcnt_.ws2_seq];
    get_wait_cycles(wait_32, memory_page::pak_ws2_upper, mem_access::non_seq) = 2_u8 + ws_nonseq[waitcnt_.ws2_nonseq] + ws2_seq[waitcnt_.ws2_seq];

    get_wait_cycles(wait_32, memory_page::pak_ws0_lower, mem_access::seq) = 2_u8 * (1_u8 + ws0_seq[waitcnt_.ws0_seq]);
    get_wait_cycles(wait_32, memory_page::pak_ws0_upper, mem_access::seq) = 2_u8 * (1_u8 + ws0_seq[waitcnt_.ws0_seq]);
    get_wait_cycles(wait_32, memory_page::pak_ws1_lower, mem_access::seq) = 2_u8 * (1_u8 + ws1_seq[waitcnt_.ws1_seq]);
    get_wait_cycles(wait_32, memory_page::pak_ws1_upper, mem_access::seq) = 2_u8 * (1_u8 + ws1_seq[waitcnt_.ws1_seq]);
    get_wait_cycles(wait_32, memory_page::pak_ws2_lower, mem_access::seq) = 2_u8 * (1_u8 + ws2_seq[waitcnt_.ws2_seq]);
    get_wait_cycles(wait_32, memory_page::pak_ws2_upper, mem_access::seq) = 2_u8 * (1_u8 + ws2_seq[waitcnt_.ws2_seq]);
}

} // namespace gba::cpu
