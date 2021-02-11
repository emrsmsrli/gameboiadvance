/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_H
#define GAMEBOIADVANCE_PPU_H

#include <gba/core/container.h>
#include <gba/core/event/event.h>
#include <gba/core/fwd.h>
#include <gba/ppu/types.h>

namespace gba::ppu {

class engine {
    friend arm::arm7tdmi;

    scheduler* scheduler_;

    vector<u8> palette_ram_{1_kb};
    vector<u8> vram_{96_kb};
    vector<u8> oam_{1_kb};

    dispcnt dispcnt_;
    dispstat dispstat_;
    u8 vcount_;  // current scanline

    bg_regular bg0_{0_u32};
    bg_regular bg1_{1_u32};
    bg_affine bg2_{2_u32};
    bg_affine bg3_{3_u32};

    window win0_{0_u32};
    window win1_{1_u32};
    win_in win_in_;
    win_out win_out_;

    bool green_swap_ = false; // todo emulate
    mosaic mosaic_;
    bldcnt bldcnt_;
    bldalpha bldalpha_;
    u8 bldy_;

public:
    static constexpr auto screen_width = 240_u8;
    static constexpr auto screen_height = 160_u8;

    static constexpr auto cycles_per_frame = 280'896_u64;

    event<u8, const array<color, screen_width>&> event_on_hblank;
    event<> event_on_vblank;

    engine(scheduler* scheduler) noexcept;

private:
    void on_hblank(u64 cycles_late);
    void on_hdraw(u64 cycles_late);
};

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_PPU_H
