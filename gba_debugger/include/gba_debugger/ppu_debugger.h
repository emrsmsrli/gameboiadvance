/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_DEBUGGER_H
#define GAMEBOIADVANCE_PPU_DEBUGGER_H

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <gba/ppu/ppu.h>

namespace gba::debugger {

struct ppu_debugger {
    ppu::engine* ppu_engine;

    sf::Image screen_buffer;
    sf::Texture screen_texture;

    explicit ppu_debugger(ppu::engine* engine);
    void draw() const noexcept;

private:
    void on_scanline(u8 screen_y, const array<ppu::color, ppu::engine::screen_width>& scanline) noexcept;
    void on_update_texture() noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_PPU_DEBUGGER_H
