/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/gamepak_debugger.h>

#include <sstream>

#include <imgui.h>
#include <access_private.h>

#include <gba/cartridge/gamepak.h>
#include <gba_debugger/debugger_helpers.h>

ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::fs::path, path_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::cartridge::rtc, rtc_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, std::unique_ptr<gba::cartridge::backup>, backup_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::cartridge::backup::type, backup_type_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, std::string_view, game_title_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, std::string_view, game_code_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, std::string_view, maker_code_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::u8, main_unit_code_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::u8, software_version_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::u8, checksum_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, bool, loaded_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, bool, has_rtc_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, bool, has_mirroring_)

ACCESS_PRIVATE_FIELD(gba::cartridge::backup_eeprom, gba::u64, buffer_)
ACCESS_PRIVATE_FIELD(gba::cartridge::backup_eeprom, gba::u8, bus_width_)
ACCESS_PRIVATE_FIELD(gba::cartridge::backup_eeprom, gba::u8, transmission_count_)
ACCESS_PRIVATE_FIELD(gba::cartridge::backup_eeprom, bool, read_mode_)
ACCESS_PRIVATE_FIELD(gba::cartridge::backup_eeprom, gba::cartridge::backup_eeprom::state_debugger, state_)

ACCESS_PRIVATE_FIELD(gba::cartridge::backup_flash, gba::u64, current_bank_)
ACCESS_PRIVATE_FIELD(gba::cartridge::backup_flash, gba::cartridge::backup_flash::device_id_debugger, device_id_)
ACCESS_PRIVATE_FIELD(gba::cartridge::backup_flash, gba::cartridge::backup_flash::state_debugger, state_)
ACCESS_PRIVATE_FIELD(gba::cartridge::backup_flash, gba::cartridge::backup_flash::cmd_debugger, current_cmds_)

ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::u8, pin_states_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::u8, directions_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::u8, remaining_bytes_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::u8, bits_read_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::u8, bit_buffer_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, bool, read_allowed_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::cartridge::rtc::transfer_state_debugger, transfer_state_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::cartridge::rtc::time_regs_debugger, time_regs_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::cartridge::rtc_command, current_cmd_)
ACCESS_PRIVATE_FIELD(gba::cartridge::rtc, gba::u8, control_) // 24h mode bit 6

