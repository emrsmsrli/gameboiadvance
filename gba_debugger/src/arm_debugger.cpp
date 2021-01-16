/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/arm_debugger.h>

#include <imgui.h>
#include <access_private.h>

#include <gba/arm/arm7tdmi.h>
#include <gba_debugger/debugger_helpers.h>

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r0_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r1_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r2_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r3_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r4_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r5_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r6_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r7_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r8_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r9_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r10_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r11_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r12_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r13_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r14_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r15_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::psr, cpsr_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::banked_fiq_regs, fiq_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::banked_mode_regs, svc_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::banked_mode_regs, abt_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::banked_mode_regs, irq_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::banked_mode_regs, und_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::pipeline, pipeline_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u16, ie_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u16, if_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, bool, ime_)

namespace gba::debugger {

namespace {

void draw_regs(arm::arm7tdmi* arm) noexcept
{
    ImGui::TextUnformatted("Registers");
    ImGui::Separator();

    const auto print_reg_n = [](const u32 reg, const u32 n) {
        for(u32 i = 0_u32; i < n; ++i) {
            ImGui::TableNextColumn();
            ImGui::Text("%08X", reg.get());
        }
    };

    const auto psr_tooltip = [](arm::psr& p) {
        if(ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("n: %s", fmt_bool(p.n));  // signed flag
            ImGui::Text("z: %s", fmt_bool(p.z));  // zero flag
            ImGui::Text("c: %s", fmt_bool(p.c));  // carry flag
            ImGui::Text("v: %s", fmt_bool(p.v));  // overflow flag
            ImGui::Text("i: %s", fmt_bool(p.i));  // irq disabled flag
            ImGui::Text("f: %s", fmt_bool(p.f));  // fiq disabled flag
            ImGui::Text("t: %s", fmt_bool(p.t));  // thumb mode flag
            ImGui::Text("mode: %s", [&]() {
                switch(p.mode) {
                    case arm::privilege_mode::usr: return "usr";
                    case arm::privilege_mode::fiq: return "fiq";
                    case arm::privilege_mode::irq: return "irq";
                    case arm::privilege_mode::svc: return "svc";
                    case arm::privilege_mode::abt: return "abt";
                    case arm::privilege_mode::und: return "und";
                    case arm::privilege_mode::sys: return "sys";
                }
            }());
            ImGui::EndTooltip();
        }
    };

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.f, 4.f));
    if(ImGui::BeginTable("#arm_registers", 7,
      ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg,
      ImVec2(0.f, 0.f))) {
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(1);

        ImGui::TextUnformatted("USR/SYS"); ImGui::TableNextColumn();
        ImGui::TextUnformatted("FIQ"); ImGui::TableNextColumn();
        ImGui::TextUnformatted("SVC"); ImGui::TableNextColumn();
        ImGui::TextUnformatted("ABT"); ImGui::TableNextColumn();
        ImGui::TextUnformatted("IRQ"); ImGui::TableNextColumn();
        ImGui::TextUnformatted("UND");
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R0");
        print_reg_n(access_private::r0_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R1");
        print_reg_n(access_private::r1_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R2");
        print_reg_n(access_private::r2_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R3");
        print_reg_n(access_private::r3_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R4");
        print_reg_n(access_private::r4_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R5");
        print_reg_n(access_private::r5_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R6");
        print_reg_n(access_private::r6_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R7");
        print_reg_n(access_private::r7_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R8");
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::r8_(*arm).get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::fiq_(*arm).r8.get());
        print_reg_n(access_private::r8_(*arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R9");
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::r9_(*arm).get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::fiq_(*arm).r9.get());
        print_reg_n(access_private::r9_(*arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R10");
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::r10_(*arm).get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::fiq_(*arm).r10.get());
        print_reg_n(access_private::r10_(*arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R11");
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::r11_(*arm).get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::fiq_(*arm).r11.get());
        print_reg_n(access_private::r11_(*arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R12");
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::r12_(*arm).get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::fiq_(*arm).r12.get());
        print_reg_n(access_private::r12_(*arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R13");
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::r13_(*arm).get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::fiq_(*arm).r13.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::svc_(*arm).r13.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::abt_(*arm).r13.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::irq_(*arm).r13.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::und_(*arm).r13.get());
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R14");
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::r14_(*arm).get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::fiq_(*arm).r14.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::svc_(*arm).r14.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::abt_(*arm).r14.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::irq_(*arm).r14.get());
        ImGui::TableNextColumn(); ImGui::Text("%08X", access_private::und_(*arm).r14.get());
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R15");
        print_reg_n(access_private::r15_(*arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("CPSR");
        for(u32 i = 0_u32; i < 6_u32; ++i) {
            ImGui::TableNextColumn();
            ImGui::Text("%08X", static_cast<u32>(access_private::cpsr_(*arm)).get());
            psr_tooltip(access_private::cpsr_(*arm));
        }
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("SPSR");
        /* no spsr in usr/sys */ ImGui::TableNextColumn();
        ImGui::TableNextColumn(); ImGui::Text("%08X", static_cast<u32>(access_private::fiq_(*arm).spsr).get());
        psr_tooltip(access_private::fiq_(*arm).spsr);
        ImGui::TableNextColumn(); ImGui::Text("%08X", static_cast<u32>(access_private::svc_(*arm).spsr).get());
        psr_tooltip(access_private::svc_(*arm).spsr);
        ImGui::TableNextColumn(); ImGui::Text("%08X", static_cast<u32>(access_private::abt_(*arm).spsr).get());
        psr_tooltip(access_private::abt_(*arm).spsr);
        ImGui::TableNextColumn(); ImGui::Text("%08X", static_cast<u32>(access_private::irq_(*arm).spsr).get());
        psr_tooltip(access_private::irq_(*arm).spsr);
        ImGui::TableNextColumn(); ImGui::Text("%08X", static_cast<u32>(access_private::und_(*arm).spsr).get());
        psr_tooltip(access_private::und_(*arm).spsr);

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

} // namespace

void arm_debugger::draw() const noexcept
{
    if(ImGui::Begin("ARM")) {
        draw_regs(arm);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextUnformatted("Pipeline");
        ImGui::Separator();
        ImGui::Text("executing: %08X", access_private::pipeline_(*arm).executing.get());
        ImGui::Text("decoding: %08X", access_private::pipeline_(*arm).decoding.get());
        ImGui::Text("fetch: %s", [&]() {
            switch(access_private::pipeline_(*arm).fetch_type) {
                case arm::mem_access::non_seq: return "non seq";
                case arm::mem_access::seq: return "seq";
            }
        }());

        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Text("ie: %04X", access_private::ie_(*arm).get());
        ImGui::Text("if: %04X", access_private::if_(*arm).get());
        ImGui::Text("ime: %s", fmt_bool(access_private::ime_(*arm)));
    }
    ImGui::End();
}

} // namespace gba::debugger
