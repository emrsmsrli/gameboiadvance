/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_CORE_H
#define GAMEBOIADVANCE_CORE_H

#include <gba/apu/apu.h>
#include <gba/cartridge/gamepak.h>
#include <gba/core/scheduler.h>
#include <gba/cpu/cpu.h>
#include <gba/keypad.h>
#include <gba/ppu/ppu.h>

namespace gba {

class core : public cpu::bus_interface {
    scheduler scheduler_;
    cartridge::gamepak gamepak_;
    cpu::cpu cpu_;
    ppu::engine ppu_engine_;
    apu::engine apu_engine_;
    keypad::keypad keypad_;

public:
#if WITH_DEBUGGER
    delegate<void(u32, cpu::debugger_access_width)> on_io_read;
    delegate<void(u32, u32, cpu::debugger_access_width)> on_io_write;
#endif // WITH_DEBUGGER

    explicit core(vector<u8> bios)
      : gamepak_{},
        cpu_{std::move(bios), this, &scheduler_},
        ppu_engine_{&scheduler_},
        apu_engine_{&cpu_.timer_controller_[0_u32], &cpu_.timer_controller_[1_u32], &scheduler_}
    {
        ppu_engine_.set_dma_controller_handle(dma::controller_handle{&cpu_.dma_controller_});
        apu_engine_.set_dma_controller_handle(dma::controller_handle{&cpu_.dma_controller_});

        ppu_engine_.set_irq_controller_handle(cpu_.get_interrupt_handle());
        gamepak_.set_irq_controller_handle(cpu_.get_interrupt_handle());
        // sio_engine_.set_irq_controller_handle(arm.get_interrupt_handle());
    }

    [[nodiscard]] event<u8, const ppu::scanline_buffer&>& on_scanline_event() noexcept { return ppu_engine_.event_on_scanline; }
    [[nodiscard]] event<>& on_vblank_event() noexcept { return ppu_engine_.event_on_vblank; }
    [[nodiscard]] event<vector<apu::stereo_sample<float>>>& sound_buffer_overflow_event() noexcept { return apu_engine_.get_buffer_overflow_event(); }

    FORCEINLINE void set_dst_sample_rate(const u32 sample_rate) noexcept { apu_engine_.set_dst_sample_rate(sample_rate); }
    FORCEINLINE void set_sound_buffer_capacity(const usize capacity) noexcept { apu_engine_.set_buffer_capacity(capacity); }

    void tick(u64 cycles = 1_u8) noexcept
    {
        const u64 until = scheduler_.now() + cycles;
        while(scheduler_.now() < until) {
            cpu_.tick();
        }
    }

    void tick_one_frame() noexcept { tick(ppu::engine::cycles_per_frame); }

    void release_key(const keypad::key key) noexcept { keypad_.release(key); }
    void press_key(const keypad::key key) noexcept
    {
        keypad_.press(key);
        if(keypad_.interrupt_available()) {
            cpu_.request_interrupt(cpu::interrupt_source::keypad);
        }
    }

    void load_pak(const fs::path& path)
    {
        gamepak_.load(path);
        gamepak_.set_scheduler(&scheduler_);
    }

    void skip_bios() noexcept { cpu_.skip_bios(); }

private:
    // todo = now - start - cycles_per_frame
    u64 frame_cycle_overshoot;

    [[nodiscard]] u32 read_32(u32 addr, cpu::mem_access access) noexcept final { return read<u32>(addr, access); }
    void write_32(u32 addr, u32 data, cpu::mem_access access) noexcept final { write<u32>(addr, data, access); }
    [[nodiscard]] u16 read_16(u32 addr, cpu::mem_access access) noexcept final { return read<u16>(addr, access); }
    void write_16(u32 addr, u16 data, cpu::mem_access access) noexcept final { write<u16>(addr, data, access); }
    [[nodiscard]] u8 read_8(u32 addr, cpu::mem_access access) noexcept final { return read<u8>(addr, access); }
    void write_8(u32 addr, u8 data, cpu::mem_access access) noexcept final { write<u8>(addr, data, access); }

    [[nodiscard]] u8 read_io(u32 addr, cpu::mem_access access) noexcept;
    void write_io(u32 addr, u8 data) noexcept;

    void idle() noexcept final { tick_components(1_u64); }
    void tick_components(u64 cycles) noexcept final
    {
        // todo break this into pieces that handle pak prefetch system https://mgba.io/2015/06/27/cycle-counting-prefetch/
        if(UNLIKELY(!cpu_.dma_controller_.is_running())) {
            cpu_.dma_controller_.run_channels();
        }

        scheduler_.add_cycles(cycles);
    }

    template<typename T> T read(u32 address, cpu::mem_access access) noexcept;
    template<typename T> void write(u32 address, T data, cpu::mem_access access) noexcept;
};

} // namespace gba

#include <gba/core_bus.inl>

#endif //GAMEBOIADVANCE_CORE_H
