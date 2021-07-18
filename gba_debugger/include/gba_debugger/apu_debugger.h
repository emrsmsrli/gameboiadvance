/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_APU_DEBUGGER_H
#define GAMEBOIADVANCE_APU_DEBUGGER_H

#include <imgui_memory_editor/imgui_memory_editor.h>

#include <gba/core/fwd.h>
#include <gba/core/container.h>
#include <gba/apu/apu_types.h>

namespace gba::debugger {

struct preferences;

class apu_debugger {
    MemoryEditor ram_viewer_;
    preferences* prefs_;
    apu::engine* apu_engine_;

    vector<apu::stereo_sample<float>> sound_buffer_1_;
    vector<apu::stereo_sample<float>> sound_buffer_2_;
    vector<apu::stereo_sample<float>> sound_buffer_3_;
    vector<apu::stereo_sample<float>> sound_buffer_4_;
    vector<apu::stereo_sample<float>> sound_buffer_fifo_a_;
    vector<apu::stereo_sample<float>> sound_buffer_fifo_b_;

public:
    apu_debugger(apu::engine* apu_engine, preferences* prefs) noexcept;
    void draw() noexcept;

    FORCEINLINE void set_buffer_capacity(const usize capacity) noexcept
    {
        sound_buffer_1_.resize(capacity);
        sound_buffer_2_.resize(capacity);
        sound_buffer_3_.resize(capacity);
        sound_buffer_4_.resize(capacity);
        sound_buffer_fifo_a_.resize(capacity);
        sound_buffer_fifo_b_.resize(capacity);
    }

    void on_sample_written(usize idx) noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_APU_DEBUGGER_H
