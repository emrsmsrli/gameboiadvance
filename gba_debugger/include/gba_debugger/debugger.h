/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DEBUGGER_H
#define GAMEBOIADVANCE_DEBUGGER_H

#include <sdl2cpp/sdl_audio.h>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#include <gba/apu/apu_types.h>
#include <gba/core/fwd.h>
#include <gba_debugger/apu_debugger.h>
#include <gba_debugger/breakpoint_database.h>
#include <gba_debugger/cpu_debugger.h>
#include <gba_debugger/gamepak_debugger.h>
#include <gba_debugger/keypad_debugger.h>
#include <gba_debugger/memory_debugger.h>
#include <gba_debugger/ppu_debugger.h>
#include <gba_debugger/preferences.h>

namespace gba::debugger {

class window {
    preferences prefs_;

    sf::RenderWindow window_;
    sf::Event window_event_;
    sf::Clock dt_;

    vector<float> frame_time_history_{128_usize};
    sf::Clock frame_dt_;
    usize total_instructions_;
    usize total_frames_;
    float total_frame_time_ = 0.f;

    sdl::audio_device audio_device_;

    core* core_;
    cpu::cpu* cpu_;
    scheduler* scheduler_;
    breakpoint_database breakpoint_database_;
    disassembly_view disassembly_view_;
    memory_view memory_view_;
    gamepak_debugger gamepak_debugger_;
    cpu_debugger cpu_debugger_;
    ppu_debugger ppu_debugger_;
    apu_debugger apu_debugger_;
    keypad_debugger keypad_debugger_;

    bool tick_allowed_ = false;
    u32 last_executed_addr_;
    cpu_debugger::execution_request execution_request_{cpu_debugger::execution_request::none};

public:
    explicit window(core* core) noexcept;

    [[nodiscard]] bool draw() noexcept;

    bool on_instruction_execute(u32 address) noexcept;
    void on_io_read(u32 address, cpu::debugger_access_width access_type) noexcept;
    void on_io_write(u32 address, u32 data, cpu::debugger_access_width access_type) noexcept;

private:
    void on_execution_requested(cpu_debugger::execution_request type) noexcept;
    void on_scanline(u8, const ppu::scanline_buffer&) noexcept;
    void on_vblank() noexcept;
    void on_audio_buffer_full(const vector<apu::stereo_sample<float>>& buffer) noexcept;
    void on_eeprom_bus_width_detected() noexcept;

    void generate_memory_debugger_entries() noexcept;
    void reset_core() noexcept;

    enum class framerate_limit { unlimited, _30, _60, _120, _144, vsync };

    void set_framerate_limit(framerate_limit limit) noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DEBUGGER_H
