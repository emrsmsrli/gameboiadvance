/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DMA_CONTROLLER_H
#define GAMEBOIADVANCE_DMA_CONTROLLER_H

#include <gba/core/container.h>
#include <gba/core/fwd.h>
#include <gba/core/math.h>
#include <gba/core/scheduler.h>
#include <gba/cpu/irq_controller_handle.h>

namespace gba::dma {

constexpr auto channel_count = 4_u32;

enum class occasion : u32::type { vblank, hblank, video, fifo_a, fifo_b };

struct data { u32 src; u32 dst; u32 count; };

struct channel : data {
    struct control {
        enum class address_control : u8::type {
            increment,
            decrement,
            fixed,
            inc_reload
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
        transfer_size size{transfer_size::hword};
        bool drq = false;
        bool irq = false;
        bool enabled = false;
    };

    u32 id;

    scheduler::hw_event::handle last_event_handle;

    control cnt;
    data internal;
    u32 latch;

    cpu::mem_access next_access_type;

    explicit channel(const u32 i) noexcept
      : id{i} {}

    void write_dst(u8 n, u8 data) noexcept;
    void write_src(u8 n, u8 data) noexcept;
    void write_count(u8 n, u8 data) noexcept;

    [[nodiscard]] u8 read_cnt_l() const noexcept;
    [[nodiscard]] u8 read_cnt_h() const noexcept;
};

class controller {
    cpu::bus_interface* bus_;
    cpu::irq_controller_handle irq_;
    scheduler* scheduler_;

    static_vector<channel*, channel_count> running_channels_;
    static_vector<channel*, channel_count> scheduled_channels_;

    array<channel, channel_count> channels_{
      channel{0_u32},
      channel{1_u32},
      channel{2_u32},
      channel{3_u32},
    };

public:
    controller(cpu::bus_interface* bus, cpu::irq_controller_handle irq, scheduler* scheduler) noexcept
      : bus_{bus}, irq_{irq}, scheduler_{scheduler} {}

    void write_cnt_l(usize idx, u8 data) noexcept;
    void write_cnt_h(usize idx, u8 data) noexcept;

    [[nodiscard]] FORCEINLINE bool is_running() const noexcept { return is_running_; }
    [[nodiscard]] FORCEINLINE bool should_start_running() const noexcept { return !running_channels_.empty(); }
    void run_channels() noexcept;
    void request(occasion occasion) noexcept;

    [[nodiscard]] u32 latch() const noexcept { return latch_; }

    channel& operator[](const usize idx) noexcept { return channels_[idx]; }
    const channel& operator[](const usize idx) const noexcept { return channels_[idx]; }

private:
    u32 latch_;
    bool is_running_ = false;

    static void latch(channel& channel, bool for_repeat, bool for_fifo) noexcept;

    void on_channel_start(u64 /*late_cycles*/) noexcept;
    void schedule(channel& channel, channel::control::timing timing) noexcept;
};

class controller_handle {
    controller* controller_;

public:
    controller_handle() = default;
    explicit controller_handle(controller* controller) noexcept : controller_{controller} {}
    void request_dma(const occasion dma_occasion) noexcept { controller_->request(dma_occasion); }
    void disable_video_transfer() noexcept
    {
        auto& channel = (*controller_)[3_usize];
        if(channel.cnt.enabled && channel.cnt.when == channel::control::timing::special) {
            channel.cnt.enabled = false;
        }
    }
};

} // namespace gba::dma

#endif //GAMEBOIADVANCE_DMA_CONTROLLER_H
