/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_FRONTEND_H
#define GAMEBOIADVANCE_FRONTEND_H

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <sdl2cpp/sdl_audio.h>

#include <gba/core.h>

namespace gba::frontend {

enum class tick_result { exiting, sleeping, ticking };

class window {
    gba::core* core_;

    float current_volume_ = 1.f;
    bool bios_skip_ = false;

    sf::RenderWindow window_;
    sf::Event window_event_;

    sf::Image screen_buffer_;
    sf::Texture screen_texture_;
    uint32_t window_scale_;

    sdl::audio_device audio_device_{2, sdl::audio_device::format::f32, 48000, 2048};

public:
    window(core* core, uint32_t window_scale, float initial_volume, bool bios_skip);

    [[nodiscard]] tick_result tick() noexcept;

private:
    void on_scanline(u8 y, const ppu::scanline_buffer& buffer) noexcept;
    void on_vblank() noexcept;
    void on_audio_buffer_full(const vector<apu::stereo_sample<float>>& buffer) noexcept;

    void modify_volume(const float delta) noexcept;
    [[nodiscard]] fs::path pick_rom() noexcept;
    void load_rom(const fs::path& path) noexcept;

    void update_window_title() noexcept { window_.setTitle(make_window_title()); }
    [[nodiscard]] std::string make_window_title() const noexcept
    {
        return fmt::format("gameboiadvance - {} - vol: {:.1f}",
          core_->pak_loaded() ? core_->game_title() : "no rom", current_volume_);
    }
};

} // namespace gba::frontend

#endif //GAMEBOIADVANCE_FRONTEND_H
