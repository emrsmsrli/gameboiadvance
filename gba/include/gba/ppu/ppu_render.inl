/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_RENDER_INL
#define GAMEBOIADVANCE_PPU_RENDER_INL

#include "ppu.h"

namespace gba::ppu {

template<typename... BG>
void engine::render_bg_regular(BG& ... bg) noexcept
{
    const auto draw_if_enabled = [&](auto& bg) {
        if(dispcnt_.enable_bg[bg.id]) {
            render_bg_regular_impl(bg);
        }
    };

    (..., draw_if_enabled(bg));
}

template<typename... BG>
void engine::render_bg_affine(BG& ... bg) noexcept
{
    const auto draw_if_enabled = [&](auto& bg) {
        if(dispcnt_.enable_bg[bg.id]) {
            render_bg_affine_impl(bg);
        }
    };

    (..., draw_if_enabled(bg));
}

template<typename... BG>
void engine::compose(BG& ... bg) noexcept
{
    static_vector<u32, 4> ids;
    const auto add_id_if_enabled = [&](auto&& bg) {
        if(dispcnt_.enable_bg[bg.id]) {
            ids.push_back(bg.id);
        }
    };

    (..., add_id_if_enabled(bg));
    compose_impl(ids);
}

template<typename BG>
void engine::render_bg_regular_impl(BG& bg) noexcept
{
    // stub
}

} // namespace gba::ppu

#endif //GAMEBOIADVANCE_PPU_RENDER_INL
