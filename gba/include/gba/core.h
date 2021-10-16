/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_CORE_H
#define GAMEBOIADVANCE_CORE_H

#include <gba/archive.h>
#include <gba/apu/apu.h>
#include <gba/cartridge/gamepak.h>
#include <gba/core/scheduler.h>
#include <gba/cpu/cpu.h>
#include <gba/helper/gzip.h>
#include <gba/keypad.h>
#include <gba/ppu/ppu.h>

namespace gba {

enum class state_slot : u32::type { slot1 = 1, slot2, slot3, slot4, slot5, max };

class core : public cpu::bus_interface {
    scheduler scheduler_;
    cartridge::gamepak gamepak_;
    cpu::cpu cpu_;
    ppu::engine ppu_engine_;
    apu::engine apu_engine_;
    keypad::keypad keypad_;

    archive default_state_;
    fs::path states_path_;

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

    FORCEINLINE void set_volume(const float volume) noexcept { apu_engine_.set_volume(volume); }
    FORCEINLINE void set_dst_sample_rate(const u32 sample_rate) noexcept { apu_engine_.set_dst_sample_rate(sample_rate); }
    FORCEINLINE void set_sound_buffer_capacity(const usize capacity) noexcept { apu_engine_.set_buffer_capacity(capacity); }

    void tick(u32 cycles = 1_u32) noexcept
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

    [[nodiscard]] bool pak_loaded() const noexcept { return gamepak_.loaded(); }
    void load_pak(const fs::path& path)
    {
        gamepak_.load(path);

        if(pak_loaded()) {
            gamepak_.set_scheduler(&scheduler_);

            states_path_ = path.parent_path() / "states" / path.filename();
            states_path_.replace_extension();

            if(!fs::exists(states_path_) && !fs::create_directories(states_path_)) {
                LOG_WARN(core, "states path could not be created {}", states_path_.string());
            }

            save_default_state();
        }
    }

    void skip_bios() noexcept { cpu_.skip_bios(); }

    void reset(const bool should_skip_bios = false) noexcept
    {
        if(pak_loaded() && !default_state_.empty()) {
            default_state_.seek_to_start();
            deserialize(default_state_);
            if(should_skip_bios) {
                skip_bios();
            }
        }
    }

    void save_state(const state_slot slot) const noexcept
    {
        if(!pak_loaded() || !fs::exists(states_path_)) {
            return;
        }

        ASSERT(slot < state_slot::max);
        archive archive;
        serialize(archive);

        const fs::path slot_path = get_slot_path(slot);
        const std::optional<vector<u8>> compressed_archive = gzip::compress(archive.data());
        if(compressed_archive.has_value()) {
            fs::write_file(slot_path, compressed_archive.value());
            LOG_INFO(core, "state saved to slot {}", from_enum<u32>(slot));
        } else {
            LOG_WARN(core, "error compressing archive");
        }
    }

    void load_state(const state_slot slot) noexcept
    {
        if(!pak_loaded() || !fs::exists(states_path_)) {
            return;
        }

        ASSERT(slot < state_slot::max);
        const fs::path slot_path = get_slot_path(slot);

        if(!fs::exists(slot_path)) {
            LOG_WARN(core, "no saved state found in slot {}", from_enum<u32>(slot));
            return;
        }

        const std::optional<vector<u8>> decompressed_archive = gzip::decompress(fs::read_file(slot_path));
        if(decompressed_archive.has_value()) {
            const archive archive{decompressed_archive.value()};
            deserialize(archive);
            LOG_INFO(core, "state loaded from slot {}", from_enum<u32>(slot));
        } else {
            LOG_WARN(core, "error decompressing archive");
        }
    }

private:
    [[nodiscard]] fs::path get_slot_path(const state_slot slot) const noexcept
    {
        return states_path_ / fmt::format("slot{}.bin", from_enum<u32>(slot));
    }

    void save_default_state() noexcept
    {
        default_state_.clear();
        serialize(default_state_);
    }

    void serialize(archive& archive) const noexcept
    {
        scheduler_.serialize(archive);
        gamepak_.serialize(archive);
        cpu_.serialize(archive);
        ppu_engine_.serialize(archive);
        apu_engine_.serialize(archive);
        // sio_engine_.serialize(archive);
        keypad_.serialize(archive);
    }

    void deserialize(const archive& archive) noexcept
    {
        scheduler_.deserialize(archive);
        gamepak_.deserialize(archive);
        cpu_.deserialize(archive);
        ppu_engine_.deserialize(archive);
        apu_engine_.deserialize(archive);
        // sio_engine_.deserialize(archive);
        keypad_.deserialize(archive);
    }

    [[nodiscard]] u32 read_32(u32 addr, cpu::mem_access access) noexcept final { return read<u32>(addr, access); }
    void write_32(u32 addr, u32 data, cpu::mem_access access) noexcept final { write<u32>(addr, data, access); }
    [[nodiscard]] u16 read_16(u32 addr, cpu::mem_access access) noexcept final { return read<u16>(addr, access); }
    void write_16(u32 addr, u16 data, cpu::mem_access access) noexcept final { write<u16>(addr, data, access); }
    [[nodiscard]] u8 read_8(u32 addr, cpu::mem_access access) noexcept final { return read<u8>(addr, access); }
    void write_8(u32 addr, u8 data, cpu::mem_access access) noexcept final { write<u8>(addr, data, access); }

    [[nodiscard]] u8 read_io(u32 addr) noexcept;
    void write_io(u32 addr, u8 data) noexcept;

    void idle() noexcept final { tick_components(1_u32); }
    void tick_components(const u32 cycles) noexcept final
    {
        if(UNLIKELY(!cpu_.dma_controller_.is_running() && cpu_.dma_controller_.should_start_running())) {
            cpu_.dma_controller_.run_channels();
        }

        scheduler_.add_cycles(cycles);
        cpu_.prefetch_tick(cycles);
    }

    template<typename T> T read(u32 address, cpu::mem_access access) noexcept;
    template<typename T> void write(u32 address, T data, cpu::mem_access access) noexcept;
};

} // namespace gba

#include <gba/core_bus.inl>

#endif //GAMEBOIADVANCE_CORE_H
