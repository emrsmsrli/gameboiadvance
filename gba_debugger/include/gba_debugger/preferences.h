/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PREFERENCES_H
#define GAMEBOIADVANCE_PREFERENCES_H

#include <gba/core/container.h>

namespace gba::debugger {

struct ppu_bg_preferences {
    bool enable_visible_area = true;
    bool enable_visible_area_border = true;
    int render_scale = 2;
};

struct preferences {
    int ppu_framebuffer_render_scale = 2;

    array<ppu_bg_preferences, 4> ppu_bg_prefs{};
    int ppu_bg_tiles_render_scale = 2;
    int ppu_obj_tiles_render_scale = 2;
    int ppu_win_render_scale = 2;
    array<float, 4> ppu_winout_color{1.f, 1.f, 1.f, 1.f};
    array<float, 4> ppu_win0_color{1.f, 0.f, 0.f, 1.f};
    array<float, 4> ppu_win1_color{0.f, 0.f, 1.f, 1.f};
    array<float, 4> ppu_winobj_color{0.f, 0.f, 0.f, 1.f};

    bool apu_audio_enabled = true;
    float apu_audio_volume = 0.5f;
    unsigned int apu_enabled_channel_graphs = 0b1;

    bool debugger_background_emulate = false;
    bool debugger_bios_skip = false;
    int debugger_framerate = 0;
};

} // namespace gba::debugger

#endif  //GAMEBOIADVANCE_PREFERENCES_H
