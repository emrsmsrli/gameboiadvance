/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/disassembly_entry.h>

#include <imgui.h>

#include <gba_debugger/breakpoint_database.h>
#include <gba_debugger/disassembler.h>
#include <gba_debugger/debugger_helpers.h>

namespace gba::debugger {

void disassembly_entry::draw() noexcept
{
    ImGui::BeginGroup();
    const float radius = ImGui::GetTextLineHeight() * .5f + 1.f;
    if(execution_breakpoint* bp = bp_db->get_execution_breakpoint(addr)) {
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        if(bp->enabled) {
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cursor.x + radius, cursor.y + radius),
              radius, 0xFF0000FF);
        } else {
            ImGui::GetWindowDrawList()->AddCircle(ImVec2(cursor.x + radius, cursor.y + radius),
              radius, 0xFF0000FF, 0, 1.5f);
        }
    }

    ImGui::Dummy(ImVec2{radius * 2, radius * 2});
    ImGui::SameLine(0.f, 5.f);

    if(is_thumb) {
        if(is_pc) {
            ImGui::TextColored(ImColor(0xFF0000FF), "%08X | %04X | %s",
              addr.get(), instr.get(),
              disassembler::disassemble_thumb(addr, narrow<u16>(instr)).c_str());
        } else {
            ImGui::Text("{:08X} | {:04X} | {}", addr,
              instr, disassembler::disassemble_thumb(addr, narrow<u16>(instr)));
        }
    } else {
        if(is_pc) {
            ImGui::TextColored(ImColor(0xFF0000FF), "%08X | %08X | %s",
              addr.get(), instr.get(),
              disassembler::disassemble_arm(addr, instr).c_str());
        } else {
            ImGui::Text("{:08X} | {:08X} | {}", addr,
              instr, disassembler::disassemble_arm(addr, instr));
        }
    }
    ImGui::EndGroup();

    if(ImGui::IsItemClicked()) {
        const ImGuiIO& io = ImGui::GetIO();
        bp_db->modify_execution_breakpoint(addr, io.KeyShift);
    }
}

} // namespace gba::debugger
