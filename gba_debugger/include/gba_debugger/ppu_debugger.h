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

enum class win_type { w0, w1, obj, out };

struct ppu_debugger {
    ppu::engine* ppu_engine;

    sf::Image screen_buffer_;
    sf::Texture screen_texture_;

    sf::Image tiles_buffer_;
    sf::Texture tiles_texture_;

    array<sf::Image, 4> bg_buffers_;
    array<sf::Texture, 4> bg_textures_;

    array<sf::Image, 128> obj_buffers_;
    array<sf::Texture, 128> obj_textures_;

    vector<win_type> win_types_;
    sf::Image win_buffer_;
    sf::Texture win_texture_;

    explicit ppu_debugger(ppu::engine* engine);
    void draw() noexcept;

    void on_scanline(u8 screen_y, const ppu::scanline_buffer& scanline) noexcept;
    void on_update_texture() noexcept;

    void draw_regular_bg_map(const ppu::bg_regular& bg) noexcept;
    void draw_affine_bg_map(const ppu::bg_affine& bg) noexcept;
    void draw_bitmap_bg(const ppu::bg_affine& bg, u32 mode) noexcept;
    void draw_bg_tiles() noexcept;
    void draw_obj_tiles() noexcept;
    void draw_obj() noexcept;
    void draw_win_buffer() noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_PPU_DEBUGGER_H
