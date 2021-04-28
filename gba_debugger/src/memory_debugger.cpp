/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/memory_debugger.h>

#include <imgui.h>
#include <imgui_memory_editor/imgui_memory_editor.h>

#include <gba_debugger/disassembler.h>
#include <gba/helper/variant_visit.h>

namespace gba::debugger {

namespace {

template<typename InstrType>
void do_draw(const memory_view_entry& entry) noexcept
{
    constexpr auto instr_size = sizeof(InstrType);
    ImGuiListClipper clipper(static_cast<int>(entry.data->size().get() / instr_size));
    while(clipper.Step()) {
        for(auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto address = static_cast<u32::type>(instr_size * i);
            InstrType inst = memcpy<InstrType>(*entry.data, address);
            if constexpr(instr_size == 2) {
                ImGui::Text("%08X | %04X | %s",
                  entry.base_addr.get() + address,
                  inst.get(),
                  disassembler::disassemble_thumb(address, inst).c_str());
            } else {
                ImGui::Text("%08X | %08X | %s",
                  entry.base_addr.get() + address,
                  inst.get(),
                  disassembler::disassemble_arm(address, inst).c_str());
            }
        }
    }
}

} // namespace

void disassembly_view::draw_with_mode(bool thumb_mode) noexcept
{
    if(ImGui::Begin("Disassembly")) {
        if(ImGui::BeginTabBar("#disassembly_tab")) {
            for(const auto& entry_var : entries_) {
                visit_nt(entry_var, overload{
                    [&](const memory_view_entry& entry) {
                        if(ImGui::BeginTabItem(entry.name.data())) {
                            static int mode = 0;
                            static constexpr array modes{"auto", "arm", "thumb"};
                            ImGui::Combo("mode", &mode, modes.data(), modes.size().get());

                            if(mode > 0) {
                                thumb_mode = mode == 2;
                            }

                            ImGui::BeginChild("#disassembly_child");
                            if(thumb_mode) {
                                do_draw<u16>(entry);
                            } else {
                                do_draw<u32>(entry);
                            }
                            ImGui::EndChild();
                            ImGui::EndTabItem();
                        }
                    },
                    [](custom_disassembly_entry) {
                        if(ImGui::BeginTabItem("Custom")) {
                            static int mode = 0;
                            static constexpr array modes{"arm", "thumb"};
                            static array<char, 9> address_buf{};
                            static array<char, 9> instr_buf{};
                            static u32 inst = 0x0000'0000_u32;
                            static u32 addr = 0x0000'0000_u32;

                            if(ImGui::Combo("mode", &mode, modes.data(), modes.size().get())) {
                                std::memset(instr_buf.data(), 0, 9);
                            }

                            const bool thumb = mode == 1;

                            ImGui::SetNextItemWidth(120);
                            const bool new_addr_available = ImGui::InputText("address", address_buf.data(), address_buf.size().get(),
                              ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
                            if(new_addr_available) {
                                if(std::strlen(address_buf.data()) != 0) {
                                    addr = static_cast<u32::type>(std::strtoul(address_buf.data(), nullptr, 16));
                                } else {
                                    addr =  0x0000'0000_u32;
                                }
                            }

                            ImGui::SetNextItemWidth(120);
                            const bool new_instr_available = ImGui::InputText("instr", instr_buf.data(), thumb ? 5 : 9,
                              ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
                            if(new_instr_available) {
                                if(std::strlen(instr_buf.data()) != 0) {
                                    inst = static_cast<u32::type>(std::strtoul(instr_buf.data(), nullptr, 16));
                                } else {
                                    inst =  0x0000'0000_u32;
                                }
                            }

                            if(thumb) {
                                ImGui::TextUnformatted(disassembler::disassemble_thumb(addr, narrow<u16>(inst)).c_str());
                            } else {
                                ImGui::TextUnformatted(disassembler::disassemble_arm(addr, inst).c_str());
                            }
                            ImGui::EndTabItem();
                        }
                    }
                });
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void memory_view::draw() noexcept
{
    static MemoryEditor memory_editor;
    memory_editor.ReadOnly = true;

    if(ImGui::Begin("Memory")) {
        if(ImGui::BeginTabBar("#memory_tab")) {
            for(const memory_view_entry& entry : entries_) {
                if(ImGui::BeginTabItem(entry.name.data())) {
                    memory_editor.DrawContents(entry.data->data(), entry.data->size().get(), entry.base_addr.get());
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

} // namespace gba::debugger
