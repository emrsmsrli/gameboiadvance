/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cpu/arm7tdmi.h>

namespace gba::cpu {

u32 arm7tdmi::read_32_aligned(const u32 addr, const mem_access access) noexcept
{
    const u32 data = bus_->read_32(addr, access);
    const u32 rotate_amount = (addr & 0b11_u32) * 8_u32;
    return (data >> rotate_amount) | (data << (32_u32 - rotate_amount));
}

u32 arm7tdmi::read_16_signed(const u32 addr, const mem_access access) noexcept
{
    if(bit::test(addr, 0_u8)) {
        return make_unsigned(math::sign_extend<8>(widen<u32>(bus_->read_8(addr, access))));
    }
    return make_unsigned(math::sign_extend<16>(widen<u32>(bus_->read_16(addr, access))));
}

u32 arm7tdmi::read_16_aligned(const u32 addr, const mem_access access) noexcept
{
    const u32 data = bus_->read_16(addr, access);
    const u32 rotate_amount = bit::extract(addr, 0_u8);
    return (data >> (8_u32 * rotate_amount)) | (data << (24_u8 * rotate_amount));
}

u32 arm7tdmi::read_8_signed(const u32 addr, const mem_access access) noexcept
{
    return make_unsigned(math::sign_extend<8>(widen<u32>(bus_->read_8(addr, access))));
}

} // namespace gba::cpu
