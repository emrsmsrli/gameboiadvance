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
            for(const memory_view_entry& entry : entries_) {
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
