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
    gba_{g},
    gamepak_debugger_{&g->pak},
    arm_debugger_{&g->arm}
{
    using namespace std::string_view_literals;
    disassembly_view_.add_entry(memory_view_entry{"ROM"sv, &g->pak.pak_data(), 0x0800'0000_u32});
    disassembly_view_.add_entry(memory_view_entry{"EWRAM"sv, &access_private::wram_(g->arm), 0x0200'0000_u32});
    disassembly_view_.add_entry(memory_view_entry{"IWRAM"sv, &access_private::iwram_(g->arm), 0x0300'0000_u32});

    memory_view_.add_entry(memory_view_entry{"ROM"sv, &g->pak.pak_data(), 0x0800'0000_u32});
    memory_view_.add_entry(memory_view_entry{"EWRAM"sv, &access_private::wram_(g->arm), 0x0200'0000_u32});
    memory_view_.add_entry(memory_view_entry{"IWRAM"sv, &access_private::iwram_(g->arm), 0x0300'0000_u32});
    // memory_view_.add_entry(memory_view_entry{"PALETTE"sv, &todo, 0x0500'0000_u32});
    // memory_view_.add_entry(memory_view_entry{"VRAM"sv, &todo, 0x0600'0000_u32});
    // memory_view_.add_entry(memory_view_entry{"OAM"sv, &todo, 0x0700'0000_u32});
    switch(g->pak.backup_type()) {
        case backup::type::eeprom_4:
        case backup::type::eeprom_64:
            memory_view_.add_entry(memory_view_entry{
              "EEPROM"sv,
              &access_private::backup_(g->pak)->data(),
              0x0DFF'FF00_u32
            });
            break;
        case backup::type::sram:
            memory_view_.add_entry(memory_view_entry{
              "SRAM"sv,
              &access_private::backup_(g->pak)->data(),
              0x0E00'0000_u32
            });
            break;
        case backup::type::flash_64:
        case backup::type::flash_128:
            memory_view_.add_entry(memory_view_entry{
              "FLASH"sv,
              &access_private::backup_(g->pak)->data(),
              0x0E00'0000_u32
            });
        case backup::type::none:
        case backup::type::detect:
            break;
    }

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
    disassembly_view_.draw_with_mode(access_private::cpsr_(gba_->arm).t);
    memory_view_.draw();
    gamepak_debugger_.draw();
    arm_debugger_.draw();

    window_.clear(sf::Color::Black);
    ImGui::SFML::Render();
    window_.display();
    return true;
}

} // namespace gba::debugger
