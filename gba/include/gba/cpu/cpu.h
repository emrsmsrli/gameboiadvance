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

struct prefetch_buffer {
    static inline constexpr u32 capacity_in_bytes = 16_u32;

    u32 begin;
    u32 end;
    u32 size;
    u32 capacity;
    i32 cycles_left;
    i32 cycles_needed;
    u32 addr_increment;
    bool active = false;

    [[nodiscard]] FORCEINLINE bool empty() const noexcept { return size == 0_u32; }
    [[nodiscard]] FORCEINLINE bool full() const noexcept { return size == capacity; }
};

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
    prefetch_buffer prefetch_buffer_;

    using stall_table_entry = array<u8, 16>;
    array<stall_table_entry, 2> stall_16_{
      stall_table_entry{1_u8, 1_u8, 3_u8, 1_u8, 1_u8, 1_u8, 1_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8},
      stall_table_entry{1_u8, 1_u8, 3_u8, 1_u8, 1_u8, 1_u8, 1_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8}
    };
    array<stall_table_entry, 2> stall_32_{
      stall_table_entry{1_u8, 1_u8, 6_u8, 1_u8, 1_u8, 2_u8, 2_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8},
      stall_table_entry{1_u8, 1_u8, 6_u8, 1_u8, 1_u8, 2_u8, 2_u8, 1_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 0_u8, 1_u8}
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
    template<typename T>
    [[nodiscard]] FORCEINLINE u8& stall_cycles(const mem_access access, const memory_page page) noexcept
    {
        if(UNLIKELY(page > memory_page::pak_sram_2)) {
            static u8 unused_area_cycles = 1_u8;
            return unused_area_cycles;
        }

        ASSERT(access != mem_access::none);
        if constexpr(std::is_same_v<T, u32>) {
            return stall_32_[from_enum<u32>(access)][from_enum<u32>(page)];
        } else {
            static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u8>);
            return stall_16_[from_enum<u32>(access)][from_enum<u32>(page)];
        }
    }

    [[nodiscard]] u32 read_bios(u32 addr) noexcept;
    [[nodiscard]] u32 read_unused(u32 addr) noexcept;

    void prefetch(u32 addr, u32 cycles) noexcept;
    void prefetch_tick(u32 cycles) noexcept;
    void update_waitstate_table() noexcept;
};

} // namespace gba::cpu

#endif //GAMEBOIADVANCE_CPU_H