namespace gba::debugger {

void gamepak_debugger::draw() const noexcept
{
    if(ImGui::Begin("GamePak")) {
        if(ImGui::BeginTabBar("#gamepak_tab")) {
            const cartridge::backup::type backup_type = access_private::backup_type_(pak);
            const auto backup_str = [&]() {
                switch(backup_type) {
                    case cartridge::backup::type::detect: return "detect";
                    case cartridge::backup::type::none: return "none";
                    case cartridge::backup::type::eeprom_undetected: return "eeprom_undetected";
                    case cartridge::backup::type::eeprom_4: return "eeprom_4";
                    case cartridge::backup::type::eeprom_64: return "eeprom_64";
                    case cartridge::backup::type::sram: return "sram";
                    case cartridge::backup::type::flash_64: return "flash_64";
                    case cartridge::backup::type::flash_128: return "flash_128";
                    default:
                        UNREACHABLE();
                }
            };

            if(ImGui::BeginTabItem("Info")) {
                ImGui::Text("title: {}", access_private::game_title_(pak));
                ImGui::Text("game code: {}", access_private::game_code_(pak));
                ImGui::Text("maker code: {}", access_private::maker_code_(pak));

                ImGui::Spacing();
                ImGui::Text("backup: {}", backup_str());

                ImGui::Text("main unit code: {:02X}", access_private::main_unit_code_(pak));
                ImGui::Text("software version: {:02X}", access_private::software_version_(pak));
                ImGui::Text("checksum: {:02X}", access_private::checksum_(pak));

                ImGui::Spacing();
                ImGui::Text("loaded: {}", access_private::loaded_(pak));
                ImGui::Text("has rtc: {}", access_private::has_rtc_(pak));
                ImGui::Text("has mirroring: {}", access_private::has_mirroring_(pak));

                ImGui::Spacing();
                ImGui::Text("path: {}", access_private::path_(pak).string());
                ImGui::EndTabItem();
            }

            if(backup_type != cartridge::backup::type::none && backup_type != cartridge::backup::type::sram) {
                if(ImGui::BeginTabItem(fmt::format("Backup: {}", backup_str()).c_str())) {
                    switch(backup_type) {
                        case cartridge::backup::type::eeprom_undetected:
                        case cartridge::backup::type::eeprom_4:
                        case cartridge::backup::type::eeprom_64: {
                            auto* eeprom = dynamic_cast<cartridge::backup_eeprom*>(access_private::backup_(pak).get());
                            ASSERT(eeprom);
                            ImGui::Text("bus width: {}", access_private::bus_width_(eeprom));
                            ImGui::Text("buffer: {:0B}", access_private::buffer_(eeprom));
                            ImGui::Text("transmission count: {}", access_private::transmission_count_(eeprom));
                            ImGui::Text("in read mode: {}", access_private::read_mode_(eeprom));
                            ImGui::Text("state: {}", [&]() {
                                switch(access_private::state_(eeprom)) {
                                    case cartridge::backup_eeprom::state_debugger::accepting_commands: return "accepting_commands";
                                    case cartridge::backup_eeprom::state_debugger::transmitting_addr: return "transmitting_addr";
                                    case cartridge::backup_eeprom::state_debugger::transmitting_data: return "transmitting_data";
                                    case cartridge::backup_eeprom::state_debugger::transmitting_ignored_bits: return "transmitting_ignored_bits";
                                    case cartridge::backup_eeprom::state_debugger::waiting_finish_bit: return "waiting_finish_bit";
                                    default:
                                        UNREACHABLE();
                                }
                            }());
                            break;
                        }
                        case cartridge::backup::type::flash_64:
                        case cartridge::backup::type::flash_128: {
                            auto* flash = dynamic_cast<cartridge::backup_flash*>(access_private::backup_(pak).get());
                            ASSERT(flash);
                            ImGui::Text("bank: {}", access_private::current_bank_(flash));

                            const u16 device_id = memcpy<u16>(access_private::device_id_(flash), 0_usize);
                            ImGui::Text("device id: {} vendor: {}", device_id, device_id == 0xD4BF_u16 ? "SST" : "Macronix");
                            ImGui::Text("state: {}", [&]() {
                                switch(access_private::state_(flash)) {
                                    case cartridge::backup_flash::state_debugger::accept_cmd: return "accept_cmd";
                                    case cartridge::backup_flash::state_debugger::cmd_phase1: return "cmd_phase1";
                                    case cartridge::backup_flash::state_debugger::cmd_phase2: return "cmd_phase2";
                                    case cartridge::backup_flash::state_debugger::cmd_phase3: return "cmd_phase3";
                                    default:
                                        UNREACHABLE();
                                }
                            }());
                            ImGui::Text("commands: {}", [&]() -> std::string {
                                const cartridge::backup_flash::cmd_debugger current_cmds = access_private::current_cmds_(flash);
                                if(current_cmds == cartridge::backup_flash::cmd_debugger::none) {
                                    return "none";
                                }

                                std::stringstream stream;
                                const char* delim = "";
                                for(auto cmd : {
                                  cartridge::backup_flash::cmd_debugger::device_id,
                                  cartridge::backup_flash::cmd_debugger::erase,
                                  cartridge::backup_flash::cmd_debugger::write_byte,
                                  cartridge::backup_flash::cmd_debugger::select_bank
                                }) {
                                    stream << delim;
                                    delim = ", ";
                                    switch(cmd) {
                                        case cartridge::backup_flash::cmd_debugger::device_id:
                                            stream << "device_id";
                                            break;
                                        case cartridge::backup_flash::cmd_debugger::erase:
                                            stream << "erase";
                                            break;
                                        case cartridge::backup_flash::cmd_debugger::write_byte:
                                            stream << "write_byte";
                                            break;
                                        case cartridge::backup_flash::cmd_debugger::select_bank:
                                            stream << "select_bank";
                                            break;
                                        default:
                                            UNREACHABLE();
                                    }
                                }
                                return stream.str();
                            }());
                            break;
                        }
                        default:
                            UNREACHABLE();
                    }

                    ImGui::EndTabItem();
                }
            }

            if(access_private::has_rtc_(pak)) {
                if(ImGui::BeginTabItem("RTC")) {
                    auto& rtc = access_private::rtc_(pak);

                    if(ImGui::BeginTable("#rtc_pininfo", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        const u8 pin_states = access_private::pin_states_(rtc);
                        const u8 pin_directions = access_private::directions_(rtc);

                        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("p3");
                        ImGui::TableNextColumn(); ImGui::TextUnformatted("cs");
                        ImGui::TableNextColumn(); ImGui::TextUnformatted("sio");
                        ImGui::TableNextColumn(); ImGui::TextUnformatted("sck");
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("states:     {:04B}", pin_states);
                        ImGui::TableNextColumn(); ImGui::Text("{}", bit::extract(pin_states, 3_u8));
                        ImGui::TableNextColumn(); ImGui::Text("{}", bit::extract(pin_states, 2_u8));
                        ImGui::TableNextColumn(); ImGui::Text("{}", bit::extract(pin_states, 1_u8));
                        ImGui::TableNextColumn(); ImGui::Text("{}", bit::extract(pin_states, 0_u8));
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("directions: {:04B}", pin_directions);
                        ImGui::TableNextColumn(); ImGui::Text("{} ({})", bit::extract(pin_directions, 3_u8), bit::test(pin_directions, 3_u8) ? "out" : "in");
                        ImGui::TableNextColumn(); ImGui::Text("{} ({})", bit::extract(pin_directions, 2_u8), bit::test(pin_directions, 2_u8) ? "out" : "in");
                        ImGui::TableNextColumn(); ImGui::Text("{} ({})", bit::extract(pin_directions, 1_u8), bit::test(pin_directions, 1_u8) ? "out" : "in");
                        ImGui::TableNextColumn(); ImGui::Text("{} ({})", bit::extract(pin_directions, 0_u8), bit::test(pin_directions, 0_u8) ? "out" : "in");
                        ImGui::EndTable();
                    }
                    ImGui::Text("remaining bytes: {}", access_private::remaining_bytes_(rtc));
                    ImGui::Text("read bits: {}", access_private::bits_read_(rtc));
                    ImGui::Text("bit buffer: {:08B}", access_private::bit_buffer_(rtc));
                    ImGui::Text("read allowed: {}", access_private::read_allowed_(rtc));

                    ImGui::TextUnformatted("RTC data"); ImGui::Separator();
                    ImGui::Text("24h mode: {}", bit::test(access_private::control_(rtc), 6_u8));
                    ImGui::Text("transfer state: {}", [&]() {
                        switch(access_private::transfer_state_(rtc)) {
                            case cartridge::rtc::transfer_state_debugger::waiting_hi_sck: return "waiting_hi_sck";
                            case cartridge::rtc::transfer_state_debugger::waiting_hi_cs: return "waiting_hi_cs";
                            case cartridge::rtc::transfer_state_debugger::transferring_cmd: return "transferring_cmd";
                            default:
                                UNREACHABLE();
                        }
                    }());

                    auto& rtc_cmd = access_private::current_cmd_(rtc);
                    ImGui::Text("rtc command: {}, is read access read: {}", [&]() {
                        switch(rtc_cmd.cmd_type) {
                            case cartridge::rtc_command::type::none: return "none";
                            case cartridge::rtc_command::type::reset: return "reset";
                            case cartridge::rtc_command::type::date_time: return "date_time";
                            case cartridge::rtc_command::type::force_irq: return "force_irq";
                            case cartridge::rtc_command::type::control: return "control";
                            case cartridge::rtc_command::type::time: return "time";
                            default:
                                UNREACHABLE();
                        }
                    }(), rtc_cmd.is_access_read);
                    ImGui::TextUnformatted("Time"); ImGui::Separator();

                    auto& time = access_private::time_regs_(rtc);
                    ImGui::BeginGroup();
                    ImGui::Text("year:  {:X} ({})", time[0_usize], time[0_usize]);
                    ImGui::Text("month: {:X} ({})", time[1_usize], time[1_usize]);
                    ImGui::Text("mday:  {:X} ({})", time[2_usize], time[2_usize]);
                    ImGui::Text("wday:  {:X} ({})", time[3_usize], time[3_usize]);
                    ImGui::EndGroup(); ImGui::SameLine(0.f, 100.f);
                    ImGui::BeginGroup();
                    ImGui::Text("hour:  {:X} ({})", time[4_usize], time[4_usize]);
                    ImGui::Text("min:   {:X} ({})", time[5_usize], time[5_usize]);
                    ImGui::Text("sec:   {:X} ({})", time[6_usize], time[6_usize]);
                    ImGui::EndGroup();

                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}

} // namespace gba::debugger
