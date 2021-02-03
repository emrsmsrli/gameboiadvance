/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>
#include <gba/gba.h>

namespace gba::arm {

namespace {

constexpr auto addr_ie = 0x400'0200_u32;            //  2    R/W  IE        Interrupt Enable Register
constexpr auto addr_if = 0x400'0202_u32;            //  2    R/W  IF        Interrupt Request Flags / IRQ Acknowledge
constexpr auto addr_waitcnt = 0x400'0204_u32;       //  2    R/W  WAITCNT   Game Pak Waitstate Control
constexpr auto addr_ime = 0x400'0208_u32;           //  2    R/W  IME       Interrupt Master Enable Register
constexpr auto addr_postboot = 0x400'0300_u32;      //  1    R/W  POSTFLG   Undocumented - Post Boot Flag
constexpr auto addr_haltcnt = 0x400'0301_u32;       //  1    W    HALTCNT   Undocumented - Power Down Control

} // namespace

u32 arm7tdmi::read_32_aligned(const u32 addr, const mem_access access) noexcept
{
    const u32 data = read_32(addr, access);
    const u32 rotate_amount = (addr & 0b11_u32) * 8_u32;
    return (data >> rotate_amount) | (data << (32_u32 - rotate_amount));
}

u32 arm7tdmi::read_32(const u32 addr, const mem_access access) noexcept
{
    if(addr < 0x0000'3FFF_u32) {
        if(r(15_u8) < 0x0000'3FFF_u32) {
            return 0_u32;
        }

        return memcpy<u32>(bios_, addr);
    }

    return 0_u32;
}

void arm7tdmi::write_32(const u32 addr, const u32 data, const mem_access access) noexcept
{
    UNREACHABLE();
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

u32 arm7tdmi::read_16(const u32 addr, const mem_access access) noexcept
{
    UNREACHABLE();
}

void arm7tdmi::write_16(const u32 addr, const u16 data, const mem_access access) noexcept
{
    UNREACHABLE();
}

u32 arm7tdmi::read_8_signed(const u32 addr, const mem_access access) noexcept
{
    return make_unsigned(math::sign_extend<8>(widen<u32>(read_8(addr, access))));
}

u32 arm7tdmi::read_8(const u32 addr, const mem_access access) noexcept
{
    UNREACHABLE();
}

void arm7tdmi::write_8(const u32 addr, const u8 data, const mem_access access) noexcept
{
    UNREACHABLE();
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

u8 arm7tdmi::read_io(const u32 addr) noexcept
{
    switch(addr.get()) {
        case keypad::keypad::addr_state: return narrow<u8>(gba_->keypad.keyinput_);
        case keypad::keypad::addr_state + 1: return narrow<u8>(gba_->keypad.keyinput_ >> 8_u16);
        case keypad::keypad::addr_control: return narrow<u8>(gba_->keypad.keycnt_.select);
        case keypad::keypad::addr_control + 1:
            return narrow<u8>(widen<u32>(gba_->keypad.keycnt_.select) >> 8_u32 & 0b11_u32
              | bit::from_bool(gba_->keypad.keycnt_.enabled) << 6_u32
              | static_cast<u32::type>(gba_->keypad.keycnt_.cond_strategy) << 7_u32);

        case addr_ime: return bit::from_bool<u8>(ime_);
        case addr_ie: return narrow<u8>(ie_);
        case addr_ie + 1: return narrow<u8>(ie_ >> 8_u8);
        case addr_if: return narrow<u8>(if_);
        case addr_if + 1: return narrow<u8>(if_ >> 8_u8);
    }

    return 0_u8;
}

void arm7tdmi::write_io(const u32 addr, const u8 data) noexcept
{
    switch(addr.get()) {
        case keypad::keypad::addr_control:
            gba_->keypad.keycnt_.select = (gba_->keypad.keycnt_.select & 0xFF00_u16) | data;
            if(gba_->keypad.interrupt_available()) {
                request_interrupt(arm::interrupt_source::keypad);
            }
            break;
        case keypad::keypad::addr_control + 1:
            gba_->keypad.keycnt_.select = (gba_->keypad.keycnt_.select & 0x00FF_u16)
              | (widen<u16>(data & 0b11_u8) << 8_u16);
            gba_->keypad.keycnt_.enabled = bit::test(data, 6_u8);
            gba_->keypad.keycnt_.cond_strategy =
              static_cast<keypad::keypad::irq_control::condition_strategy>(bit::extract(data, 7_u8).get());
            if(gba_->keypad.interrupt_available()) {
                request_interrupt(arm::interrupt_source::keypad);
            }
            break;

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
    }
}

} // namespace gba::arm
