/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/ppu_debugger.h>

#include <SFML/Graphics/Sprite.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

namespace gba::debugger {

ppu_debugger::ppu_debugger(ppu::engine* engine)
  : ppu_engine{engine}
{
    screen_buffer.create(ppu::engine::screen_width, ppu::engine::screen_height);
    screen_texture.create(ppu::engine::screen_width, ppu::engine::screen_height);
    engine->event_on_hblank.add_delegate({connect_arg<&ppu_debugger::on_scanline>, this});
    engine->event_on_vblank.add_delegate({connect_arg<&ppu_debugger::on_update_texture>, this});
}

void ppu_debugger::draw() const noexcept
{
    if(ImGui::Begin("PPU")) {
        sf::Sprite screen{screen_texture};
        screen.setScale(2.f, 2.f);
        ImGui::Image(screen);
    }

    ImGui::End();
}

void ppu_debugger::on_scanline(u8 screen_y, const array<ppu::color, ppu::engine::screen_width>& scanline) noexcept
{
    for(u32 x = 0_u32; x < ppu::engine::screen_width; ++x) {
        screen_buffer.setPixel(x.get(), screen_y.get(), sf::Color{scanline[x].to_u32().get()});
    }
}

void ppu_debugger::on_update_texture() noexcept
{
    screen_texture.update(screen_buffer);
}

} // namespace gba::debugger
