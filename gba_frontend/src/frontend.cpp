/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <frontend/frontend.h>

#include <portable-file-dialogs.h>

namespace gba::frontend {

window::window(core* core, const uint32_t window_scale, bool bios_skip)
  : core_{core},
    window_{
      sf::VideoMode{ppu::screen_width * window_scale, ppu::screen_height * window_scale},
      "gameboiadvance",
      sf::Style::Close | sf::Style::Resize
    },
    window_scale_{window_scale},
    bios_skip_{bios_skip}
{
    audio_device_.resume();

    screen_buffer_.create(ppu::screen_width, ppu::screen_height);
    screen_texture_.create(ppu::screen_width, ppu::screen_height);

    core->on_scanline_event().add_delegate({connect_arg<&window::on_scanline>, this});
    core->on_vblank_event().add_delegate({connect_arg<&window::on_vblank>, this});
    core->sound_buffer_overflow_event().add_delegate({connect_arg<&window::on_audio_buffer_full>, this});

    core->set_dst_sample_rate(audio_device_.frequency());
    core->set_sound_buffer_capacity(audio_device_.sample_count());
    core->set_volume(current_volume_);

    if(bios_skip) {
        core->skip_bios();
    }

    if(!core_->pak_loaded()) {
        constexpr bool no_cancel = true;
        const fs::path result = pick_rom(no_cancel);
        load_rom(result);
    }
}

void window::on_scanline(const u8 y, const ppu::scanline_buffer& buffer) noexcept
{
    for(u32 x : range(ppu::screen_width)) {
        screen_buffer_.setPixel(x.get(), y.get(), sf::Color{buffer[x].to_u32().get()});
    }
}

void window::on_vblank() noexcept
{
    screen_texture_.update(screen_buffer_);

    constexpr float frame_scale = 2.f;
    sf::Sprite frame{screen_texture_};
    frame.setScale(frame_scale, frame_scale);

    window_.clear(sf::Color::Black);
    window_.draw(frame);
    window_.display();
}

tick_result window::tick() noexcept
{
    const auto save_load_state = [&](const state_slot slot) {
        if(window_event_.key.control) {
            core_->load_state(slot);
        } else {
            core_->save_state(slot);
        }
    };

    while(window_.pollEvent(window_event_)) {
        if(window_event_.type == sf::Event::Closed) { return tick_result::exiting; }

        if(window_event_.type == sf::Event::KeyPressed) {
            switch(window_event_.key.code) {
                case sf::Keyboard::W: core_->press_key(gba::keypad::key::up); break;
                case sf::Keyboard::A: core_->press_key(gba::keypad::key::left); break;
                case sf::Keyboard::S: core_->press_key(gba::keypad::key::down); break;
                case sf::Keyboard::D: core_->press_key(gba::keypad::key::right); break;
                case sf::Keyboard::K: core_->press_key(gba::keypad::key::b); break;
                case sf::Keyboard::O: core_->press_key(gba::keypad::key::a); break;
                case sf::Keyboard::B: core_->press_key(gba::keypad::key::select); break;
                case sf::Keyboard::N: core_->press_key(gba::keypad::key::start); break;
                case sf::Keyboard::T: core_->press_key(gba::keypad::key::left_shoulder); break;
                case sf::Keyboard::U: core_->press_key(gba::keypad::key::right_shoulder); break;
                case sf::Keyboard::Add: modify_volume(0.1f); break;
                case sf::Keyboard::Subtract: modify_volume(-0.1f); break;
                default:
                    break;
            }
        } else if(window_event_.type == sf::Event::KeyReleased) {
            switch(window_event_.key.code) {
                case sf::Keyboard::W: core_->release_key(gba::keypad::key::up); break;
                case sf::Keyboard::A: core_->release_key(gba::keypad::key::left); break;
                case sf::Keyboard::S: core_->release_key(gba::keypad::key::down); break;
                case sf::Keyboard::D: core_->release_key(gba::keypad::key::right); break;
                case sf::Keyboard::K: core_->release_key(gba::keypad::key::b); break;
                case sf::Keyboard::O: core_->release_key(gba::keypad::key::a); break;
                case sf::Keyboard::B: core_->release_key(gba::keypad::key::select); break;
                case sf::Keyboard::N: core_->release_key(gba::keypad::key::start); break;
                case sf::Keyboard::T: core_->release_key(gba::keypad::key::left_shoulder); break;
                case sf::Keyboard::U: core_->release_key(gba::keypad::key::right_shoulder); break;
                case sf::Keyboard::R: core_->reset(bios_skip_); break;
                case sf::Keyboard::F1: save_load_state(state_slot::slot1); break;
                case sf::Keyboard::F2: save_load_state(state_slot::slot2); break;
                case sf::Keyboard::F3: save_load_state(state_slot::slot3); break;
                case sf::Keyboard::F4: save_load_state(state_slot::slot4); break;
                case sf::Keyboard::F5: save_load_state(state_slot::slot5); break;
                case sf::Keyboard::Tab: {
                    if(window_event_.key.control) {
                        constexpr bool no_cancel = false;
                        const fs::path result = pick_rom(no_cancel);
                        if(!result.empty()) {
                            load_rom(result);
                        }
                    }
                }
                default:
                    break;
            }
        } else if(window_event_.type == sf::Event::Resized) {
            const auto height_scale = window_event_.size.height / gba::ppu::screen_height;
            const auto width_scale = window_event_.size.width / gba::ppu::screen_width;
            window_scale_ = std::max(std::max(height_scale, width_scale), 1_u32);
            window_.setSize({window_scale_ * gba::ppu::screen_width, window_scale_ * gba::ppu::screen_height});
            LOG_INFO(frontend, "window scale: {}", window_scale_);
        }
    }

    if(!window_.hasFocus()) {
        return tick_result::sleeping;
    }

    core_->tick_one_frame();
    return tick_result::ticking;
}

void window::on_audio_buffer_full(const vector<gba::apu::stereo_sample<float>>& buffer) noexcept
{
    const gba::usize buffer_size_in_bytes = sizeof(gba::apu::stereo_sample<float>) * buffer.size();
    audio_device_.enqueue(reinterpret_cast<const void*>(buffer.data()), buffer_size_in_bytes.get());
    while(audio_device_.queue_size() > buffer_size_in_bytes) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }
}

void window::modify_volume(const float delta) noexcept
{
    current_volume_ = std::clamp(current_volume_ + delta, 0.f, 1.f);
    core_->set_volume(current_volume_);
}

fs::path window::pick_rom(const bool no_cancel) noexcept
{
    fs::path rom_path;
    do {
        const auto result = pfd::open_file("Pick ROM",
          core_->pak_path().string(),
          {"GBA ROMs", "*.gba", "GZip GBA ROMs", "*.gz", "All GBA ROMs", "*.gba *.gz"},
          pfd::opt::none).result();
        if(!result.empty()) {
            rom_path = result.front();
        }
    } while(no_cancel && rom_path.empty());
    return rom_path;
}

void window::load_rom(const fs::path& path) noexcept
{
    core_->reset(bios_skip_);
    core_->load_pak(path);
    window_.setTitle(fmt::format("gameboiadvance - {}", core_->game_title()));
}

} // namespace gba::frontend
