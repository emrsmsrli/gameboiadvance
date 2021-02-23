/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/debugger.h>

#include <thread>
#include <chrono>

#include <access_private.h>
#include <imgui-SFML.h>

#include <gba/core.h>

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r15_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::psr, cpsr_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, wram_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, iwram_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::vector<gba::u8>, pak_data_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, std::unique_ptr<gba::cartridge::backup>, backup_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, palette_ram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, vram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, oam_)

namespace gba::debugger {

window::window(core* core) noexcept
  : window_{sf::VideoMode{1000u, 1000u}, "GBA Debugger"},
    window_event_{},
    core_{core},
    gamepak_debugger_{&core->pak},
    arm_debugger_{&core->arm},
    ppu_debugger_{&core->ppu},
    keypad_debugger_{&core->keypad}
{
    using namespace std::string_view_literals;
    disassembly_view_.add_entry(memory_view_entry{"ROM"sv, &access_private::pak_data_(core->pak), 0x0800'0000_u32});
    disassembly_view_.add_entry(memory_view_entry{"EWRAM"sv, &access_private::wram_(core->arm), 0x0200'0000_u32});
    disassembly_view_.add_entry(memory_view_entry{"IWRAM"sv, &access_private::iwram_(core->arm), 0x0300'0000_u32});

    memory_view_.add_entry(memory_view_entry{"ROM"sv, &access_private::pak_data_(core->pak), 0x0800'0000_u32});
    memory_view_.add_entry(memory_view_entry{"EWRAM"sv, &access_private::wram_(core->arm), 0x0200'0000_u32});
    memory_view_.add_entry(memory_view_entry{"IWRAM"sv, &access_private::iwram_(core->arm), 0x0300'0000_u32});
    memory_view_.add_entry(memory_view_entry{"PALETTE"sv, &access_private::palette_ram_(core->ppu), 0x0500'0000_u32});
    memory_view_.add_entry(memory_view_entry{"VRAM"sv, &access_private::vram_(core->ppu), 0x0600'0000_u32});
    memory_view_.add_entry(memory_view_entry{"OAM"sv, &access_private::oam_(core->ppu), 0x0700'0000_u32});
    switch(core->pak.backup_type()) {
        case cartridge::backup::type::eeprom_4:
        case cartridge::backup::type::eeprom_64:
            memory_view_.add_entry(memory_view_entry{
              "EEPROM"sv,
              &access_private::backup_(core->pak)->data(),
              0x0DFF'FF00_u32
            });
            break;
        case cartridge::backup::type::sram:
            memory_view_.add_entry(memory_view_entry{
              "SRAM"sv,
              &access_private::backup_(core->pak)->data(),
              0x0E00'0000_u32
            });
            break;
        case cartridge::backup::type::flash_64:
        case cartridge::backup::type::flash_128:
            memory_view_.add_entry(memory_view_entry{
              "FLASH"sv,
              &access_private::backup_(core->pak)->data(),
              0x0E00'0000_u32
            });
        case cartridge::backup::type::none:
        case cartridge::backup::type::detect:
            break;
    }

    window_.resetGLStates();
    window_.setVerticalSyncEnabled(true);
    ImGui::SFML::Init(window_);

    [[maybe_unused]] const sf::ContextSettings& settings = window_.getSettings();
    LOG_TRACE(debugger, "OpenGL {}.{}, attr: 0x{:X}", settings.majorVersion, settings.minorVersion, settings.attributeFlags);

    // todo register to gba events
}

bool window::draw() noexcept
{
    while(window_.pollEvent(window_event_)) {
        if(window_event_.type == sf::Event::Closed) { return false; }
        ImGui::SFML::ProcessEvent(window_event_);

        if(window_event_.type == sf::Event::KeyPressed) {
            switch(window_event_.key.code) {
                case sf::Keyboard::W: core_->press_key(keypad::keypad::key::up); break;
                case sf::Keyboard::A: core_->press_key(keypad::keypad::key::left); break;
                case sf::Keyboard::S: core_->press_key(keypad::keypad::key::down); break;
                case sf::Keyboard::D: core_->press_key(keypad::keypad::key::right); break;
                case sf::Keyboard::K: core_->press_key(keypad::keypad::key::b); break;
                case sf::Keyboard::O: core_->press_key(keypad::keypad::key::a); break;
                case sf::Keyboard::B: core_->press_key(keypad::keypad::key::select); break;
                case sf::Keyboard::N: core_->press_key(keypad::keypad::key::start); break;
                case sf::Keyboard::T: core_->press_key(keypad::keypad::key::left_shoulder); break;
                case sf::Keyboard::U: core_->press_key(keypad::keypad::key::right_shoulder); break;
                default:
                    break;
            }
        } else if(window_event_.type == sf::Event::KeyReleased) {
            switch(window_event_.key.code) {
                case sf::Keyboard::W: core_->release_key(keypad::keypad::key::up); break;
                case sf::Keyboard::A: core_->release_key(keypad::keypad::key::left); break;
                case sf::Keyboard::S: core_->release_key(keypad::keypad::key::down); break;
                case sf::Keyboard::D: core_->release_key(keypad::keypad::key::right); break;
                case sf::Keyboard::K: core_->release_key(keypad::keypad::key::b); break;
                case sf::Keyboard::O: core_->release_key(keypad::keypad::key::a); break;
                case sf::Keyboard::B: core_->release_key(keypad::keypad::key::select); break;
                case sf::Keyboard::N: core_->release_key(keypad::keypad::key::start); break;
                case sf::Keyboard::T: core_->release_key(keypad::keypad::key::left_shoulder); break;
                case sf::Keyboard::U: core_->release_key(keypad::keypad::key::right_shoulder); break;
                default:
                    break;
            }
        }
    }

    if(!window_.hasFocus()) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
        return true;
    }

    ImGui::SFML::Update(window_, dt_.restart());

    // todo draw debugger components
    disassembly_view_.draw_with_mode(access_private::cpsr_(core_->arm).t);
    memory_view_.draw();
    gamepak_debugger_.draw();
    arm_debugger_.draw();
    ppu_debugger_.draw();
    keypad_debugger_.draw();

    window_.clear(sf::Color::Black);
    ImGui::SFML::Render();
    window_.display();
    return true;
}

} // namespace gba::debugger
