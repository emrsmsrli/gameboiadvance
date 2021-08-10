/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_CPU_H
#define GAMEBOIADVANCE_CPU_H

#include <gba/cpu/arm7tdmi.h>
#include <gba/cpu/timer.h>
#include <gba/cpu/dma_controller.h>

namespace gba::cpu {

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

struct waitstate_control {
    u8 sram;
    u8 ws0_nonseq;
    u8 ws0_seq;
    u8 ws1_nonseq;
    u8 ws1_seq;
    u8 ws2_nonseq;
    u8 ws2_seq;
    u8 phi;
    bool prefetch_buffer_enable = false;
};

enum class halt_control {
    halted,
    stopped,
    running,
};


// todo move to somewhere more meaningful
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

class cpu : public arm7tdmi {
    friend core;

    vector<u8> bios_{16_kb};
    vector<u8> wram_{256_kb};
    vector<u8> iwram_{32_kb};

    timer::controller timer_controller_;
    dma::controller dma_controller_;

    /*
     * The BIOS memory is protected against reading,
     * the GBA allows reading opcodes or data only if the program counter is
     * located inside the BIOS area. If the program counter is not in the BIOS area,
     * reading will return the most recent successfully fetched BIOS opcode.
     */
    u32 bios_last_read_;

    u8 post_boot_;

    waitstate_control waitcnt_;
    array<u8, 32> wait_16{ // cycle counts for 16bit r/w, nonseq and seq access
      1_u8, 1_u8, 3_u8, 1_u8, 1_u8, 1_u8, 1_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8,
      1_u8, 1_u8, 3_u8, 1_u8, 1_u8, 1_u8, 1_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8
    };
    array<u8, 32> wait_32{ // cycle counts for 32bit r/w, nonseq and seq access
      1_u8, 1_u8, 6_u8, 1_u8, 1_u8, 2_u8, 2_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8,
      1_u8, 1_u8, 6_u8, 1_u8, 1_u8, 2_u8, 2_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8
    };

    halt_control haltcnt_{halt_control::running};

public:
    cpu(vector<u8> bios, bus_interface* bus, scheduler* scheduler) noexcept
      : arm7tdmi{bus, scheduler},
        bios_{std::move(bios)},
        dma_controller_{bus, get_interrupt_handle(), scheduler},
        timer_controller_{scheduler, get_interrupt_handle()}
    {
        ASSERT(bios_.size() == 16_kb);
        update_waitstate_table();
    }

    void skip_bios() noexcept;

    void tick() noexcept;

private:
    [[nodiscard]] u32 read_bios(u32 addr) noexcept;
    [[nodiscard]] u32 read_unused(u32 addr, mem_access access) noexcept;

    void update_waitstate_table() noexcept;
};

} // namespace gba::cpu

#endif //GAMEBOIADVANCE_CPU_H
