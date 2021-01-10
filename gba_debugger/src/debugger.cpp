/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/debugger.h>

#include <thread>
#include <chrono>

#include <imgui-SFML.h>

#include <gba/gba.h>


namespace gba::debugger {

window::window(gba* g) noexcept
  : window_{sf::VideoMode{1000u, 1000u}, "GBA Debugger"},
    window_event_{},
    gamepak_debugger_{&g->pak},
    arm_debugger_{&g->arm}
{
    window_.resetGLStates();
    window_.setVerticalSyncEnabled(true);
    ImGui::SFML::Init(window_);

    [[maybe_unused]] const sf::ContextSettings& settings = window_.getSettings();
    LOG_TRACE("OpenGL {}.{}, attr: 0x{:X}", settings.majorVersion, settings.minorVersion, settings.attributeFlags);

    // todo register to gba events
}

bool window::draw() noexcept
{
    while(window_.pollEvent(window_event_)) {
        if(window_event_.type == sf::Event::Closed) { return false; }
        ImGui::SFML::ProcessEvent(window_event_);
    }

    if(!window_.hasFocus()) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
        return true;
    }

    ImGui::SFML::Update(window_, dt_.restart());

    // todo draw debugger components
    gamepak_debugger_.draw();
    arm_debugger_.draw();

    window_.clear(sf::Color::Black);
    ImGui::SFML::Render();
    window_.display();
    return true;
}

} // namespace gba::debugger
