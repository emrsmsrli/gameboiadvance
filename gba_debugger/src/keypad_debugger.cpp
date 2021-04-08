/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/keypad_debugger.h>

#include <imgui.h>

#include <gba/keypad.h>
#include <gba/helper/range.h>
#include <gba_debugger/debugger_helpers.h>

namespace gba::debugger {

void keypad_debugger::draw() const noexcept
{
    if(ImGui::Begin("Keypad")) {
        const auto get_key_str = [](keypad::key k) {
            switch(k) {
                case keypad::key::a: return "a";
                case keypad::key::b: return "b";
                case keypad::key::select: return "select";
                case keypad::key::start: return "start";
                case keypad::key::right: return "right";
                case keypad::key::left: return "left";
                case keypad::key::up: return "up";
                case keypad::key::down: return "down";
                case keypad::key::right_shoulder: return "right_shoulder";
                case keypad::key::left_shoulder: return "left_shoulder";
                case keypad::key::max: return "max";
                default:
                    UNREACHABLE();
            }
        };

        const auto draw_input_reg = [&](const char* name, const u16 reg, auto modifier) {
            ImGui::Text("{}: {:04X}", name, reg);
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                for(keypad::key k : range(keypad::key::a, keypad::key::max)) {
                    ImGui::Text("{}: {}", get_key_str(k), modifier(bit::test(reg, from_enum<u8>(k))));
                }
                ImGui::EndTooltip();
            }
        };

        draw_input_reg("input", kp->keyinput_, [](bool t) { return !t; });
        draw_input_reg("irq mask", kp->keycnt_.select, [](bool t) { return t; });
        ImGui::Text("irq enable: {}", kp->keycnt_.enabled);
        ImGui::Text("irq condition: {}", [&]() {
            switch(kp->keycnt_.cond_strategy) {
                case keypad::irq_control::condition_strategy::any: return "any";
                case keypad::irq_control::condition_strategy::all: return "all";
                default:
                    UNREACHABLE();
            }
        }());
    }

    ImGui::End();
}

} // namespace gba::debugger
