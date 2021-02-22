/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DMA_CONTROLLER_H
#define GAMEBOIADVANCE_DMA_CONTROLLER_H

#include <gba/core/fwd.h>
#include <gba/core/container.h>
#include <gba/core/math.h>

namespace gba::dma {

constexpr auto channel_count_ = 4_u32;

struct data { u32 src; u32 dst; u32 count; };

struct channel : data {
    struct control {
        enum class address_control : u8::type {
            increment,
            decrement,
            fixed,
            inc_reload /* prohibited for src*/
        };

        enum class timing : u8::type {
            immediately,
            vblank,
            hblank,
            special /*DMA0=Prohibited, DMA1/DMA2=Sound FIFO, DMA3=Video Capture*/
        };

        enum class transfer_size : u8::type { hword, word };

        address_control dst_control{address_control::increment};
        address_control src_control{address_control::increment};
        timing when{timing::immediately};
        bool repeat = false;
        transfer_size size{transfer_size::hword}; // 0: 16bit 1: 32bit
        bool drq = false; // - DMA3 only -  (0=Normal, 1=DRQ <from> Game Pak, DMA3)
        bool irq = false;
        bool enabled = false;
    };

    u32 id;

    control cnt;
    data internal;

    arm::mem_access next_access_type;

    channel(const u32 i) noexcept
      : id{i} {}

    void write_dst(const u8 n, const u8 data) noexcept;
    void write_src(const u8 n, const u8 data) noexcept;
    void write_count(const u8 n, const u8 data) noexcept;

    [[nodiscard]] u8 read_cnt_l() const noexcept;
    [[nodiscard]] u8 read_cnt_h() const noexcept;
};

// todo When accessing OAM (7000000h) or OBJ VRAM (6010000h) by HBlank Timing, then the "H-Blank Interval Free" bit in DISPCNT register must be set.
class controller {
public:
    enum class occasion { vblank, hblank, video /*todo implement this in ppu*/, fifo_a, fifo_b };

private:
    arm::arm7tdmi* arm_;

    static_vector<channel*, channel_count_> running_channels_;
    static_vector<channel*, channel_count_> scheduled_channels_;

public:
#if WITH_DEBUGGER
    using channels_debugger = static_vector<channel*, channel_count_>;
#endif // WITH_DEBUGGER

    array<channel, channel_count_> channels{
      channel{0_u32},
      channel{1_u32},
      channel{2_u32},
      channel{3_u32},
    };

    explicit controller(arm::arm7tdmi* arm) noexcept : arm_{arm} {}

    void write_cnt_l(usize idx, u8 data) noexcept;
    void write_cnt_h(usize idx, u8 data) noexcept;

    void run_channels() noexcept;
    void request(occasion occasion) noexcept;

private:
    u32 latch_;

    static void latch(channel& channel, bool for_repeat, bool for_fifo) noexcept;

    void channel_start_internal(const u64 /*late_cycles*/) noexcept;
    void schedule_internal(channel& channel, channel::control::timing timing) noexcept;
};

} // namespace gba::arm

#endif //GAMEBOIADVANCE_DMA_CONTROLLER_H
