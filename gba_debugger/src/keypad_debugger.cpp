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
        const auto get_key_str = [](keypad::keypad::key k) {
            switch(k) {
                case keypad::keypad::key::a: return "a";
                case keypad::keypad::key::b: return "b";
                case keypad::keypad::key::select: return "select";
                case keypad::keypad::key::start: return "start";
                case keypad::keypad::key::right: return "right";
                case keypad::keypad::key::left: return "left";
                case keypad::keypad::key::up: return "up";
                case keypad::keypad::key::down: return "down";
                case keypad::keypad::key::right_shoulder: return "right_shoulder";
                case keypad::keypad::key::left_shoulder: return "left_shoulder";
                case keypad::keypad::key::max: return "max";
                default:
                    UNREACHABLE();
            }
        };

        const auto draw_input_reg = [&](const char* name, const u16 reg, auto modifier) {
            ImGui::Text("%s: %04X", name, reg.get());
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                for(keypad::keypad::key k : range(keypad::keypad::key::a, keypad::keypad::key::max)) {
                    ImGui::Text("%s: %s", get_key_str(k), fmt_bool(modifier(bit::test(reg, from_enum<u8>(k)))));
                }
                ImGui::EndTooltip();
            }
        };

        draw_input_reg("input", kp->keyinput_, [](bool t) { return !t; });
        draw_input_reg("irq mask", kp->keycnt_.select, [](bool t) { return t; });
        ImGui::Text("irq enable: %s", fmt_bool(kp->keycnt_.enabled));
        ImGui::Text("irq condition: %s", [&]() {
            switch(kp->keycnt_.cond_strategy) {
                case keypad::keypad::irq_control::condition_strategy::any: return "any";
                case keypad::keypad::irq_control::condition_strategy::all: return "all";
                default:
                    UNREACHABLE();
            }
        }());
    }

    ImGui::End();
}

} // namespace gba::debugger
