/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */


#ifndef GAMEBOIADVANCE_CORE_BUS_H
#define GAMEBOIADVANCE_CORE_BUS_H

#include <gba/cpu/bus_interface.h>

namespace gba {

namespace detail {

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
    return cartridge::rtc::port_data <= addr && addr <= cartridge::rtc::port_control;
}

// gets called in 0xD-0xE addr range only
FORCEINLINE constexpr bool is_eeprom(const usize pak_size, const cartridge::backup::type type, const u32 addr) noexcept
{
    constexpr usize mb_16 = 16_kb * 1024_usize;
    return (type == cartridge::backup::type::eeprom_undetected
        || type == cartridge::backup::type::eeprom_64
        || type == cartridge::backup::type::eeprom_4)
      && (pak_size <= mb_16 || addr >= 0x0DFF'FF00_u32);
}

FORCEINLINE constexpr bool is_sram_flash(const cartridge::backup::type type) noexcept
{
    return type == cartridge::backup::type::sram
      || type == cartridge::backup::type::flash_64
      || type == cartridge::backup::type::flash_128;
}

FORCEINLINE cpu::mem_access force_nonseq_access(const u32 addr, const cpu::mem_access default_access) noexcept
{
    // force nonseq access on 128kb boundaries in rom
    if((addr & 0x1'FFFF_u32) == 0_u32) {
        return cpu::mem_access::non_seq;
    }
    return default_access;
}

} // namespace detail

namespace traits {

template<typename T> inline constexpr bool is_word_access = std::is_same_v<T, u32>;
template<typename T> inline constexpr bool is_hword_access = std::is_same_v<T, u16>;
template<typename T> inline constexpr bool is_byte_access = std::is_same_v<T, u8>;

template<typename T> struct io_traits;

template<>
struct io_traits<u32> {
#if WITH_DEBUGGER
    static inline constexpr cpu::debugger_access_width debugger_access_width = cpu::debugger_access_width::word;
#endif // WITH_DEBUGGER

    static inline constexpr u32 addr_alignment_mask = 0b11_u32;
    static inline constexpr u32 sram_flash_panning_mask = 0x0101'0101_u32;
};

template<>
struct io_traits<u16> {
#if WITH_DEBUGGER
    static inline constexpr cpu::debugger_access_width debugger_access_width = cpu::debugger_access_width::hword;
#endif // WITH_DEBUGGER

    static inline constexpr u32 addr_alignment_mask = 0b01_u32;
    static inline constexpr u16 sram_flash_panning_mask = 0x0101_u16;
};

template<>
struct io_traits<u8> {
#if WITH_DEBUGGER
    static inline constexpr cpu::debugger_access_width debugger_access_width = cpu::debugger_access_width::byte;
#endif // WITH_DEBUGGER

    static inline constexpr u32 addr_alignment_mask = 0b00_u32;
    static inline constexpr u8 sram_flash_panning_mask = 0x1_u8;
};

} // namespace traits

template<typename T>
T core::read(u32 addr, cpu::mem_access access) noexcept
{
#if WITH_DEBUGGER
    on_io_read(addr, traits::io_traits<T>::debugger_access_width);
#endif // WITH_DEBUGGER

    const auto page = to_enum<cpu::memory_page>(addr >> 24_u32);
    if(LIKELY(access != cpu::mem_access::none)) {
        const bool accessing_rom = page >= cpu::memory_page::pak_ws0_lower && page <= cpu::memory_page::pak_ws2_upper;
        if(accessing_rom) {
            access = detail::force_nonseq_access(addr, access);
        }

        const u32 cycles = cpu_.stall_cycles<T>(access, page);
        if(LIKELY(cpu_.waitcnt_.prefetch_buffer_enable) && accessing_rom) {
            cpu_.prefetch(addr, cycles);
        } else {
            tick_components(cycles);
        }
    }

    if constexpr(traits::io_traits<T>::addr_alignment_mask != 0_u32) {
        if(LIKELY(page != cpu::memory_page::pak_sram_1 && page != cpu::memory_page::pak_sram_2)) {
            addr = mask::clear(addr, traits::io_traits<T>::addr_alignment_mask);
        }
    }

    switch(page) {
        case cpu::memory_page::bios:
            return narrow<T>(cpu_.read_bios(addr));
        case cpu::memory_page::ewram:
            return memcpy<T>(cpu_.wram_, addr & 0x0003'FFFF_u32);
        case cpu::memory_page::iwram:
            return memcpy<T>(cpu_.iwram_, addr & 0x0000'7FFF_u32);
        case cpu::memory_page::io: {
            u32 io_val;
            for(u32 idx = 0_u32; idx < sizeof(T); ++idx) {
                io_val |= widen<u32>(read_io(addr + idx)) << (8_u32 * idx);
            }
            return narrow<T>(io_val);
        }
        case cpu::memory_page::palette_ram:
            return memcpy<T>(ppu_engine_.palette_ram_, addr & 0x0000'03FF_u32);
        case cpu::memory_page::vram:
            return memcpy<T>(ppu_engine_.vram_, detail::adjust_vram_addr(addr));
        case cpu::memory_page::oam_ram:
            return memcpy<T>(ppu_engine_.oam_, addr & 0x0000'03FF_u32);
        case cpu::memory_page::pak_ws2_upper:
            if(UNLIKELY(detail::is_eeprom(gamepak_.pak_data_.size(), gamepak_.backup_type(), addr))) {
                if constexpr(traits::is_word_access<T>) {
                    return widen<u32>(gamepak_.backup_->read(addr))
                      | widen<u32>(gamepak_.backup_->read(addr)) << 16_u32;
                } else if constexpr(traits::is_hword_access<T>) {
                    return gamepak_.backup_->read(addr);
                }
            }
            [[fallthrough]];
        case cpu::memory_page::pak_ws0_lower: case cpu::memory_page::pak_ws0_upper:
        case cpu::memory_page::pak_ws1_lower: case cpu::memory_page::pak_ws1_upper:
        case cpu::memory_page::pak_ws2_lower:
            addr &= gamepak_.mirror_mask_;
            if(UNLIKELY(detail::is_gpio(addr)) && gamepak_.rtc_.read_allowed()) {
                if constexpr(traits::is_word_access<T>) {
                    return widen<u32>(gamepak_.rtc_.read(addr))
                      | widen<u32>(gamepak_.rtc_.read(addr + 2_u32)) << 16_u32;
                } else if constexpr(traits::is_hword_access<T>) {
                    return gamepak_.rtc_.read(addr);
                }
            }

            if(UNLIKELY(addr >= gamepak_.pak_data_.size())) {
                if constexpr(traits::is_word_access<T>) {
                    const u32 fill_addr = (addr >> 1_u32) & 0xFFFF_u32;
                    return ((fill_addr + 1_u32) << 16_u32) | fill_addr;
                } else if constexpr(traits::is_hword_access<T>) {
                    return narrow<u16>(addr >> 1_u32);
                } else {
                    return narrow<u8>((addr >> 1_u32) >> (bit::extract(addr, 0_u8) << 3_u32));
                }
            }

            return memcpy<T>(gamepak_.pak_data_, addr);
        case cpu::memory_page::pak_sram_1: case cpu::memory_page::pak_sram_2: {
            if(cpu_.prefetch_buffer_.active) {
                cpu_.prefetch_buffer_.active = false;
                cpu_.prefetch_buffer_.size = 0_u32;
                tick_components(1_u32);
            }

            addr &= 0x0EFF'FFFF_u32;
            T data = 0xFF_u8;
            if(detail::is_sram_flash(gamepak_.backup_type())) {
                data = gamepak_.backup_->read(addr);
            }
            return data * traits::io_traits<T>::sram_flash_panning_mask;
        }
        default:
            return narrow<T>(cpu_.read_unused(addr));
    }
}

template<typename T>
void core::write(u32 addr, const T data, cpu::mem_access access) noexcept
{
#if WITH_DEBUGGER
    on_io_write(addr, data, traits::io_traits<T>::debugger_access_width);
#endif // WITH_DEBUGGER

    const auto page = to_enum<cpu::memory_page>(addr >> 24_u32);

    ASSERT(access != cpu::mem_access::none);
    const bool accessing_rom = page >= cpu::memory_page::pak_ws0_lower && page <= cpu::memory_page::pak_ws2_upper;
    if(accessing_rom) {
        access = detail::force_nonseq_access(addr, access);
    }

    const u32 cycles = cpu_.stall_cycles<T>(access, page);
    if(LIKELY(cpu_.waitcnt_.prefetch_buffer_enable) && accessing_rom) {
        cpu_.prefetch(addr, cycles);
    } else {
        tick_components(cycles);
    }

    if constexpr(traits::io_traits<T>::addr_alignment_mask != 0_u32) {
        if(LIKELY(page != cpu::memory_page::pak_sram_1 && page != cpu::memory_page::pak_sram_2)) {
            addr = mask::clear(addr, traits::io_traits<T>::addr_alignment_mask);
        }
    }

    switch(page) {
        case cpu::memory_page::ewram:
            memcpy<T>(cpu_.wram_, addr & 0x0003'FFFF_u32, data);
            break;
        case cpu::memory_page::iwram:
            memcpy<T>(cpu_.iwram_, addr & 0x0000'7FFF_u32, data);
            break;
        case cpu::memory_page::io:
            for(u32 idx = 0_u32; idx < sizeof(T); ++idx) {
                write_io(addr + idx, narrow<u8>(data >> (8_u32 * idx)));
            }
            break;
        case cpu::memory_page::palette_ram:
            if constexpr(traits::is_byte_access<T>) {
                memcpy<u16>(ppu_engine_.palette_ram_, addr & 0x0000'03FE_u32, data * 0x0101_u16);
            } else {
                memcpy<T>(ppu_engine_.palette_ram_, addr & 0x0000'03FF_u32, data);
            }
            break;
        case cpu::memory_page::vram:
            if constexpr(traits::is_byte_access<T>) {
                const u32 limit = ppu_engine_.dispcnt_.bg_mode > 2 ? 0x1'4000_u32 : 0x1'0000_u32;
                if(const u32 adjusted_addr = detail::adjust_vram_addr(addr); adjusted_addr < limit) {
                    memcpy<u16>(ppu_engine_.vram_, bit::clear(adjusted_addr, 0_u8), data * 0x0101_u16);
                }
            } else {
                memcpy<T>(ppu_engine_.vram_, detail::adjust_vram_addr(addr), data);
            }
            break;
        case cpu::memory_page::oam_ram:
            if constexpr(!traits::is_byte_access<T>) {
                memcpy<T>(ppu_engine_.oam_, addr & 0x0000'03FF_u32, data);
            }
            break;
        case cpu::memory_page::pak_ws2_upper:
            if constexpr(traits::is_hword_access<T>) {
                const bool is_eeprom = detail::is_eeprom(gamepak_.pak_data_.size(), gamepak_.backup_type(), addr);
                if(UNLIKELY(is_eeprom)) {
                    if(cpu_.dma_controller_.is_running()) {
                        if(UNLIKELY(gamepak_.backup_type() == cartridge::backup::type::eeprom_undetected)) {
                            const bool is_eeprom_64 = cpu_.dma_controller_[3_usize].internal.count == 17_u8;
                            gamepak_.on_eeprom_bus_width_detected(is_eeprom_64
                              ? cartridge::backup::type::eeprom_64
                              : cartridge::backup::type::eeprom_4);
                        }

                        gamepak_.backup_->write(addr, narrow<u8>(data));
                    }
                    break;
                }
            }
            [[fallthrough]];
        case cpu::memory_page::pak_ws0_lower: case cpu::memory_page::pak_ws0_upper:
        case cpu::memory_page::pak_ws1_lower: case cpu::memory_page::pak_ws1_upper:
        case cpu::memory_page::pak_ws2_lower:
            addr &= cartridge::gamepak::default_mirror_mask;
            if constexpr(traits::is_word_access<T>) {
                if(gamepak_.has_rtc_ && detail::is_gpio(addr)) {
                    gamepak_.rtc_.write(addr, narrow<u8>(data));
                    gamepak_.rtc_.write(addr + 2_u32, narrow<u8>(data >> 16_u32));
                }
            } else if constexpr(traits::is_hword_access<T>) {
                if(gamepak_.has_rtc_ && detail::is_gpio(addr)) {
                    gamepak_.rtc_.write(addr, narrow<u8>(data));
                }
            }
            break;
        case cpu::memory_page::pak_sram_1: case cpu::memory_page::pak_sram_2:
            if(detail::is_sram_flash(gamepak_.backup_type())) {
                addr &= 0x0EFF'FFFF_u32;
                if constexpr(traits::is_word_access<T>) {
                    gamepak_.backup_->write(addr, narrow<u8>(data >> (8_u32 * (addr & 0b11_u32))));
                } else if constexpr(traits::is_hword_access<T>) {
                    gamepak_.backup_->write(addr, narrow<u8>(data >> (8_u16 * (narrow<u16>(addr) & 0b1_u16))));
                } else {
                    gamepak_.backup_->write(addr, data);
                }
            }
            break;
        default:
            if constexpr(traits::is_word_access<T>) {
                LOG_DEBUG(arm::io, "invalid write32 to address {:08X}, {:08X}", addr, data);
            } else if constexpr(traits::is_hword_access<T>) {
                LOG_DEBUG(arm::io, "invalid write16 to address {:08X}, {:04X}", addr, data);
            } else {
                LOG_DEBUG(arm::io, "invalid write8 to address {:08X}, {:02X}", addr, data);
            }
            break;
    }
}

} // namespace gba

#endif //GAMEBOIADVANCE_CORE_BUS_H
