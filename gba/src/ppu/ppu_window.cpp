/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/ppu/ppu.h>

#include <algorithm>

namespace gba::ppu {

void engine::generate_window_buffer() noexcept
{
    if(dispcnt_.win1_enabled) {
        generate_window_buffer(win1_,  &win_in_.win1);
    }

    if(dispcnt_.win0_enabled) {
        generate_window_buffer(win0_,  &win_in_.win0);
    }
}

void engine::generate_window_buffer(window& win, win_enable_bits* enable_bits) noexcept
{
    bool& can_draw_win = win_can_draw_flags_[win.id];
    if(vcount_ == win.top_left.y) {
        can_draw_win = true;
    }
    if(vcount_ == win.bottom_right.y) {
        can_draw_win = false;
    }

    if(!can_draw_win) {
        return;
    }

    if(win.top_left.x <= win.bottom_right.x) {
        if(win.top_left.x < screen_width) {
            const auto win_start = win_buffer_.begin() + win.top_left.x.get();
            const auto win_end = win_buffer_.begin() + std::min<u32>(screen_width, win.bottom_right.x).get();
            std::fill(win_start, win_end, enable_bits);
        }
    } else {
        if(win.bottom_right.x < screen_width) {
            std::fill(win_buffer_.begin(), win_buffer_.begin() + win.bottom_right.x.get(), enable_bits);
        }
        if(win.top_left.x < screen_width) {
            std::fill(win_buffer_.begin() + win.top_left.x.get(), win_buffer_.end(), enable_bits);
        }
    }
}

} // namespace gba::ppu
