/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/debugger.h>

#include <string_view>
#include <thread>
#include <chrono>

#include <access_private.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <SFML/OpenGL.hpp>

#include <gba/core.h>
#include <gba_debugger/debugger_helpers.h>

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

namespace {

std::string_view to_string_view(const arm::debugger_access_width access_type) noexcept
{
    switch(access_type) {
        case arm::debugger_access_width::byte: return "byte";
        case arm::debugger_access_width::hword: return "hword";
        case arm::debugger_access_width::word: return "word";
        default: UNREACHABLE();
    }
}

} // namespace

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
    LOG_TRACE(debugger, "{} {}", glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    core_->arm.on_instruction_execute.connect<&window::on_instruction_execute>(this);
    core_->arm.on_io_read.connect<&window::on_io_read>(this);
    core_->arm.on_io_write.connect<&window::on_io_write>(this);
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
                case sf::Keyboard::F7: core_->arm.tick(); break;
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
                case sf::Keyboard::F9: tick_allowed_ = !tick_allowed_; break;
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

    const u64 until = core_->schdlr.now() + ppu::engine::cycles_per_frame;
    while (tick_allowed_ && core_->schdlr.now() < until) {
        core_->arm.tick();
    }

    ImGui::SFML::Update(window_, dt_.restart());

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

bool window::on_instruction_execute(const u32 address) noexcept
{
    const bool should_break = last_executed_addr_ != address
      && arm_debugger_.has_enabled_execution_breakpoint(address);

    if(should_break) {
        tick_allowed_ = false;
        LOG_DEBUG(debugger, "execution breakpoint hit: {:08X}", address);
    }

    last_executed_addr_ = address;
    return should_break;
}

void window::on_io_read(const u32 address, const arm::debugger_access_width access_type) noexcept
{
    if(arm_debugger_.has_enabled_read_breakpoint(address, access_type)) {
        tick_allowed_ = false;
        LOG_DEBUG(debugger, "read breakpoint hit: {:08X}, {} access", address, to_string_view(access_type));
    }
}

void window::on_io_write(const u32 address, const u32 data, const arm::debugger_access_width access_type) noexcept
{
    if(arm_debugger_.has_enabled_write_breakpoint(address, data, access_type)) {
        tick_allowed_ = false;
        LOG_DEBUG(debugger, "write breakpoint hit: {:08X} <- {:0X}, {} access",
          address, data, to_string_view(access_type));
    }
}

} // namespace gba::debugger
