/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cpu/cpu.h>

#include <gba/archive.h>

namespace gba::cpu {

namespace {

} // namespace

void cpu::cpu::skip_bios() noexcept
{
    if(pc() != 0x0000'0000_u32) {
        return;
    }

    LOG_DEBUG(cpu, "skipping bios");

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
        return read_unused(addr) >> shift;
    }

    if(pc() < 0x0000'4000_u32) {
        bios_last_read_ = memcpy<u32>(bios_, addr);
    }
    return bios_last_read_ >> shift;
}

u32 cpu::read_unused(const u32 addr) noexcept
{
    const u32 alignment = (addr & 0b11_u32) << 3_u32;

    if(UNLIKELY(dma_controller_.is_running())) {
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
                    data = (widen<u32>(bus_->read_16(pc() + 2_u32, mem_access::none)) << 16_u32) | pipeline_.decoding;
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

void cpu::prefetch(const u32 addr, const u32 cycles) noexcept
{
    if(addr != pc()) {
        prefetch_buffer_.active = true;
        prefetch_buffer_.size = 0_u32;
        bus_->tick_components(cycles);
        return;
    }

    if(!prefetch_buffer_.empty() && addr == prefetch_buffer_.begin) {
        prefetch_buffer_.begin += prefetch_buffer_.addr_increment;
        --prefetch_buffer_.size;
        bus_->tick_components(1_u32);
        return;
    }

    if(prefetch_buffer_.active && addr == prefetch_buffer_.end) {
        bus_->tick_components(make_unsigned(prefetch_buffer_.cycles_left));
        prefetch_buffer_.begin = prefetch_buffer_.end;
        prefetch_buffer_.size = 0_u32;
        return;
    }

    bus_->tick_components(cycles);
    prefetch_buffer_.active = true;
    prefetch_buffer_.size = 0_u32;

    const auto page = to_enum<memory_page>(addr >> 24_u32);
    if(cpsr().t) {
        prefetch_buffer_.cycles_needed = stall_cycles<u16>(mem_access::seq, page);
        prefetch_buffer_.addr_increment = 2_u32;
    } else {
        prefetch_buffer_.cycles_needed = stall_cycles<u32>(mem_access::seq, page);
        prefetch_buffer_.addr_increment = 4_u32;
    }

    prefetch_buffer_.cycles_left = prefetch_buffer_.cycles_needed;
    prefetch_buffer_.capacity = prefetch_buffer::capacity_in_bytes / prefetch_buffer_.addr_increment;
    prefetch_buffer_.begin = addr + prefetch_buffer_.addr_increment;
    prefetch_buffer_.end = prefetch_buffer_.begin;
}

void cpu::prefetch_tick(const u32 cycles) noexcept
{
    if(!prefetch_buffer_.active || dma_controller_.is_running()) {
        return;
    }

    prefetch_buffer_.cycles_left -= make_signed(cycles);
    if (prefetch_buffer_.cycles_left <= 0_i32) {
        ++prefetch_buffer_.size;

        if(prefetch_buffer_.full()) {
            prefetch_buffer_.active = false;
        } else {
            prefetch_buffer_.end += prefetch_buffer_.addr_increment;
            prefetch_buffer_.cycles_left += prefetch_buffer_.cycles_needed;
        }
    }
}

void cpu::update_waitstate_table() noexcept
{
    static constexpr array<u8, 4> ws_nonseq{4_u8, 3_u8, 2_u8, 8_u8};
    static constexpr array<u8, 2> ws0_seq{2_u8, 1_u8};
    static constexpr array<u8, 2> ws1_seq{4_u8, 1_u8};
    static constexpr array<u8, 2> ws2_seq{8_u8, 1_u8};

    const u8 w_sram = 1_u8 + ws_nonseq[waitcnt_.sram];
    stall_cycles<u16>(mem_access::non_seq, memory_page::pak_sram_1) = w_sram;
    stall_cycles<u16>(mem_access::seq, memory_page::pak_sram_1) = w_sram;
    stall_cycles<u32>(mem_access::non_seq, memory_page::pak_sram_1) = w_sram;
    stall_cycles<u32>(mem_access::seq, memory_page::pak_sram_1) = w_sram;

    stall_cycles<u16>(mem_access::non_seq, memory_page::pak_ws0_lower) = 1_u8 + ws_nonseq[waitcnt_.ws0_nonseq];
    stall_cycles<u16>(mem_access::non_seq, memory_page::pak_ws0_upper) = 1_u8 + ws_nonseq[waitcnt_.ws0_nonseq];
    stall_cycles<u16>(mem_access::non_seq, memory_page::pak_ws1_lower) = 1_u8 + ws_nonseq[waitcnt_.ws1_nonseq];
    stall_cycles<u16>(mem_access::non_seq, memory_page::pak_ws1_upper) = 1_u8 + ws_nonseq[waitcnt_.ws1_nonseq];
    stall_cycles<u16>(mem_access::non_seq, memory_page::pak_ws2_lower) = 1_u8 + ws_nonseq[waitcnt_.ws2_nonseq];
    stall_cycles<u16>(mem_access::non_seq, memory_page::pak_ws2_upper) = 1_u8 + ws_nonseq[waitcnt_.ws2_nonseq];

    stall_cycles<u16>(mem_access::seq, memory_page::pak_ws0_lower) = 1_u8 + ws0_seq[waitcnt_.ws0_seq];
    stall_cycles<u16>(mem_access::seq, memory_page::pak_ws0_upper) = 1_u8 + ws0_seq[waitcnt_.ws0_seq];
    stall_cycles<u16>(mem_access::seq, memory_page::pak_ws1_lower) = 1_u8 + ws1_seq[waitcnt_.ws1_seq];
    stall_cycles<u16>(mem_access::seq, memory_page::pak_ws1_upper) = 1_u8 + ws1_seq[waitcnt_.ws1_seq];
    stall_cycles<u16>(mem_access::seq, memory_page::pak_ws2_lower) = 1_u8 + ws2_seq[waitcnt_.ws2_seq];
    stall_cycles<u16>(mem_access::seq, memory_page::pak_ws2_upper) = 1_u8 + ws2_seq[waitcnt_.ws2_seq];

    stall_cycles<u32>(mem_access::non_seq, memory_page::pak_ws0_lower) = 2_u8 + ws_nonseq[waitcnt_.ws0_nonseq] + ws0_seq[waitcnt_.ws0_seq];
    stall_cycles<u32>(mem_access::non_seq, memory_page::pak_ws0_upper) = 2_u8 + ws_nonseq[waitcnt_.ws0_nonseq] + ws0_seq[waitcnt_.ws0_seq];
    stall_cycles<u32>(mem_access::non_seq, memory_page::pak_ws1_lower) = 2_u8 + ws_nonseq[waitcnt_.ws1_nonseq] + ws1_seq[waitcnt_.ws1_seq];
    stall_cycles<u32>(mem_access::non_seq, memory_page::pak_ws1_upper) = 2_u8 + ws_nonseq[waitcnt_.ws1_nonseq] + ws1_seq[waitcnt_.ws1_seq];
    stall_cycles<u32>(mem_access::non_seq, memory_page::pak_ws2_lower) = 2_u8 + ws_nonseq[waitcnt_.ws2_nonseq] + ws2_seq[waitcnt_.ws2_seq];
    stall_cycles<u32>(mem_access::non_seq, memory_page::pak_ws2_upper) = 2_u8 + ws_nonseq[waitcnt_.ws2_nonseq] + ws2_seq[waitcnt_.ws2_seq];

    stall_cycles<u32>(mem_access::seq, memory_page::pak_ws0_lower) = 2_u8 * (1_u8 + ws0_seq[waitcnt_.ws0_seq]);
    stall_cycles<u32>(mem_access::seq, memory_page::pak_ws0_upper) = 2_u8 * (1_u8 + ws0_seq[waitcnt_.ws0_seq]);
    stall_cycles<u32>(mem_access::seq, memory_page::pak_ws1_lower) = 2_u8 * (1_u8 + ws1_seq[waitcnt_.ws1_seq]);
    stall_cycles<u32>(mem_access::seq, memory_page::pak_ws1_upper) = 2_u8 * (1_u8 + ws1_seq[waitcnt_.ws1_seq]);
    stall_cycles<u32>(mem_access::seq, memory_page::pak_ws2_lower) = 2_u8 * (1_u8 + ws2_seq[waitcnt_.ws2_seq]);
    stall_cycles<u32>(mem_access::seq, memory_page::pak_ws2_upper) = 2_u8 * (1_u8 + ws2_seq[waitcnt_.ws2_seq]);
}

void cpu::serialize(archive& archive) const noexcept
{
    archive.serialize(r_);
    for(auto& bank : reg_banks_.reg_banks) {
        archive.serialize(bank.r);
    }
    archive.serialize(static_cast<u32>(cpsr_));
    for(auto& bank : spsr_banks_.banks) {
        archive.serialize(static_cast<u32>(bank));
    }
    archive.serialize(ie_);
    archive.serialize(if_);
    archive.serialize(ime_);
    archive.serialize(irq_signal_);
    archive.serialize(scheduled_irq_signal_);
    archive.serialize(irq_signal_delay_handle_);

    archive.serialize(pipeline_.decoding);
    archive.serialize(pipeline_.executing);
    archive.serialize(pipeline_.fetch_type);

    archive.serialize(wram_);
    archive.serialize(iwram_);
    archive.serialize(timer_controller_);
    archive.serialize(dma_controller_);
    archive.serialize(bios_last_read_);
    archive.serialize(post_boot_);

    archive.serialize(prefetch_buffer_.begin);
    archive.serialize(prefetch_buffer_.end);
    archive.serialize(prefetch_buffer_.size);
    archive.serialize(prefetch_buffer_.capacity);
    archive.serialize(prefetch_buffer_.cycles_left);
    archive.serialize(prefetch_buffer_.cycles_needed);
    archive.serialize(prefetch_buffer_.addr_increment);
    archive.serialize(prefetch_buffer_.active);

    archive.serialize(waitcnt_.sram);
    archive.serialize(waitcnt_.ws0_nonseq);
    archive.serialize(waitcnt_.ws0_seq);
    archive.serialize(waitcnt_.ws1_nonseq);
    archive.serialize(waitcnt_.ws1_seq);
    archive.serialize(waitcnt_.ws2_nonseq);
    archive.serialize(waitcnt_.ws2_seq);
    archive.serialize(waitcnt_.phi);
    archive.serialize(waitcnt_.prefetch_buffer_enable);
    archive.serialize(haltcnt_);
}

void cpu::deserialize(const archive& archive) noexcept
{
    archive.deserialize(r_);
    for(auto& bank : reg_banks_.reg_banks) {
        archive.deserialize(bank.r);
    }

    cpsr_ = archive.deserialize<u32>();
    for(auto& bank : spsr_banks_.banks) {
        bank = archive.deserialize<u32>();
    }
    archive.deserialize(ie_);
    archive.deserialize(if_);
    archive.deserialize(ime_);
    archive.deserialize(irq_signal_);
    archive.deserialize(scheduled_irq_signal_);
    archive.deserialize(irq_signal_delay_handle_);

    archive.deserialize(pipeline_.decoding);
    archive.deserialize(pipeline_.executing);
    archive.deserialize(pipeline_.fetch_type);

    archive.deserialize(wram_);
    archive.deserialize(iwram_);
    archive.deserialize(timer_controller_);
    archive.deserialize(dma_controller_);
    archive.deserialize(bios_last_read_);
    archive.deserialize(post_boot_);

    archive.deserialize(prefetch_buffer_.begin);
    archive.deserialize(prefetch_buffer_.end);
    archive.deserialize(prefetch_buffer_.size);
    archive.deserialize(prefetch_buffer_.capacity);
    archive.deserialize(prefetch_buffer_.cycles_left);
    archive.deserialize(prefetch_buffer_.cycles_needed);
    archive.deserialize(prefetch_buffer_.addr_increment);
    archive.deserialize(prefetch_buffer_.active);

    archive.deserialize(waitcnt_.sram);
    archive.deserialize(waitcnt_.ws0_nonseq);
    archive.deserialize(waitcnt_.ws0_seq);
    archive.deserialize(waitcnt_.ws1_nonseq);
    archive.deserialize(waitcnt_.ws1_seq);
    archive.deserialize(waitcnt_.ws2_nonseq);
    archive.deserialize(waitcnt_.ws2_seq);
    archive.deserialize(waitcnt_.phi);
    archive.deserialize(waitcnt_.prefetch_buffer_enable);
    archive.deserialize(haltcnt_);

    update_waitstate_table();
}

} // namespace gba::cpu
