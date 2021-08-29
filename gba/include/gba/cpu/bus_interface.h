/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */


#ifndef GAMEBOIADVANCE_MEMORY_BUS_INTERFACE_H
#define GAMEBOIADVANCE_MEMORY_BUS_INTERFACE_H

#include <gba/core/integer.h>
#include <gba/helper/bitflags.h>

namespace gba::cpu {

#if WITH_DEBUGGER
enum class debugger_access_width : u32::type { byte, hword, word, any };
#endif // WITH_DEBUGGER

enum class mem_access : u32::type {
    non_seq,
    seq,
    none
};

struct bus_interface {
    virtual ~bus_interface() = default;

    virtual u32 read_32(u32 addr, mem_access access) noexcept = 0;
    virtual void write_32(u32 addr, u32 data, mem_access access) noexcept = 0;

    virtual u16 read_16(u32 addr, mem_access access) noexcept = 0;
    virtual void write_16(u32 addr, u16 data, mem_access access) noexcept = 0;

    virtual u8 read_8(u32 addr, mem_access access) noexcept = 0;
    virtual void write_8(u32 addr, u8 data, mem_access access) noexcept = 0;

    virtual void tick_components(u32 cycles) noexcept = 0;
    virtual void idle() noexcept = 0;
};

} // namespace gba::cpu

ENABLE_BITFLAG_OPS(gba::cpu::mem_access);

#endif //GAMEBOIADVANCE_MEMORY_BUS_INTERFACE_H
