/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

#include <gba/core.h>
#include <gba/arm/mmio_addr.h>

namespace gba::arm {

namespace {

enum class memory_page : u32::type {
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

FORCEINLINE constexpr bool is_eeprom(const usize pak_size, const cartridge::backup::type type, const u32 addr) noexcept
{
    return (type == cartridge::backup::type::eeprom_64 || type == cartridge::backup::type::eeprom_4)
      && (pak_size < (32_u32 * 1024_kb) || addr >= 0x0DFF'FF00_u32);
}

FORCEINLINE constexpr bool is_sram_flash(const cartridge::backup::type type) noexcept
{
    return type == cartridge::backup::type::sram
        || type == cartridge::backup::type::flash_64
        || type == cartridge::backup::type::flash_128;
}

FORCEINLINE u8& get_wait_cycles(array<u8, 32>& table, const memory_page page, const mem_access access) noexcept
{
    if(UNLIKELY(page > memory_page::pak_sram_2)) {
        static u8 unused_area = 1_u8;
        return unused_area;
    }

    // make sure we only have nonseq & seq bits
    const u32 access_offset = ((from_enum<u32>(access) & 0b11_u32) - 1_u32) * 16_u32;
    return table[from_enum<u32>(page) + access_offset];
}

FORCEINLINE mem_access get_actual_access(const memory_page page, const u32 addr, mem_access default_access) noexcept
{
    if(page >= memory_page::pak_ws0_lower
      && page <= memory_page::pak_ws2_upper
      && (addr & 0x1'FFFF_u32) != 0_u32) { // force nonseq access on 128kb boundaries in rom
        return (default_access & ~mem_access::seq) | mem_access::non_seq;
    }

    return default_access;
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
    const auto page = to_enum<memory_page>(addr >> 24_u32);

    if(LIKELY(!bitflags::is_set(access, mem_access::dry_run))) {
        tick_components(get_wait_cycles(wait_32, page, get_actual_access(page, addr, access)));
    }

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
              | (widen<u32>(read_io(addr + 2_u32)) << 16_u32)
              | (widen<u32>(read_io(addr + 3_u32)) << 24_u32);
        }
        case memory_page::palette_ram:
            return memcpy<u32>(core_->ppu.palette_ram_, addr & 0x0000'03FF_u32);
        case memory_page::vram:
            return memcpy<u32>(core_->ppu.vram_, adjust_vram_addr(addr));
        case memory_page::oam_ram:
            return memcpy<u32>(core_->ppu.oam_, addr & 0x0000'03FF_u32);
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower: case memory_page::pak_ws2_upper:
            addr &= core_->pak.mirror_mask_;
            if (addr >= core_->pak.pak_data_.size()) {
                return ((addr / 2_u32) & 0xFFFF_u32) | (((addr + 2_u32) / 2_u32) << 16_u32);
            }
            if(is_gpio(addr) && core_->pak.rtc_.read_allowed()) {
                return widen<u32>(core_->pak.rtc_.read(addr + 2_u32)) << 16_u32
                  | core_->pak.rtc_.read(addr);
            }
            return memcpy<u32>(core_->pak.pak_data_, addr);
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(core_->pak.backup_type() == cartridge::backup::type::sram) {
                return core_->pak.backup_->read(addr) * 0x0101'0101_u32;
            }
            return 0xFFFF'FFFF_u32;
        default:
            return read_unused(addr);
    }
}

void arm7tdmi::write_32(u32 addr, const u32 data, const mem_access access) noexcept
{
    const auto page = to_enum<memory_page>(addr >> 24_u32);

    ASSERT(!bitflags::is_set(access, mem_access::dry_run));
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
            memcpy<u32>(core_->ppu.palette_ram_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::vram:
            memcpy<u32>(core_->ppu.vram_, adjust_vram_addr(addr), data);
            break;
        case memory_page::oam_ram:
            memcpy<u32>(core_->ppu.oam_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower: case memory_page::pak_ws2_upper:
            addr &= cartridge::gamepak::default_mirror_mask;
            if(core_->pak.has_rtc_ && is_gpio(addr)) {
                core_->pak.rtc_.write(addr, narrow<u8>(data));
                core_->pak.rtc_.write(addr + 2_u32, narrow<u8>(data >> 16_u32));
            }
            break;
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(is_sram_flash(core_->pak.backup_type())) {
                return core_->pak.backup_->write(addr, narrow<u8>(data >> (8_u32 * (addr & 0b11_u32))));
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
    const auto page = to_enum<memory_page>(addr >> 24_u32);

    if(LIKELY(!bitflags::is_set(access, mem_access::dry_run))) {
        tick_components(get_wait_cycles(wait_16, page, get_actual_access(page, addr, access)));
    }

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
            return memcpy<u16>(core_->ppu.palette_ram_, addr & 0x0000'03FF_u32);
        case memory_page::vram:
            return memcpy<u16>(core_->ppu.vram_, adjust_vram_addr(addr));
        case memory_page::oam_ram:
            return memcpy<u16>(core_->ppu.oam_, addr & 0x0000'03FF_u32);
        case memory_page::pak_ws2_upper:
            if(is_eeprom(core_->pak.pak_data_.size(), core_->pak.backup_type(), addr)) {
                return core_->pak.backup_->read(addr);
            }
            [[fallthrough]];
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower:
            addr &= core_->pak.mirror_mask_;
            if(is_gpio(addr) && core_->pak.rtc_.read_allowed()) {
                return core_->pak.rtc_.read(addr);
            }
            if (addr >= core_->pak.pak_data_.size()) {
                return narrow<u16>(addr / 2_u32);
            }
            return memcpy<u16>(core_->pak.pak_data_, addr);
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(is_sram_flash(core_->pak.backup_type())) {
                return core_->pak.backup_->read(addr) * 0x0101_u16;
            }
            return 0xFFFF_u16;
        default:
            return narrow<u16>(read_unused(addr));
    }
}

void arm7tdmi::write_16(u32 addr, const u16 data, const mem_access access) noexcept
{
    const auto page = to_enum<memory_page>(addr >> 24_u32);

    ASSERT(!bitflags::is_set(access, mem_access::dry_run));
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
            memcpy<u16>(core_->ppu.palette_ram_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::vram:
            memcpy<u16>(core_->ppu.vram_, adjust_vram_addr(addr), data);
            break;
        case memory_page::oam_ram:
            memcpy<u16>(core_->ppu.oam_, addr & 0x0000'03FF_u32, data);
            break;
        case memory_page::pak_ws2_upper:
            if(is_eeprom(core_->pak.pak_data_.size(), core_->pak.backup_type(), addr)
              && bitflags::is_set(access, mem_access::dma)) {
                core_->pak.backup_->write(addr, narrow<u8>(data));
                break;
            }
            [[fallthrough]];
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower:
            addr &= cartridge::gamepak::default_mirror_mask;
            if(core_->pak.has_rtc_ && is_gpio(addr)) {
                core_->pak.rtc_.write(addr, narrow<u8>(data));
                core_->pak.rtc_.write(addr + 1_u32, narrow<u8>(data >> 8_u16));
            }
            break;
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(is_sram_flash(core_->pak.backup_type())) {
                return core_->pak.backup_->write(addr, narrow<u8>(data >> (8_u16 * (narrow<u16>(addr) & 0b1_u16))));
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
    const auto page = to_enum<memory_page>(addr >> 24_u32);

    if(LIKELY(!bitflags::is_set(access, mem_access::dry_run))) {
        tick_components(get_wait_cycles(wait_16, page, get_actual_access(page, addr, access)));
    }

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
            return memcpy<u8>(core_->ppu.palette_ram_, addr & 0x0000'03FF_u32);
        case memory_page::vram:
            return memcpy<u8>(core_->ppu.vram_, adjust_vram_addr(addr));
        case memory_page::oam_ram:
            return memcpy<u8>(core_->ppu.oam_, addr & 0x0000'03FF_u32);
        case memory_page::pak_ws0_lower: case memory_page::pak_ws0_upper:
        case memory_page::pak_ws1_lower: case memory_page::pak_ws1_upper:
        case memory_page::pak_ws2_lower: case memory_page::pak_ws2_upper:
            addr &= core_->pak.mirror_mask_;
            if (addr >= core_->pak.pak_data_.size()) {
                return narrow<u8>((addr / 2_u32) >> (bit::extract(addr, 1_u8) * 8_u32));
            }
            if(is_gpio(addr) && core_->pak.rtc_.read_allowed()) {
                return core_->pak.rtc_.read(addr);
            }
            return memcpy<u8>(core_->pak.pak_data_, addr);
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(is_sram_flash(core_->pak.backup_type())) {
                return core_->pak.backup_->read(addr);
            }
            return 0xFF_u8;
        default:
            return narrow<u8>(read_unused(addr));
    }
}

void arm7tdmi::write_8(u32 addr, const u8 data, const mem_access access) noexcept
{
    const auto page = to_enum<memory_page>(addr >> 24_u32);

    ASSERT(!bitflags::is_set(access, mem_access::dry_run));
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
            memcpy<u16>(core_->ppu.palette_ram_, addr & 0x0000'03FE_u32, data * 0x0101_u16);
            break;
        case memory_page::vram: {
            const u32 limit = core_->ppu.dispcnt_.bg_mode > 2 ? 0x1'4000_u32 : 0x1'0000_u32;
            if(const u32 adjusted_addr = adjust_vram_addr(addr); adjusted_addr < limit) {
                memcpy<u16>(core_->ppu.vram_, bit::clear(adjusted_addr, 0_u8), data * 0x0101_u16);
            }
            break;
        }
        case memory_page::oam_ram:
            break;
        case memory_page::pak_sram_1: case memory_page::pak_sram_2:
            addr &= 0x0EFF'FFFF_u32;
            if(is_sram_flash(core_->pak.backup_type())) {
                return core_->pak.backup_->write(addr, data);
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
        const auto current_page = to_enum<memory_page>(r15_ >> 24_u32);
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
                    // LSW = [$+4], MSW = [$+6]   ;for opcodes at 4-byte aligned locations
                    data = (widen<u32>(read_16(r15_ + 6_u32, mem_access::dry_run)) << 16_u32) | pipeline_.decoding;
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
    auto& ppu = core_->ppu;

    const auto win_enable_read = [](ppu::win_enable_bits& area) {
        return bit::from_bool<u8>(area.bg_enable[0_usize])
          | bit::from_bool<u8>(area.bg_enable[1_usize]) << 1_u8
          | bit::from_bool<u8>(area.bg_enable[2_usize]) << 2_u8
          | bit::from_bool<u8>(area.bg_enable[3_usize]) << 3_u8
          | bit::from_bool<u8>(area.obj_enable) << 4_u8
          | bit::from_bool<u8>(area.special_effect) << 5_u8;
    };

    switch(addr.get()) {
        case keypad::addr_state:     return narrow<u8>(core_->keypad.keyinput_);
        case keypad::addr_state + 1: return narrow<u8>(core_->keypad.keyinput_ >> 8_u16);
        case keypad::addr_control:   return narrow<u8>(core_->keypad.keycnt_.select);
        case keypad::addr_control + 1:
            return narrow<u8>(widen<u32>(core_->keypad.keycnt_.select) >> 8_u32 & 0b11_u32
              | bit::from_bool(core_->keypad.keycnt_.enabled) << 6_u32
              | from_enum<u32>(core_->keypad.keycnt_.cond_strategy) << 7_u32);

        case ppu::addr_dispcnt:
            return bit::from_bool<u8>(ppu.dispcnt_.forced_blank) << 7_u8
              | bit::from_bool<u8>(ppu.dispcnt_.obj_mapping_1d) << 6_u8
              | bit::from_bool<u8>(ppu.dispcnt_.hblank_interval_free) << 5_u8
              | ppu.dispcnt_.frame_select << 4_u8
              | ppu.dispcnt_.bg_mode;
        case ppu::addr_dispcnt + 1:
            return bit::from_bool<u8>(ppu.dispcnt_.enable_bg[0_usize])
            | bit::from_bool<u8>(ppu.dispcnt_.enable_bg[1_usize]) << 1_u8
            | bit::from_bool<u8>(ppu.dispcnt_.enable_bg[2_usize]) << 2_u8
            | bit::from_bool<u8>(ppu.dispcnt_.enable_bg[3_usize]) << 3_u8
            | bit::from_bool<u8>(ppu.dispcnt_.enable_obj) << 4_u8
            | bit::from_bool<u8>(ppu.dispcnt_.enable_w0) << 5_u8
            | bit::from_bool<u8>(ppu.dispcnt_.enable_w1) << 6_u8
            | bit::from_bool<u8>(ppu.dispcnt_.enable_wobj) << 7_u8;
        case ppu::addr_greenswap:     return bit::from_bool<u8>(ppu.green_swap_);
        case ppu::addr_greenswap + 1: return 0_u8;
        case ppu::addr_dispstat:
            return bit::from_bool<u8>(ppu.dispstat_.vblank)
              | bit::from_bool<u8>(ppu.dispstat_.hblank) << 1_u8
              | bit::from_bool<u8>(ppu.dispstat_.vcounter) << 2_u8
              | bit::from_bool<u8>(ppu.dispstat_.vblank_irq) << 3_u8
              | bit::from_bool<u8>(ppu.dispstat_.hblank_irq) << 4_u8
              | bit::from_bool<u8>(ppu.dispstat_.vcounter_irq) << 5_u8;
        case ppu::addr_dispstat + 1: return ppu.dispstat_.vcount_setting;
        case ppu::addr_vcount:       return ppu.vcount_;
        case ppu::addr_vcount + 1:   return 0_u8;
        case ppu::addr_bg0cnt:       return ppu.bg0_.cnt.read_lower();
        case ppu::addr_bg0cnt + 1:   return ppu.bg0_.cnt.read_upper();
        case ppu::addr_bg1cnt:       return ppu.bg1_.cnt.read_lower();
        case ppu::addr_bg1cnt + 1:   return ppu.bg1_.cnt.read_upper();
        case ppu::addr_bg2cnt:       return ppu.bg2_.cnt.read_lower();
        case ppu::addr_bg2cnt + 1:   return ppu.bg2_.cnt.read_upper();
        case ppu::addr_bg3cnt:       return ppu.bg3_.cnt.read_lower();
        case ppu::addr_bg3cnt + 1:   return ppu.bg3_.cnt.read_upper();
        case ppu::addr_winin:        return win_enable_read(ppu.win_in_.win0);
        case ppu::addr_winin + 1:    return win_enable_read(ppu.win_in_.win1);
        case ppu::addr_winout:       return win_enable_read(ppu.win_out_.outside);
        case ppu::addr_winout + 1:   return win_enable_read(ppu.win_out_.obj);
        case ppu::addr_bldcnt:
            return bit::from_bool<u8>(ppu.bldcnt_.first.bg[0_usize])
              | bit::from_bool<u8>(ppu.bldcnt_.first.bg[1_usize]) << 1_u8
              | bit::from_bool<u8>(ppu.bldcnt_.first.bg[2_usize]) << 2_u8
              | bit::from_bool<u8>(ppu.bldcnt_.first.bg[3_usize]) << 3_u8
              | bit::from_bool<u8>(ppu.bldcnt_.first.obj) << 4_u8
              | bit::from_bool<u8>(ppu.bldcnt_.first.backdrop) << 5_u8
              | from_enum<u8>(ppu.bldcnt_.effect_type) << 6_u8;
        case ppu::addr_bldcnt + 1:
            return bit::from_bool<u8>(ppu.bldcnt_.second.bg[0_usize])
              | bit::from_bool<u8>(ppu.bldcnt_.second.bg[1_usize]) << 1_u8
              | bit::from_bool<u8>(ppu.bldcnt_.second.bg[2_usize]) << 2_u8
              | bit::from_bool<u8>(ppu.bldcnt_.second.bg[3_usize]) << 3_u8
              | bit::from_bool<u8>(ppu.bldcnt_.second.obj) << 4_u8
              | bit::from_bool<u8>(ppu.bldcnt_.second.backdrop) << 5_u8;
        case ppu::addr_bldalpha: return ppu.blend_settings_.eva;
        case ppu::addr_bldalpha + 1: return ppu.blend_settings_.evb;

        case apu::addr_sound1cnt_l:
        case apu::addr_sound1cnt_l + 1:
        case apu::addr_sound1cnt_h:
        case apu::addr_sound1cnt_h + 1:
        case apu::addr_sound1cnt_x:
        case apu::addr_sound1cnt_x + 1:
        case apu::addr_sound2cnt_l:
        case apu::addr_sound2cnt_l + 1:
        case apu::addr_sound2cnt_h:
        case apu::addr_sound2cnt_h + 1:
        case apu::addr_sound3cnt_l:
        case apu::addr_sound3cnt_l + 1:
        case apu::addr_sound3cnt_h:
        case apu::addr_sound3cnt_h + 1:
        case apu::addr_sound3cnt_x:
        case apu::addr_sound3cnt_x + 1:
        case apu::addr_sound4cnt_l:
        case apu::addr_sound4cnt_l + 1:
        case apu::addr_sound4cnt_h:
        case apu::addr_sound4cnt_h + 1:
        case apu::addr_soundcnt_l:
        case apu::addr_soundcnt_l + 1:
        case apu::addr_soundcnt_h:
        case apu::addr_soundcnt_h + 1:
        case apu::addr_soundcnt_x:
        case apu::addr_soundcnt_x + 1:
        case apu::addr_soundbias:
        case apu::addr_soundbias + 1:
        case apu::addr_wave_ram:
        case apu::addr_wave_ram + 1: //???
        case apu::addr_fifo_a:
        case apu::addr_fifo_a + 1:
        case apu::addr_fifo_a + 2:
        case apu::addr_fifo_a + 3:
        case apu::addr_fifo_b:
        case apu::addr_fifo_b + 1:
        case apu::addr_fifo_b + 2:
        case apu::addr_fifo_b + 3:
            return 0_u8;

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

        case addr_dma0cnt_h:     return dma_controller_.channels[0_usize].read_cnt_l();
        case addr_dma0cnt_h + 1: return dma_controller_.channels[0_usize].read_cnt_h();
        case addr_dma1cnt_h:     return dma_controller_.channels[1_usize].read_cnt_l();
        case addr_dma1cnt_h + 1: return dma_controller_.channels[1_usize].read_cnt_h();
        case addr_dma2cnt_h:     return dma_controller_.channels[2_usize].read_cnt_l();
        case addr_dma2cnt_h + 1: return dma_controller_.channels[2_usize].read_cnt_h();
        case addr_dma3cnt_h:     return dma_controller_.channels[3_usize].read_cnt_l();
        case addr_dma3cnt_h + 1: return dma_controller_.channels[3_usize].read_cnt_h();

        case addr_ime:    return bit::from_bool<u8>(ime_);
        case addr_ie:     return narrow<u8>(ie_);
        case addr_ie + 1: return narrow<u8>(ie_ >> 8_u8);
        case addr_if:     return narrow<u8>(if_);
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
    auto& ppu = core_->ppu;

    const auto win_enable_write = [](ppu::win_enable_bits& area, const u8 data) {
        area.bg_enable[0_usize] = bit::test(data, 0_u8);
        area.bg_enable[1_usize] = bit::test(data, 1_u8);
        area.bg_enable[2_usize] = bit::test(data, 2_u8);
        area.bg_enable[3_usize] = bit::test(data, 3_u8);
        area.obj_enable = bit::test(data, 4_u8);
        area.special_effect = bit::test(data, 5_u8);
    };

    switch(addr.get()) {
        case keypad::addr_control:
            core_->keypad.keycnt_.select = bit::set_byte(core_->keypad.keycnt_.select, 0_u8, data);
            if(core_->keypad.interrupt_available()) {
                request_interrupt(arm::interrupt_source::keypad);
            }
            break;
        case keypad::addr_control + 1:
            core_->keypad.keycnt_.select = bit::set_byte(core_->keypad.keycnt_.select, 1_u8, data & 0b11_u8);
            core_->keypad.keycnt_.enabled = bit::test(data, 6_u8);
            core_->keypad.keycnt_.cond_strategy =
              to_enum<keypad::keypad::irq_control::condition_strategy>(bit::extract(data, 7_u8));
            if(core_->keypad.interrupt_available()) {
                request_interrupt(arm::interrupt_source::keypad);
            }
            break;

        case ppu::addr_dispcnt:
            ppu.dispcnt_.bg_mode = data & 0b111_u8;
            ppu.dispcnt_.frame_select = bit::extract(data, 4_u8);
            ppu.dispcnt_.hblank_interval_free = bit::test(data, 5_u8);
            ppu.dispcnt_.obj_mapping_1d = bit::test(data, 6_u8);
            ppu.dispcnt_.forced_blank = bit::test(data, 7_u8);
            break;
        case ppu::addr_dispcnt + 1:
            ppu.dispcnt_.enable_obj = bit::test(data, 4_u8);
            ppu.dispcnt_.enable_w0 = bit::test(data, 5_u8);
            ppu.dispcnt_.enable_w1 = bit::test(data, 6_u8);
            ppu.dispcnt_.enable_wobj = bit::test(data, 7_u8);
            for(u8 bg = 0_u8; bg < ppu.dispcnt_.enable_bg.size(); ++bg) {
                ppu.dispcnt_.enable_bg[bg] = bit::test(data, bg);
            }
            break;
        case ppu::addr_greenswap: ppu.green_swap_ = bit::test(data, 0_u8);
        case ppu::addr_dispstat:
            ppu.dispstat_.vblank_irq = bit::test(data, 3_u8);
            ppu.dispstat_.hblank_irq = bit::test(data, 4_u8);
            ppu.dispstat_.vcounter_irq = bit::test(data, 5_u8);
            break;
        case ppu::addr_dispstat + 1: ppu.dispstat_.vcount_setting = data; break;
        case ppu::addr_bg0cnt:      ppu.bg0_.cnt.write_lower(data); break;
        case ppu::addr_bg0cnt + 1:  ppu.bg0_.cnt.write_upper(data); break;
        case ppu::addr_bg1cnt:      ppu.bg1_.cnt.write_lower(data); break;
        case ppu::addr_bg1cnt + 1:  ppu.bg1_.cnt.write_upper(data); break;
        case ppu::addr_bg2cnt:      ppu.bg2_.cnt.write_lower(data); break;
        case ppu::addr_bg2cnt + 1:  ppu.bg2_.cnt.write_upper(data); break;
        case ppu::addr_bg3cnt:      ppu.bg3_.cnt.write_lower(data); break;
        case ppu::addr_bg3cnt + 1:  ppu.bg3_.cnt.write_upper(data); break;
        case ppu::addr_bg0hofs:     ppu.bg0_.hoffset = bit::set_byte(ppu.bg0_.hoffset, 0_u8, data); break;
        case ppu::addr_bg0hofs + 1: ppu.bg0_.hoffset = bit::set_byte(ppu.bg0_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg0vofs:     ppu.bg0_.voffset = bit::set_byte(ppu.bg0_.voffset, 0_u8, data); break;
        case ppu::addr_bg0vofs + 1: ppu.bg0_.voffset = bit::set_byte(ppu.bg0_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg1hofs:     ppu.bg1_.hoffset = bit::set_byte(ppu.bg1_.hoffset, 0_u8, data); break;
        case ppu::addr_bg1hofs + 1: ppu.bg1_.hoffset = bit::set_byte(ppu.bg1_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg1vofs:     ppu.bg1_.voffset = bit::set_byte(ppu.bg1_.voffset, 0_u8, data); break;
        case ppu::addr_bg1vofs + 1: ppu.bg1_.voffset = bit::set_byte(ppu.bg1_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg2hofs:     ppu.bg2_.hoffset = bit::set_byte(ppu.bg2_.hoffset, 0_u8, data); break;
        case ppu::addr_bg2hofs + 1: ppu.bg2_.hoffset = bit::set_byte(ppu.bg2_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg2vofs:     ppu.bg2_.voffset = bit::set_byte(ppu.bg2_.voffset, 0_u8, data); break;
        case ppu::addr_bg2vofs + 1: ppu.bg2_.voffset = bit::set_byte(ppu.bg2_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg3hofs:     ppu.bg3_.hoffset = bit::set_byte(ppu.bg3_.hoffset, 0_u8, data); break;
        case ppu::addr_bg3hofs + 1: ppu.bg3_.hoffset = bit::set_byte(ppu.bg3_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg3vofs:     ppu.bg3_.voffset = bit::set_byte(ppu.bg3_.voffset, 0_u8, data); break;
        case ppu::addr_bg3vofs + 1: ppu.bg3_.voffset = bit::set_byte(ppu.bg3_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg2pa:       ppu.bg2_.pa = bit::set_byte(ppu.bg2_.pa, 0_u8, data); break;
        case ppu::addr_bg2pa + 1:   ppu.bg2_.pa = bit::set_byte(ppu.bg2_.pa, 1_u8, data); break;
        case ppu::addr_bg2pb:       ppu.bg2_.pb = bit::set_byte(ppu.bg2_.pb, 0_u8, data); break;
        case ppu::addr_bg2pb + 1:   ppu.bg2_.pb = bit::set_byte(ppu.bg2_.pb, 1_u8, data); break;
        case ppu::addr_bg2pc:       ppu.bg2_.pc = bit::set_byte(ppu.bg2_.pc, 0_u8, data); break;
        case ppu::addr_bg2pc + 1:   ppu.bg2_.pc = bit::set_byte(ppu.bg2_.pc, 1_u8, data); break;
        case ppu::addr_bg2pd:       ppu.bg2_.pd = bit::set_byte(ppu.bg2_.pd, 0_u8, data); break;
        case ppu::addr_bg2pd + 1:   ppu.bg2_.pd = bit::set_byte(ppu.bg2_.pd, 1_u8, data); break;
        case ppu::addr_bg2x:        ppu.bg2_.x_ref.set_byte(0_u8, data); break;
        case ppu::addr_bg2x + 1:    ppu.bg2_.x_ref.set_byte(1_u8, data); break;
        case ppu::addr_bg2x + 2:    ppu.bg2_.x_ref.set_byte(2_u8, data); break;
        case ppu::addr_bg2x + 3:    ppu.bg2_.x_ref.set_byte(3_u8, data); break;
        case ppu::addr_bg2y:        ppu.bg2_.y_ref.set_byte(0_u8, data); break;
        case ppu::addr_bg2y + 1:    ppu.bg2_.y_ref.set_byte(1_u8, data); break;
        case ppu::addr_bg2y + 2:    ppu.bg2_.y_ref.set_byte(2_u8, data); break;
        case ppu::addr_bg2y + 3:    ppu.bg2_.y_ref.set_byte(3_u8, data); break;
        case ppu::addr_bg3pa:       ppu.bg3_.pa = bit::set_byte(ppu.bg3_.pa, 0_u8, data); break;
        case ppu::addr_bg3pa + 1:   ppu.bg3_.pa = bit::set_byte(ppu.bg3_.pa, 1_u8, data); break;
        case ppu::addr_bg3pb:       ppu.bg3_.pb = bit::set_byte(ppu.bg3_.pb, 0_u8, data); break;
        case ppu::addr_bg3pb + 1:   ppu.bg3_.pb = bit::set_byte(ppu.bg3_.pb, 1_u8, data); break;
        case ppu::addr_bg3pc:       ppu.bg3_.pc = bit::set_byte(ppu.bg3_.pc, 0_u8, data); break;
        case ppu::addr_bg3pc + 1:   ppu.bg3_.pc = bit::set_byte(ppu.bg3_.pc, 1_u8, data); break;
        case ppu::addr_bg3pd:       ppu.bg3_.pd = bit::set_byte(ppu.bg3_.pd, 0_u8, data); break;
        case ppu::addr_bg3pd + 1:   ppu.bg3_.pd = bit::set_byte(ppu.bg3_.pd, 1_u8, data); break;
        case ppu::addr_bg3x:        ppu.bg3_.x_ref.set_byte(0_u8, data); break;
        case ppu::addr_bg3x + 1:    ppu.bg3_.x_ref.set_byte(1_u8, data); break;
        case ppu::addr_bg3x + 2:    ppu.bg3_.x_ref.set_byte(2_u8, data); break;
        case ppu::addr_bg3x + 3:    ppu.bg3_.x_ref.set_byte(3_u8, data); break;
        case ppu::addr_bg3y:        ppu.bg3_.y_ref.set_byte(0_u8, data); break;
        case ppu::addr_bg3y + 1:    ppu.bg3_.y_ref.set_byte(1_u8, data); break;
        case ppu::addr_bg3y + 2:    ppu.bg3_.y_ref.set_byte(2_u8, data); break;
        case ppu::addr_bg3y + 3:    ppu.bg3_.y_ref.set_byte(3_u8, data); break;

        // todo Garbage values of X2>240 or X1>X2 are interpreted as X2=240.
        case ppu::addr_win0h:       ppu.win0_.dim_x2 = data; break; // fixme + 1 (GBATEK, means -1 actually, see mosaic)
        case ppu::addr_win0h + 1:   ppu.win0_.dim_x1 = data; break;
        case ppu::addr_win1h:       ppu.win1_.dim_x2 = data; break; // fixme + 1 (GBATEK, means -1 actually, see mosaic)
        case ppu::addr_win1h + 1:   ppu.win1_.dim_x1 = data; break;
        // todo Garbage values of Y2>160 or Y1>Y2 are interpreted as Y2=160.
        case ppu::addr_win0v:       ppu.win0_.dim_y2 = data; break; // fixme + 1 (GBATEK, means -1 actually, see mosaic)
        case ppu::addr_win0v + 1:   ppu.win0_.dim_y1 = data; break;
        case ppu::addr_win1v:       ppu.win1_.dim_y2 = data; break; // fixme + 1 (GBATEK, means -1 actually, see mosaic)
        case ppu::addr_win1v + 1:   ppu.win1_.dim_y1 = data; break;
        case ppu::addr_winin:       win_enable_write(ppu.win_in_.win0, data); break;
        case ppu::addr_winin + 1:   win_enable_write(ppu.win_in_.win1, data); break;
        case ppu::addr_winout:      win_enable_write(ppu.win_out_.outside, data); break;
        case ppu::addr_winout + 1:  win_enable_write(ppu.win_out_.obj, data); break;
        case ppu::addr_mosaic:
            ppu.mosaic_.bg.h = (data & 0xF_u8) + 1_u8;
            ppu.mosaic_.bg.v = ((data >> 4_u8) & 0xF_u8) + 1_u8;
            break;
        case ppu::addr_mosaic + 1:
            ppu.mosaic_.obj.h = (data & 0xF_u8) + 1_u8;
            ppu.mosaic_.obj.v = ((data >> 4_u8) & 0xF_u8) + 1_u8;
            break;
        case ppu::addr_bldcnt:
            ppu.bldcnt_.first.bg[0_usize] = bit::test(data, 0_u8);
            ppu.bldcnt_.first.bg[1_usize] = bit::test(data, 1_u8);
            ppu.bldcnt_.first.bg[2_usize] = bit::test(data, 2_u8);
            ppu.bldcnt_.first.bg[3_usize] = bit::test(data, 3_u8);
            ppu.bldcnt_.first.obj = bit::test(data, 4_u8);
            ppu.bldcnt_.first.backdrop = bit::test(data, 5_u8);
            ppu.bldcnt_.effect_type = to_enum<ppu::bldcnt::effect>((data >> 6_u8) & 0b11_u8);
            break;
        case ppu::addr_bldcnt + 1:
            ppu.bldcnt_.second.bg[0_usize] = bit::test(data, 0_u8);
            ppu.bldcnt_.second.bg[1_usize] = bit::test(data, 1_u8);
            ppu.bldcnt_.second.bg[2_usize] = bit::test(data, 2_u8);
            ppu.bldcnt_.second.bg[3_usize] = bit::test(data, 3_u8);
            ppu.bldcnt_.second.obj = bit::test(data, 4_u8);
            ppu.bldcnt_.second.backdrop = bit::test(data, 5_u8);
            break;
        case ppu::addr_bldalpha:     ppu.blend_settings_.eva = data & 0x1F_u8; break;
        case ppu::addr_bldalpha + 1: ppu.blend_settings_.evb = data & 0x1F_u8; break;
        case ppu::addr_bldy:         ppu.blend_settings_.evy = data & 0x1F_u8; break;

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

        case addr_dma0sad:       dma_controller_.channels[0_usize].write_src(0_u8, data); break;
        case addr_dma0sad + 1:   dma_controller_.channels[0_usize].write_src(1_u8, data); break;
        case addr_dma0sad + 2:   dma_controller_.channels[0_usize].write_src(2_u8, data); break;
        case addr_dma0sad + 3:   dma_controller_.channels[0_usize].write_src(3_u8, data); break;
        case addr_dma0dad:       dma_controller_.channels[0_usize].write_dst(0_u8, data); break;
        case addr_dma0dad + 1:   dma_controller_.channels[0_usize].write_dst(1_u8, data); break;
        case addr_dma0dad + 2:   dma_controller_.channels[0_usize].write_dst(2_u8, data); break;
        case addr_dma0dad + 3:   dma_controller_.channels[0_usize].write_dst(3_u8, data); break;
        case addr_dma0cnt_l:     dma_controller_.channels[0_usize].write_count(0_u8, data); break;
        case addr_dma0cnt_l + 1: dma_controller_.channels[0_usize].write_count(1_u8, data); break;
        case addr_dma0cnt_h:     dma_controller_.write_cnt_l(0_usize, data); break;
        case addr_dma0cnt_h + 1: dma_controller_.write_cnt_h(0_usize, data); break;
        case addr_dma1sad:       dma_controller_.channels[1_usize].write_src(0_u8, data); break;
        case addr_dma1sad + 1:   dma_controller_.channels[1_usize].write_src(1_u8, data); break;
        case addr_dma1sad + 2:   dma_controller_.channels[1_usize].write_src(2_u8, data); break;
        case addr_dma1sad + 3:   dma_controller_.channels[1_usize].write_src(3_u8, data); break;
        case addr_dma1dad:       dma_controller_.channels[1_usize].write_dst(0_u8, data); break;
        case addr_dma1dad + 1:   dma_controller_.channels[1_usize].write_dst(1_u8, data); break;
        case addr_dma1dad + 2:   dma_controller_.channels[1_usize].write_dst(2_u8, data); break;
        case addr_dma1dad + 3:   dma_controller_.channels[1_usize].write_dst(3_u8, data); break;
        case addr_dma1cnt_l:     dma_controller_.channels[1_usize].write_count(0_u8, data); break;
        case addr_dma1cnt_l + 1: dma_controller_.channels[1_usize].write_count(1_u8, data); break;
        case addr_dma1cnt_h:     dma_controller_.write_cnt_l(1_usize, data); break;
        case addr_dma1cnt_h + 1: dma_controller_.write_cnt_h(1_usize, data); break;
        case addr_dma2sad:       dma_controller_.channels[2_usize].write_src(0_u8, data); break;
        case addr_dma2sad + 1:   dma_controller_.channels[2_usize].write_src(1_u8, data); break;
        case addr_dma2sad + 2:   dma_controller_.channels[2_usize].write_src(2_u8, data); break;
        case addr_dma2sad + 3:   dma_controller_.channels[2_usize].write_src(3_u8, data); break;
        case addr_dma2dad:       dma_controller_.channels[2_usize].write_dst(0_u8, data); break;
        case addr_dma2dad + 1:   dma_controller_.channels[2_usize].write_dst(1_u8, data); break;
        case addr_dma2dad + 2:   dma_controller_.channels[2_usize].write_dst(2_u8, data); break;
        case addr_dma2dad + 3:   dma_controller_.channels[2_usize].write_dst(3_u8, data); break;
        case addr_dma2cnt_l:     dma_controller_.channels[2_usize].write_count(0_u8, data); break;
        case addr_dma2cnt_l + 1: dma_controller_.channels[2_usize].write_count(1_u8, data); break;
        case addr_dma2cnt_h:     dma_controller_.write_cnt_l(2_usize, data); break;
        case addr_dma2cnt_h + 1: dma_controller_.write_cnt_h(2_usize, data); break;
        case addr_dma3sad:       dma_controller_.channels[3_usize].write_src(0_u8, data); break;
        case addr_dma3sad + 1:   dma_controller_.channels[3_usize].write_src(1_u8, data); break;
        case addr_dma3sad + 2:   dma_controller_.channels[3_usize].write_src(2_u8, data); break;
        case addr_dma3sad + 3:   dma_controller_.channels[3_usize].write_src(3_u8, data); break;
        case addr_dma3dad:       dma_controller_.channels[3_usize].write_dst(0_u8, data); break;
        case addr_dma3dad + 1:   dma_controller_.channels[3_usize].write_dst(1_u8, data); break;
        case addr_dma3dad + 2:   dma_controller_.channels[3_usize].write_dst(2_u8, data); break;
        case addr_dma3dad + 3:   dma_controller_.channels[3_usize].write_dst(3_u8, data); break;
        case addr_dma3cnt_l:     dma_controller_.channels[3_usize].write_count(0_u8, data); break;
        case addr_dma3cnt_l + 1: dma_controller_.channels[3_usize].write_count(1_u8, data); break;
        case addr_dma3cnt_h:     dma_controller_.write_cnt_l(3_usize, data); break;
        case addr_dma3cnt_h + 1: dma_controller_.write_cnt_h(3_usize, data); break;

        case addr_ime:
            ime_ = bit::test(data, 0_u8);
            break;
        case addr_ie:
            ie_ = bit::set_byte(ie_, 0_u8, data);
            break;
        case addr_ie + 1:
            ie_ = bit::set_byte(ie_, 1_u8, data & 0x3F_u8);
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
            haltcnt_ = to_enum<halt_control>(bit::extract(data, 0_u8));
            break;
        case addr_postboot:
            post_boot_ = bit::extract(data, 0_u8);
            break;
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
