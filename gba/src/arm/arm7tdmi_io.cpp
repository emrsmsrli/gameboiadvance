/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/arm/arm7tdmi.h>

namespace gba {

u32 arm7tdmi::read_32_rotated(const u32 addr, const mem_access access) const noexcept
{
    u32 data = read_32(addr, access);
    u32 rotate_amount = (data & 0b11_u32) * 8_u32;
    return (data >> rotate_amount) | (data << (32_u32 - rotate_amount));
}

u32 arm7tdmi::read_32(const u32 addr, const mem_access access) const noexcept
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

u32 arm7tdmi::read_16_rotated(const u32 addr, const mem_access access) const noexcept
{
    u32 data = read_16(addr, access);
    if(bit::test(data, 0_u8)) {
        data = (data >> 8_u32) | (data << 24_u8);
    }
    return data;
}

u32 arm7tdmi::read_16(const u32 addr, const mem_access access) const noexcept
{
    UNREACHABLE();
}

void arm7tdmi::write_16(const u32 addr, const u16 data, const mem_access access) noexcept
{
    UNREACHABLE();
}

u32 arm7tdmi::read_8(const u32 addr, const mem_access access) const noexcept
{
    UNREACHABLE();
}

void arm7tdmi::write_8(const u32 addr, const u8 data, const mem_access access) noexcept
{
    UNREACHABLE();
}

} // namespace gba
