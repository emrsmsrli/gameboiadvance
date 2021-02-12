/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

#include <gba/gba.h>
#include <gba/arm/mmio_addr.h>

namespace gba::arm {

namespace {

enum class memory_page {
    bios = 0x00,
    ewram = 0x02,
    iwram = 0x03,
    io = 0x04,
    palette_ram = 0x05,
    vram = 0x06,
    oam_ram = 0x07,
    pak_ws0_lower = 0x08,
    pak_ws0_upper = 0x09,
    pak_ws1_lower = 0x0A,
    pak_ws1_upper = 0x0B,
    pak_ws2_lower = 0x0C,
    pak_ws2_upper = 0x0D,
    pak_sram_1 = 0x0E,
    pak_sram_2 = 0x0F,
};

constexpr array<u8, 4> ws_nonseq{4_u8, 3_u8, 2_u8, 8_u8};
constexpr array<u8, 2> ws0_seq{2_u8, 1_u8};
constexpr array<u8, 2> ws1_seq{4_u8, 1_u8};
constexpr array<u8, 2> ws2_seq{8_u8, 1_u8};

// Even though VRAM is sized 96K (64K+32K),
// it is repeated in steps of 128K (64K+32K+32K, the two 32K blocks itself being mirrors of each other).
FORCEINLINE constexpr u32 adjust_vram_addr(u32 addr) noexcept
{
    addr &= 0x0001'FFFF_u32;
    if(addr >= 0x0001'8000_u32) {
        return bit::clear(addr, 15_u8); // mirror 32K
    }
    return addr;
}

FORCEINLINE constexpr bool is_gpio(const u32 addr) noexcept
{
    return 0xC4_u32 <= addr && addr < 0xCA_u32;
}

FORCEINLINE constexpr bool is_eeprom(const cartridge::backup::type type, const u32 addr) noexcept
{
    // todo return (~memory.rom.size & 0x0200 0000) || address >= 0x0DFF FF00;
    // todo same as  (rom.size == 32_mb) || address >= 0x0DFF FF00;
    return (type == cartridge::backup::type::eeprom_64 || type == cartridge::backup::type::eeprom_4)
      && addr >= 0x0DFF'FF00_u32;
}

FORCEINLINE u8& get_wait_cycles(array<u8, 32>& table, const memory_page page, const mem_access access) noexcept
{
    if(UNLIKELY(page > memory_page::pak_sram_2)) {
        static u8 unused_area = 1_u8;
        return unused_area;
    }

    return table[static_cast<u32::type>(page) + static_cast<u32::type>(access) * 16_u32];
}

} // namespace

u32 arm7tdmi::read_32_aligned(const u32 addr, const mem_access access) noexcept
{
    const u32 data = read_32(addr, access);
    const u32 rotate_amount = (addr & 0b11_u32) * 8_u32;
    return (data >> rotate_amount) | (data << (32_u32 - rotate_amount));
}

u32 arm7tdmi::read_32(u32 addr, const mem_access access) noexcept
{
    const auto page = static_cast<memory_page>(addr.get() >> 24_u32);
    tick_components(get_wait_cycles(wait_32, page, access));

    if(page != memory_page::pak_sram_1 && page != memory_page::pak_sram_2) {
        addr = mask::clear(addr, 0b11_u32);
    }

    switch(page) {
        case memory_page::bios:
            return read_bios(addr);
        case memory_page::ewram:
            return memcpy<u32>(wram_, addr & 0x0003'FFFF_u32);
        case memory_page::iwram:
            return memcpy<u32>(iwram_, addr & 0x0000'7FFF_u32);
        case memory_page::io: {
            return widen<u32>(read_io(addr))
              | (widen<u32>(read_io(addr + 1_u32)) << 8_u32)
              | (widen<u32>(read_io(addr + 2_u32)) << 8_u32)
              | (widen<u32>(read_io(addr + 3_u32)) << 8_u32);
        }
        case memory_page::palette_ram:
            return memcpy<u32>(gba_->ppu.palette_ram_, addr & 0x0000'03FF_u32);
        case memory_page::vram:
            return memcpy<u32>(gba_->ppu.vram_, adjust_vram_addr(addr));
        case memory_page::oam_ram:
            return memcpy<u32>(gba_->ppu.oam_, addr & 0x0000'03FF_u32);
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower: case memory_page::pak_ws2_upper:
            // todo The GBA forcefully uses non-sequential timing at the beginning of each 128K-block of gamepak ROM
            addr &= gba_->pak.mirror_mask_;
            // todo Because Gamepak uses the same signal-lines for both 16bit data and for lower 16bit halfword address,
            // the entire gamepak ROM area is effectively filled by incrementing 16bit values (Address/2 AND FFFFh).
            if(is_gpio(addr) && gba_->pak.rtc_.read_allowed()) {
                return widen<u32>(gba_->pak.rtc_.read(addr + 2_u32)) << 16_u32
                  | gba_->pak.rtc_.read(addr);
            }
            return memcpy<u32>(gba_->pak.pak_data_, addr);
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(gba_->pak.backup_type() == cartridge::backup::type::sram) {
                return gba_->pak.backup_->read(addr) * 0x0101'0101_u32;
            }
            return 0xFFFF'FFFF_u32;
        default:
            return read_unused(addr);
    }
}

void arm7tdmi::write_32(u32 addr, const u32 data, const mem_access access) noexcept
{
    const auto page = static_cast<memory_page>(addr.get() >> 24_u32);
    tick_components(get_wait_cycles(wait_32, page, access));

    if(page != memory_page::pak_sram_1 && page != memory_page::pak_sram_2) {
        addr = mask::clear(addr, 0b11_u32);
    }

    switch(page) {
        case memory_page::ewram:
            memcpy<u32>(wram_, addr & 0x0003'FFFF_u32, data);
            break;
        case memory_page::iwram:
            memcpy<u32>(iwram_, addr & 0x0000'7FFF_u32, data);
            break;
        case memory_page::io:
            for(u32 i = 0_u32; i < 4_u32; ++i) {
                write_io(addr + i, narrow<u8>(data >> (i * 8_u32)));
            }
            break;
        case memory_page::palette_ram:
            memcpy<u32>(gba_->ppu.palette_ram_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::vram:
            memcpy<u32>(gba_->ppu.vram_, adjust_vram_addr(addr), data);
            break;
        case memory_page::oam_ram:
            memcpy<u32>(gba_->ppu.oam_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower: case memory_page::pak_ws2_upper:
            addr &= gba_->pak.mirror_mask_;
            if(gba_->pak.has_rtc_ && is_gpio(addr)) {
                gba_->pak.rtc_.write(addr, narrow<u8>(data));
                gba_->pak.rtc_.write(addr + 2_u32, narrow<u8>(data >> 16_u32));
            }
            break;
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(gba_->pak.backup_type() == cartridge::backup::type::sram) {
                return gba_->pak.backup_->write(addr, narrow<u8>(data >> (8_u32 * (addr & 0b11_u32))));
            }
            break;
        default:
            LOG_WARN(arm::io, "invalid write32 to address {:08X}, {:08X}", addr, data);
            break;
    }
}

u32 arm7tdmi::read_16_signed(const u32 addr, const mem_access access) noexcept
{
    if(bit::test(addr, 0_u8)) {
        return make_unsigned(math::sign_extend<8>(widen<u32>(read_8(addr, access))));
    }
    return make_unsigned(math::sign_extend<16>(widen<u32>(read_16(addr, access))));
}

u32 arm7tdmi::read_16_aligned(const u32 addr, const mem_access access) noexcept
{
    const u32 data = read_16(addr, access);
    const u32 rotate_amount = bit::extract(addr, 0_u8);
    return (data >> (8_u32 * rotate_amount)) | (data << (24_u8 * rotate_amount));
}

u16 arm7tdmi::read_16(u32 addr, const mem_access access) noexcept
{
    const auto page = static_cast<memory_page>(addr.get() >> 24_u32);
    tick_components(get_wait_cycles(wait_16, page, access));

    if(page != memory_page::pak_sram_1 && page != memory_page::pak_sram_2) {
        addr = bit::clear(addr, 0_u8);
    }

    switch(page) {
        case memory_page::bios:
            return narrow<u16>(read_bios(addr));
        case memory_page::ewram:
            return memcpy<u16>(wram_, addr & 0x0003'FFFF_u32);
        case memory_page::iwram:
            return memcpy<u16>(iwram_, addr & 0x0000'7FFF_u32);
        case memory_page::io:
            return widen<u16>(read_io(addr))
              | (widen<u16>(read_io(addr + 1_u32)) << 8_u16);
        case memory_page::palette_ram:
            return memcpy<u16>(gba_->ppu.palette_ram_, addr & 0x0000'03FF_u32);
        case memory_page::vram:
            return memcpy<u16>(gba_->ppu.vram_, adjust_vram_addr(addr));
        case memory_page::oam_ram:
            return memcpy<u16>(gba_->ppu.oam_, addr & 0x0000'03FF_u32);
        case memory_page::pak_ws2_upper:
            if(is_eeprom(gba_->pak.backup_type(), addr)) {
                return gba_->pak.backup_->read(addr);
            }
            [[fallthrough]];
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower:
            // todo The GBA forcefully uses non-sequential timing at the beginning of each 128K-block of gamepak ROM
            addr &= gba_->pak.mirror_mask_;
            if(is_gpio(addr) && gba_->pak.rtc_.read_allowed()) {
                return gba_->pak.rtc_.read(addr);
            }
            // todo Because Gamepak uses the same signal-lines for both 16bit data and for lower 16bit halfword address,
            // the entire gamepak ROM area is effectively filled by incrementing 16bit values (Address/2 AND FFFFh).
            return memcpy<u16>(gba_->pak.pak_data_, addr);
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(gba_->pak.backup_type() == cartridge::backup::type::sram) {
                return gba_->pak.backup_->read(addr) * 0x0101_u16;
            }
            return 0xFFFF_u16;
        default:
            return narrow<u16>(read_unused(addr));
    }
}

void arm7tdmi::write_16(u32 addr, const u16 data, const mem_access access) noexcept
{
    const auto page = static_cast<memory_page>(addr.get() >> 24_u32);
    tick_components(get_wait_cycles(wait_16, page, access));

    if(page != memory_page::pak_sram_1 && page != memory_page::pak_sram_2) {
        addr = bit::clear(addr, 0_u8);
    }

    switch(page) {
        case memory_page::ewram:
            memcpy<u16>(wram_, addr & 0x0003'FFFF_u32, data);
            break;
        case memory_page::iwram:
            memcpy<u16>(iwram_, addr & 0x0000'7FFF_u32, data);
            break;
        case memory_page::io:
            write_io(addr, narrow<u8>(data));
            write_io(addr + 1, narrow<u8>(data >> 8_u16));
            break;
        case memory_page::palette_ram:
            memcpy<u16>(gba_->ppu.palette_ram_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::vram:
            memcpy<u16>(gba_->ppu.vram_, adjust_vram_addr(addr), data);
            break;
        case memory_page::oam_ram:
            memcpy<u16>(gba_->ppu.oam_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower:
            addr &= gba_->pak.mirror_mask_;
            if(gba_->pak.has_rtc_ && is_gpio(addr)) {
                gba_->pak.rtc_.write(addr, narrow<u8>(data));
                gba_->pak.rtc_.write(addr + 1_u32, narrow<u8>(data >> 8_u16));
            }
            break;
        case memory_page::pak_ws2_upper:
            if(is_eeprom(gba_->pak.backup_type(), addr)) {
                // fixme eeprom is only written by dma
                gba_->pak.backup_->write(addr, narrow<u8>(data));
            } else { // fixme???????????????????
                addr &= gba_->pak.mirror_mask_;
                if(gba_->pak.has_rtc_ && is_gpio(addr)) {
                    gba_->pak.rtc_.write(addr, narrow<u8>(data));
                }
            }
            break;
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(gba_->pak.backup_type() == cartridge::backup::type::sram) {
                return gba_->pak.backup_->write(addr, narrow<u8>(data >> (8_u16 * (narrow<u16>(addr) & 0b1_u16))));
            }
            break;
        default:
            LOG_WARN(arm::io, "invalid write16 to address {:08X}, {:04X}", addr, data);
            break;
    }
}

u32 arm7tdmi::read_8_signed(const u32 addr, const mem_access access) noexcept
{
    return make_unsigned(math::sign_extend<8>(widen<u32>(read_8(addr, access))));
}

u8 arm7tdmi::read_8(u32 addr, const mem_access access) noexcept
{
    const auto page = static_cast<memory_page>(addr.get() >> 24_u32);
    tick_components(get_wait_cycles(wait_16, page, access));

    switch(page) {
        case memory_page::bios:
            return narrow<u8>(read_bios(addr));
        case memory_page::ewram:
            return memcpy<u8>(wram_, addr & 0x0003'FFFF_u32);
        case memory_page::iwram:
            return memcpy<u8>(iwram_, addr & 0x0000'7FFF_u32);
        case memory_page::io:
            return read_io(addr);
        case memory_page::palette_ram:
            return memcpy<u8>(gba_->ppu.palette_ram_, addr & 0x0000'03FF_u32);
        case memory_page::vram:
            return memcpy<u8>(gba_->ppu.vram_, adjust_vram_addr(addr));
        case memory_page::oam_ram:
            return memcpy<u8>(gba_->ppu.oam_, addr & 0x0000'03FF_u32);
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower: case memory_page::pak_ws2_upper:
            // todo The GBA forcefully uses non-sequential timing at the beginning of each 128K-block of gamepak ROM
            addr &= gba_->pak.mirror_mask_;
            // todo Because Gamepak uses the same signal-lines for both 16bit data and for lower 16bit halfword address, the entire gamepak ROM area is effectively filled by incrementing 16bit values (Address/2 AND FFFFh).
            if(is_gpio(addr) && gba_->pak.rtc_.read_allowed()) {
                return gba_->pak.rtc_.read(addr);
            }
            return memcpy<u8>(gba_->pak.pak_data_, addr);
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(gba_->pak.backup_type() == cartridge::backup::type::sram) {
                return gba_->pak.backup_->read(addr);
            }
            return 0xFF_u8;
        default:
            return narrow<u8>(read_unused(addr));
    }
}

void arm7tdmi::write_8(u32 addr, const u8 data, const mem_access access) noexcept
{
    const auto page = static_cast<memory_page>(addr.get() >> 24_u32);
    tick_components(get_wait_cycles(wait_16, page, access));

    switch(page) {
        case memory_page::ewram:
            memcpy<u8>(wram_, addr & 0x0003'FFFF_u32, data);
            break;
        case memory_page::iwram:
            memcpy<u8>(iwram_, addr & 0x0000'7FFF_u32, data);
            break;
        case memory_page::io:
            write_io(addr, narrow<u8>(data));
            break;
        case memory_page::palette_ram:
            // todo check gbatek Writing 8bit Data to Video Memory
            memcpy<u8>(gba_->ppu.palette_ram_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::vram:
            // todo check gbatek Writing 8bit Data to Video Memory
            memcpy<u8>(gba_->ppu.vram_, adjust_vram_addr(addr), data);
            break;
        case memory_page::oam_ram:
            memcpy<u8>(gba_->ppu.oam_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(gba_->pak.backup_type() == cartridge::backup::type::sram) {
                return gba_->pak.backup_->write(addr, data);
            }
            break;
        default:
            LOG_WARN(arm::io, "invalid write8 to address {:08X}, {:02X}", addr, data);
            break;
    }
}

u32 arm7tdmi::read_bios(u32 addr) noexcept
{
    const u32 shift = (addr & 0b11_u32) << 3_u32;
    addr = mask::clear(addr, 0b11_u32);

    if(UNLIKELY(addr >= 0x0000'4000_u32)) {
        return read_unused(addr);
    }

    if(r15_ < 0x0000'4000_u32) {
        bios_last_read_ = memcpy<u32>(bios_, addr);
    }
    return bios_last_read_ >> shift;
}

u32 arm7tdmi::read_unused(const u32 addr) noexcept
{
    u32 data;
    if(cpsr().t) {
        const auto current_page = static_cast<memory_page>(r15_.get() >> 24_u32);
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
                if((addr & 0b11_u32) != 0_u32) {
                    data = pipeline_.executing | (pipeline_.decoding << 16_u32);
                } else {
                    // should've been LSW = [$+4], MSW = [$+6]   ;for opcodes at 4-byte aligned locations
                    LOG_WARN(arm::io, "bios|oam unused read at {:08X}", addr);
                    data = pipeline_.decoding * 0x0001'0001_u32;
                }
                break;
            case memory_page::iwram:
                if((addr & 0b11_u32) != 0_u32) {
                    data = pipeline_.executing | (pipeline_.decoding << 16_u32);
                } else {
                    data = pipeline_.decoding | (pipeline_.executing << 16_u32);
                }
                break;
            default:
                break; // returns 0
        }
    } else {
        data = pipeline_.decoding;
    }

    return data >> ((addr & 0b11_u32) << 3_u32);
}

u8 arm7tdmi::read_io(const u32 addr) noexcept
{
    switch(addr.get()) {
        case keypad::addr_state: return narrow<u8>(gba_->keypad.keyinput_);
        case keypad::addr_state + 1: return narrow<u8>(gba_->keypad.keyinput_ >> 8_u16);
        case keypad::addr_control: return narrow<u8>(gba_->keypad.keycnt_.select);
        case keypad::addr_control + 1:
            return narrow<u8>(widen<u32>(gba_->keypad.keycnt_.select) >> 8_u32 & 0b11_u32
              | bit::from_bool(gba_->keypad.keycnt_.enabled) << 6_u32
              | static_cast<u32::type>(gba_->keypad.keycnt_.cond_strategy) << 7_u32);

        case addr_tm0cnt_l:     return timers_[0_usize].read(timer::register_type::cnt_l_lsb);
        case addr_tm0cnt_l + 1: return timers_[0_usize].read(timer::register_type::cnt_l_msb);
        case addr_tm0cnt_h:     return timers_[0_usize].read(timer::register_type::cnt_h_lsb);
        case addr_tm0cnt_h + 1: return 0_u8;
        case addr_tm1cnt_l:     return timers_[1_usize].read(timer::register_type::cnt_l_lsb);
        case addr_tm1cnt_l + 1: return timers_[1_usize].read(timer::register_type::cnt_l_msb);
        case addr_tm1cnt_h:     return timers_[1_usize].read(timer::register_type::cnt_h_lsb);
        case addr_tm1cnt_h + 1: return 0_u8;
        case addr_tm2cnt_l:     return timers_[2_usize].read(timer::register_type::cnt_l_lsb);
        case addr_tm2cnt_l + 1: return timers_[2_usize].read(timer::register_type::cnt_l_msb);
        case addr_tm2cnt_h:     return timers_[2_usize].read(timer::register_type::cnt_h_lsb);
        case addr_tm2cnt_h + 1: return 0_u8;
        case addr_tm3cnt_l:     return timers_[3_usize].read(timer::register_type::cnt_l_lsb);
        case addr_tm3cnt_l + 1: return timers_[3_usize].read(timer::register_type::cnt_l_msb);
        case addr_tm3cnt_h:     return timers_[3_usize].read(timer::register_type::cnt_h_lsb);
        case addr_tm3cnt_h + 1: return 0_u8;

        case addr_ime: return bit::from_bool<u8>(ime_);
        case addr_ie: return narrow<u8>(ie_);
        case addr_ie + 1: return narrow<u8>(ie_ >> 8_u8);
        case addr_if: return narrow<u8>(if_);
        case addr_if + 1: return narrow<u8>(if_ >> 8_u8);
        case addr_waitcnt:
            return waitcnt_.sram
              | (waitcnt_.ws0_nonseq << 2_u8)
              | (waitcnt_.ws0_seq << 4_u8)
              | (waitcnt_.ws1_nonseq << 5_u8)
              | (waitcnt_.ws2_seq << 7_u8);
        case addr_waitcnt + 1:
            return waitcnt_.ws2_nonseq
              | (waitcnt_.ws2_seq << 2_u8)
              | (waitcnt_.phi << 3_u8)
              | bit::from_bool<u8>(waitcnt_.prefetch_buffer_enable) << 6_u8;
        case addr_postboot:
            return post_boot_;
    }

    return narrow<u8>(read_unused(addr));
}

void arm7tdmi::write_io(const u32 addr, const u8 data) noexcept
{
    switch(addr.get()) {
        case keypad::addr_control:
            gba_->keypad.keycnt_.select = (gba_->keypad.keycnt_.select & 0xFF00_u16) | data;
            if(gba_->keypad.interrupt_available()) {
                request_interrupt(arm::interrupt_source::keypad);
            }
            break;
        case keypad::addr_control + 1:
            gba_->keypad.keycnt_.select = (gba_->keypad.keycnt_.select & 0x00FF_u16)
              | (widen<u16>(data & 0b11_u8) << 8_u16);
            gba_->keypad.keycnt_.enabled = bit::test(data, 6_u8);
            gba_->keypad.keycnt_.cond_strategy =
              static_cast<keypad::keypad::irq_control::condition_strategy>(bit::extract(data, 7_u8).get());
            if(gba_->keypad.interrupt_available()) {
                request_interrupt(arm::interrupt_source::keypad);
            }
            break;

        // cnt_h_msb is unused
        case addr_tm0cnt_l:     timers_[0_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case addr_tm0cnt_l + 1: timers_[0_usize].write(timer::register_type::cnt_l_msb, data); break;
        case addr_tm0cnt_h:     timers_[0_usize].write(timer::register_type::cnt_h_lsb, data); break;
        case addr_tm1cnt_l:     timers_[1_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case addr_tm1cnt_l + 1: timers_[1_usize].write(timer::register_type::cnt_l_msb, data); break;
        case addr_tm1cnt_h:     timers_[1_usize].write(timer::register_type::cnt_h_lsb, data); break;
        case addr_tm2cnt_l:     timers_[2_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case addr_tm2cnt_l + 1: timers_[2_usize].write(timer::register_type::cnt_l_msb, data); break;
        case addr_tm2cnt_h:     timers_[2_usize].write(timer::register_type::cnt_h_lsb, data); break;
        case addr_tm3cnt_l:     timers_[3_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case addr_tm3cnt_l + 1: timers_[3_usize].write(timer::register_type::cnt_l_msb, data); break;
        case addr_tm3cnt_h:     timers_[3_usize].write(timer::register_type::cnt_h_lsb, data); break;

        case addr_ime:
            ime_ = bit::test(data, 0_u8);
            break;
        case addr_ie:
            ie_ = (ie_ & 0xFF00_u16) | data;
            break;
        case addr_ie + 1:
            ie_ = (ie_ & 0x00FF_u16) | (widen<u16>(data & 0x3F_u8) << 8_u16);
            break;
        case addr_if:
            if_ &= ~data;
            break;
        case addr_if + 1:
            if_ &= ~(widen<u16>(data) << 8_u16);
            break;
        case addr_waitcnt:
            waitcnt_.sram = data & 0b11_u8;
            waitcnt_.ws0_nonseq = (data >> 2_u8) & 0b11_u8;
            waitcnt_.ws0_seq = bit::extract(data, 4_u8);
            waitcnt_.ws1_nonseq = (data >> 5_u8) & 0b11_u8;
            waitcnt_.ws2_seq = bit::extract(data, 7_u8);
            update_waitstate_table();
            break;
        case addr_waitcnt + 1:
            waitcnt_.ws2_nonseq = data & 0b11_u8;
            waitcnt_.ws2_seq = bit::extract(data, 2_u8);
            waitcnt_.phi = (data >> 3_u8) & 0b11_u8;
            waitcnt_.prefetch_buffer_enable = bit::test(data, 6_u8);
            update_waitstate_table();
            break;
        case addr_haltcnt:
            haltcnt_ = static_cast<halt_control>(bit::extract(data, 0_u8).get());
            break;
        case addr_postboot:
            post_boot_ = bit::extract(data, 0_u8);
    }
}

void arm7tdmi::update_waitstate_table() noexcept
{
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

} // namespace gba::arm
