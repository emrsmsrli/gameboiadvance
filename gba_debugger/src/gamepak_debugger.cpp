/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/gamepak_debugger.h>

#include <imgui.h>
#include <access_private.h>

#include <gba/cartridge/gamepak.h>
#include <gba_debugger/debugger_helpers.h>

ACCESS_PRIVATE_FIELD(gba::gamepak, gba::fs::path, path_)
ACCESS_PRIVATE_FIELD(gba::gamepak, gba::backup::type, backup_type_)
ACCESS_PRIVATE_FIELD(gba::gamepak, std::string_view, game_title_)
ACCESS_PRIVATE_FIELD(gba::gamepak, std::string_view, game_code_)
ACCESS_PRIVATE_FIELD(gba::gamepak, std::string_view, maker_code_)
ACCESS_PRIVATE_FIELD(gba::gamepak, gba::u8, main_unit_code_)
ACCESS_PRIVATE_FIELD(gba::gamepak, gba::u8, software_version_)
ACCESS_PRIVATE_FIELD(gba::gamepak, gba::u8, checksum_)
ACCESS_PRIVATE_FIELD(gba::gamepak, bool, loaded_)
ACCESS_PRIVATE_FIELD(gba::gamepak, bool, has_rtc_)
ACCESS_PRIVATE_FIELD(gba::gamepak, bool, has_mirroring_)

namespace gba::debugger {

void debugger::gamepak_debugger::draw() const noexcept
{
    if(ImGui::Begin("GamePak")) {
        ImGui::Text("title: %s", access_private::game_title_(pak).data());
        ImGui::Text("game code: %s", access_private::game_code_(pak).data());
        ImGui::Text("maker code: %s", access_private::maker_code_(pak).data());

        ImGui::Spacing();
        ImGui::Text("backup: %s", [&]() {
            switch(access_private::backup_type_(pak)) {
                case backup::type::detect: return "detect";
                case backup::type::none: return "none";
                case backup::type::eeprom_4: return "eeprom_4";
                case backup::type::eeprom_64: return "eeprom_64";
                case backup::type::sram: return "sram";
                case backup::type::flash_64: return "flash_64";
                case backup::type::flash_128: return "flash_128";
            }
        }());

        ImGui::Text("main unit code: %02X", access_private::main_unit_code_(pak).get());
        ImGui::Text("software version: %02X", access_private::software_version_(pak).get());
        ImGui::Text("checksum: %02X", access_private::checksum_(pak).get());

        ImGui::Spacing();
        ImGui::Text("loaded: %s", fmt_bool(access_private::loaded_(pak)));
        ImGui::Text("has rtc: %s", fmt_bool(access_private::has_rtc_(pak)));
        ImGui::Text("has mirroring: %s", fmt_bool(access_private::has_mirroring_(pak)));

        ImGui::Spacing();
        ImGui::Text("path: %ls", access_private::path_(pak).c_str());
    }

    ImGui::End();
}

} // namespace gba::debugger
