/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/arm_debugger.h>

#include <algorithm>
#include <sstream>

#include <imgui.h>
#include <access_private.h>

#include <gba/core.h>
#include <gba_debugger/debugger_helpers.h>
#include <gba_debugger/disassembler.h>
#include <gba/helper/range.h>

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

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::waitstate_control, waitcnt_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::halt_control, haltcnt_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u8, post_boot_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::scheduler::event::handle, interrupt_delay_handle_)

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::arm7tdmi::timers_debugger, timers_)
ACCESS_PRIVATE_FIELD(gba::arm::timer, gba::u32, id_)
ACCESS_PRIVATE_FIELD(gba::arm::timer, gba::u32, counter_)
ACCESS_PRIVATE_FIELD(gba::arm::timer, gba::u16, reload_)
ACCESS_PRIVATE_FIELD(gba::arm::timer, gba::arm::timer_control, control_)

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::dma::controller, dma_controller_)
ACCESS_PRIVATE_FIELD(gba::dma::controller, gba::u32, latch_)
ACCESS_PRIVATE_FIELD(gba::dma::controller, gba::dma::controller::channels_debugger, running_channels_)
ACCESS_PRIVATE_FIELD(gba::dma::controller, gba::dma::controller::channels_debugger, scheduled_channels_)

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, iwram_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, wram_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, bios_)

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::core*, core_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::vector<gba::u8>, pak_data_)

namespace gba::debugger {

namespace {

void draw_regs(arm::arm7tdmi* arm) noexcept
{
    ImGui::TextUnformatted("Registers");
    ImGui::Separator();

    const auto print_reg = [](const u32 reg) {
        ImGui::TableNextColumn();
        if(reg == 0_u32) {
            ImGui::TextColored(ImColor(ImGui::GetColorU32(ImGuiCol_TextDisabled)), "%08X", reg.get());
        } else {
            ImGui::Text("{:08X}", reg);
        }
    };

    const auto print_reg_n = [&](const u32 reg, const u32 n) {
        for(u32 i = 0_u32; i < n; ++i) {
            print_reg(reg);
        }
    };

    const auto psr_tooltip = [](arm::psr& p) {
        if(ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("n: {}", p.n);  // signed flag
            ImGui::Text("z: {}", p.z);  // zero flag
            ImGui::Text("c: {}", p.c);  // carry flag
            ImGui::Text("v: {}", p.v);  // overflow flag
            ImGui::Text("i: {}", p.i);  // irq disabled flag
            ImGui::Text("f: {}", p.f);  // fiq disabled flag
            ImGui::Text("t: {}", p.t);  // thumb mode flag
            ImGui::Text("mode: {}", [&]() {
                switch(p.mode) {
                    case arm::privilege_mode::usr: return "usr";
                    case arm::privilege_mode::fiq: return "fiq";
                    case arm::privilege_mode::irq: return "irq";
                    case arm::privilege_mode::svc: return "svc";
                    case arm::privilege_mode::abt: return "abt";
                    case arm::privilege_mode::und: return "und";
                    case arm::privilege_mode::sys: return "sys";
                    default: UNREACHABLE();
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
        print_reg_n(access_private::r0_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R1");
        print_reg_n(access_private::r1_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R2");
        print_reg_n(access_private::r2_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R3");
        print_reg_n(access_private::r3_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R4");
        print_reg_n(access_private::r4_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R5");
        print_reg_n(access_private::r5_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R6");
        print_reg_n(access_private::r6_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R7");
        print_reg_n(access_private::r7_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R8");
        print_reg(access_private::r8_(arm));
        print_reg(access_private::fiq_(arm).r8);
        print_reg_n(access_private::r8_(arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R9");
        print_reg(access_private::r9_(arm));
        print_reg(access_private::fiq_(arm).r9);
        print_reg_n(access_private::r9_(arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R10");
        print_reg(access_private::r10_(arm));
        print_reg(access_private::fiq_(arm).r10);
        print_reg_n(access_private::r10_(arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R11");
        print_reg(access_private::r11_(arm));
        print_reg(access_private::fiq_(arm).r11);
        print_reg_n(access_private::r11_(arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R12");
        print_reg(access_private::r12_(arm));
        print_reg(access_private::fiq_(arm).r12);
        print_reg_n(access_private::r12_(arm), 4_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R13");
        print_reg(access_private::r13_(arm));
        print_reg(access_private::fiq_(arm).r13);
        print_reg(access_private::svc_(arm).r13);
        print_reg(access_private::abt_(arm).r13);
        print_reg(access_private::irq_(arm).r13);
        print_reg(access_private::und_(arm).r13);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R14");
        print_reg(access_private::r14_(arm));
        print_reg(access_private::fiq_(arm).r14);
        print_reg(access_private::svc_(arm).r14);
        print_reg(access_private::abt_(arm).r14);
        print_reg(access_private::irq_(arm).r14);
        print_reg(access_private::und_(arm).r14);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R15");
        print_reg_n(access_private::r15_(arm), 6_u32);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("CPSR");
        for(u32 i = 0_u32; i < 6_u32; ++i) {
            print_reg(static_cast<u32>(access_private::cpsr_(arm)));
            psr_tooltip(access_private::cpsr_(arm));
        }
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("SPSR");
        /* no spsr in usr/sys */ ImGui::TableNextColumn();
        print_reg(static_cast<u32>(access_private::fiq_(arm).spsr));
        psr_tooltip(access_private::fiq_(arm).spsr);
        print_reg(static_cast<u32>(access_private::svc_(arm).spsr));
        psr_tooltip(access_private::svc_(arm).spsr);
        print_reg(static_cast<u32>(access_private::abt_(arm).spsr));
        psr_tooltip(access_private::abt_(arm).spsr);
        print_reg(static_cast<u32>(access_private::irq_(arm).spsr));
        psr_tooltip(access_private::irq_(arm).spsr);
        print_reg(static_cast<u32>(access_private::und_(arm).spsr));
        psr_tooltip(access_private::und_(arm).spsr);

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

template<typename T, typename BPContainer>
void draw_bps(BPContainer&& container, T&& f) noexcept
{
    int idx_to_delete = -1;
    ImGuiListClipper clipper(container.size().get());
    while(clipper.Step()) {
        for(auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            auto& bp = container[static_cast<u32::type>(i)];

            ImGui::Button("X");
            if(ImGui::IsItemClicked()) { // hack
                idx_to_delete = i;
            }

            ImGui::SameLine(0, 5);
            ImGui::Checkbox("", &bp.enabled);
            if(ImGui::IsItemClicked()) { // hack
                bp.enabled = !bp.enabled;
            }

            ImGui::SameLine(0, 10);
            f(bp);
        }
    }

    if(idx_to_delete != -1) {
        container.erase(container.begin() + idx_to_delete);
    }
}

std::string_view to_string_view(const arm::interrupt_source irq) noexcept
{
    switch(irq) {
        case arm::interrupt_source::vblank: return "vblank";
        case arm::interrupt_source::hblank: return "hblank";
        case arm::interrupt_source::vcounter_match: return "vcounter_match";
        case arm::interrupt_source::timer_0_overflow: return "timer_0_overflow";
        case arm::interrupt_source::timer_1_overflow: return "timer_1_overflow";
        case arm::interrupt_source::timer_2_overflow: return "timer_2_overflow";
        case arm::interrupt_source::timer_3_overflow: return "timer_3_overflow";
        case arm::interrupt_source::serial_io: return "serial_io";
        case arm::interrupt_source::dma_0: return "dma_0";
        case arm::interrupt_source::dma_1: return "dma_1";
        case arm::interrupt_source::dma_2: return "dma_2";
        case arm::interrupt_source::dma_3: return "dma_3";
        case arm::interrupt_source::keypad: return "keypad";
        case arm::interrupt_source::gamepak: return "gamepak";
        default:
            UNREACHABLE();
    }
}

std::string_view to_string_view(const dma::channel::control::address_control control) noexcept
{
    switch(control) {
        case dma::channel::control::address_control::increment: return "increment";
        case dma::channel::control::address_control::decrement: return "decrement";
        case dma::channel::control::address_control::fixed: return "fixed";
        case dma::channel::control::address_control::inc_reload: return "inc_reload";
        default:
            UNREACHABLE();
    }
}

std::string_view to_string_view(const arm::debugger_access_width width) noexcept
{
    switch(width) {
        case arm::debugger_access_width::byte: return "byte";
        case arm::debugger_access_width::hword: return "hword";
        case arm::debugger_access_width::word: return "word";
        case arm::debugger_access_width::any: return "any";
        default:
            UNREACHABLE();
    }
}

std::string_view to_string_view(const arm_debugger::access_breakpoint::type type) noexcept
{
    switch(type) {
        case arm_debugger::access_breakpoint::type::read: return "read";
        case arm_debugger::access_breakpoint::type::write: return "write";
        case arm_debugger::access_breakpoint::type::read_write: return "read_write";
        default:
            UNREACHABLE();
    }
}

} // namespace

bool arm_debugger::access_breakpoint::operator==(const arm_debugger::access_breakpoint& other) const noexcept {
    return address_range.contains(other.address_range)
      && other.access_width == access_width
      && other.access_type == access_type
      && (access_width == arm::debugger_access_width::any || other.data == data);
}

void arm_debugger::draw() noexcept
{
    if(ImGui::Begin("ARM")) {
        if(ImGui::BeginTabBar("#arm_tab")) {
            if(ImGui::BeginTabItem("Registers")) {
                draw_regs(arm_);

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::TextUnformatted("Pipeline");
                ImGui::Separator();
                const auto draw_pipeline_instr = [&](const char* name, const u32 instr, const u32 offset) {
                    ImGui::Text(access_private::cpsr_(arm_).t ? "{}: {:04X}" : "{}: {:08X}", name, instr);
                    if(ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        if(access_private::cpsr_(arm_).t) {
                            ImGui::TextUnformatted(disassembler::disassemble_thumb(
                              access_private::r15_(arm_) - offset * 4_u32,
                              narrow<u16>(instr)).c_str());
                        } else {
                            ImGui::TextUnformatted(disassembler::disassemble_arm(
                              access_private::r15_(arm_) - offset * 2_u32,
                              instr).c_str());
                        }
                        ImGui::EndTooltip();
                    }
                };

                ImGui::BeginGroup(); {
                    draw_pipeline_instr("executing", access_private::pipeline_(arm_).executing, 1_u32);
                    draw_pipeline_instr("decoding", access_private::pipeline_(arm_).decoding, 2_u32);
                    ImGui::Text("fetch: {}", [&]() {
                        switch(access_private::pipeline_(arm_).fetch_type) {
                            case arm::mem_access::non_seq: return "non seq";
                            case arm::mem_access::seq: return "seq";
                            default: UNREACHABLE();
                        }
                    }());

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::Text("postboot: {:X}", access_private::post_boot_(arm_).get());
                    ImGui::Text("haltcnt: {}", [&]() {
                        switch(access_private::haltcnt_(arm_)) {
                            case arm::halt_control::halted: return "halted";
                            case arm::halt_control::stopped: return "stopped";
                            case arm::halt_control::running: return "running";
                            default: UNREACHABLE();
                        }
                    }());

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Spacing();

                    const auto draw_irq_reg = [](const u16 reg) {
                        ImGui::Text("ie: {:04X}", reg);
                        if(ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            for(u16 idx : range(14_u16)) {
                                u16 irq = 1_u16 << idx;
                                bool s = (reg & irq) == irq;
                                ImGui::Text("{}: {}", to_string_view(to_enum<arm::interrupt_source>(irq)), s);
                            }
                            ImGui::EndTooltip();
                        }
                    };

                    draw_irq_reg(access_private::ie_(arm_));
                    draw_irq_reg(access_private::if_(arm_));
                    ImGui::Text("ime: {}", access_private::ime_(arm_));
                    ImGui::EndGroup();
                }

                ImGui::SameLine(0.f, 75.f);

                ImGui::BeginGroup(); {
                    ImGui::TextUnformatted("waitcnt");
                    ImGui::Separator();
                    ImGui::Text("sram {}", access_private::waitcnt_(arm_).sram);
                    ImGui::Text("ws0_nonseq {}", access_private::waitcnt_(arm_).ws0_nonseq);
                    ImGui::Text("ws0_seq {}", access_private::waitcnt_(arm_).ws0_seq);
                    ImGui::Text("ws1_nonseq {}", access_private::waitcnt_(arm_).ws1_nonseq);
                    ImGui::Text("ws1_seq {}", access_private::waitcnt_(arm_).ws1_seq);
                    ImGui::Text("ws2_nonseq {}", access_private::waitcnt_(arm_).ws2_nonseq);
                    ImGui::Text("ws2_seq {}", access_private::waitcnt_(arm_).ws2_seq);
                    ImGui::Text("phi {}", access_private::waitcnt_(arm_).phi);
                    ImGui::Text("prefetch {}", access_private::waitcnt_(arm_).prefetch_buffer_enable);
                    ImGui::EndGroup();
                }

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Timers")) {
                for(auto& timer : access_private::timers_(arm_)) {
                    ImGui::Text("Timer {}", access_private::id_(timer));
                    ImGui::Separator();
                    ImGui::Text("counter: {:04X} reload: {:04X}",
                      access_private::counter_(timer),
                      access_private::reload_(timer));

                    auto& cnt = access_private::control_(timer);
                    constexpr array prescalar_shifts{0_u8, 6_u8, 8_u8, 10_u8};

                    ImGui::Text("prescalar: F/{}", 1_u32 << prescalar_shifts[cnt.prescalar]);
                    ImGui::Text("cascaded: {}", cnt.cascaded);
                    ImGui::Text("irq: {}", cnt.irq_enabled);
                    ImGui::Text("enabled: {}", cnt.enabled);

                    ImGui::Spacing();
                    ImGui::Spacing();
                }

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("DMAs")) {
                const auto fmt_channels = [](gba::dma::controller::channels_debugger& channels) -> std::string {
                    if(channels.empty()) { return "none"; }

                    std::stringstream s;
                    std::string_view delim = "";
                    for(auto* c : channels) {
                        s << delim << c->id.get();
                        delim = ", ";
                    }
                    return s.str();
                };

                auto& controller = access_private::dma_controller_(arm_);

                ImGui::Text("Scheduled channels: {}",
                  fmt_channels(access_private::scheduled_channels_(controller)));
                ImGui::Text("Running channels: {}",
                  fmt_channels(access_private::running_channels_(controller)));

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Spacing();

                for(dma::channel& channel : controller.channels) {
                    ImGui::Text("DMA {}", channel.id);
                    ImGui::Separator();
                    ImGui::BeginGroup(); {
                        ImGui::Text("enabled: {}", channel.cnt.enabled);
                        ImGui::Text("repeat: {}", channel.cnt.repeat);
                        ImGui::Text("irq: {}", channel.cnt.irq);
                        ImGui::Text("drq: {}", channel.cnt.drq);
                        ImGui::Text("dst control: {}", to_string_view(channel.cnt.dst_control));
                        ImGui::Text("src control: {}", to_string_view(channel.cnt.src_control));
                        ImGui::Text("timing: {}", [&]() {
                            switch(channel.cnt.when) {
                                case dma::channel::control::timing::immediately: return "immediately";
                                case dma::channel::control::timing::vblank: return "vblank";
                                case dma::channel::control::timing::hblank: return "hblank";
                                case dma::channel::control::timing::special: return "special";
                                default:
                                    UNREACHABLE();
                            }
                        }());
                        ImGui::Text("size: {}", [&]() {
                            switch(channel.cnt.size) {
                                case dma::channel::control::transfer_size::hword: return "hword";
                                case dma::channel::control::transfer_size::word: return "word";
                                default:
                                    UNREACHABLE();
                            }
                        }());
                        ImGui::EndGroup();
                    }

                    ImGui::SameLine(0.f, 75.f);

                    ImGui::BeginGroup(); {
                        ImGui::TextUnformatted("Data"); ImGui::Separator();
                        ImGui::Text("src: {:08X}", channel.src);
                        ImGui::Text("dst: {:08X}", channel.dst);
                        ImGui::Text("count: {:05X}", channel.count);

                        ImGui::Spacing();
                        ImGui::Spacing();

                        ImGui::TextUnformatted("Internal Data"); ImGui::Separator();
                        ImGui::Text("src: {:08X}", channel.internal.src);
                        ImGui::Text("dst: {:08X}", channel.internal.dst);
                        ImGui::Text("count: {:05X}", channel.internal.count);
                        ImGui::EndGroup();
                    }

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Spacing();
                }

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Execution")) {
                if(ImGui::BeginChild("#breakpointschild")) {
                    draw_breakpoints();
                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Spacing();
                    draw_disassembly();
                    ImGui::EndChild();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}

void arm_debugger::draw_breakpoints() noexcept
{
    static constexpr array breakpoint_types{"Execution breakpoint", "Access breakpoint"};
    static int breakpoint_type = 0;
    ImGui::Combo("", &breakpoint_type, breakpoint_types.data(), breakpoint_types.size().get());

    if(breakpoint_type == 0) {
        draw_execution_breakpoints();
    } else if(breakpoint_type == 1) {
        draw_access_breakpoints();
    }
}

void arm_debugger::draw_execution_breakpoints() noexcept
{
    static array<char, 9> address_buf{};
    static int hit_count_target_buf;

    const bool ok_pressed = ImGui::Button("OK"); ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("address", address_buf.data(), address_buf.size().get(),
      ImGuiInputTextFlags_CharsHexadecimal
        | ImGuiInputTextFlags_CharsUppercase);
    ImGui::SameLine(); ImGui::SetNextItemWidth(120);
    if(ImGui::InputInt("hit count", &hit_count_target_buf) && hit_count_target_buf < 0) {
        hit_count_target_buf = 0;
    }

    if(ok_pressed) {
        if(std::strlen(address_buf.data()) != 0) {
            execution_breakpoint breakpoint;
            breakpoint.address = static_cast<u32::type>(std::strtoul(address_buf.data(), nullptr, 16));
            if(hit_count_target_buf != 0) {
                breakpoint.hit_count_target = static_cast<u32::type>(hit_count_target_buf);
            }

            if(std::find(
              execution_breakpoints_.begin(),
              execution_breakpoints_.end(), breakpoint) == execution_breakpoints_.end())
            {
                hit_count_target_buf = 0;
                execution_breakpoints_.push_back(breakpoint);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    draw_bps(execution_breakpoints_, [](const execution_breakpoint& bp) {
        ImGui::Text("{:08X} | hit count: {} | hit target: {}", bp.address, bp.hit_count, bp.hit_count_target);
    });
}

void arm_debugger::draw_access_breakpoints() noexcept
{
    static bool incorrect_range = false;
    static constexpr array access_type_strs{"read", "write", "read&write"};
    static constexpr array access_widths{"byte", "hword", "word", "any"};
    static int access_type = 0;
    static int access_width = 3;
    static array<char, 9> address_lo_buf{};
    static array<char, 9> address_hi_buf{};
    static array<char, 9> data_buf{};

    ImGui::PushItemWidth(120);
    ImGui::Combo("access type", &access_type, access_type_strs.data(), access_type_strs.size().get());
    ImGui::SameLine();
    const bool width_changed = ImGui::Combo("access width", &access_width,
      access_widths.data(), access_widths.size().get());

    const bool ok_pressed = ImGui::Button("OK"); ImGui::SameLine();
    ImGui::InputText("addr low", address_lo_buf.data(), address_lo_buf.size().get(),
      ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);

    ImGui::SameLine();

    const static ImVec4 default_frame_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    static ImVec4 hibuf_frame_color = default_frame_color;
    if(incorrect_range) {
        ImGui::SetKeyboardFocusHere();
        incorrect_range = false;
        hibuf_frame_color = ImVec4{.55f, 0.f, 0.f, 1.f};
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, hibuf_frame_color);
    if(ImGui::InputText("addr hi", address_hi_buf.data(), address_hi_buf.size().get(),
      ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase)) {
        hibuf_frame_color = default_frame_color;
    }
    ImGui::PopStyleColor();

    static constexpr array buf_sizes{3, 5, 9, 9};
    const int buf_size = buf_sizes[static_cast<u32::type>(access_width)];
    if(access_type > 0) {
        if(width_changed) { ImGui::SetKeyboardFocusHere(); }

        ImGui::InputText("data", data_buf.data(), buf_size,
          ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
    }

    ImGui::PopItemWidth();

    if(ok_pressed) {
        if(std::strlen(address_lo_buf.data()) != 0 &&
          std::strlen(address_hi_buf.data()) != 0) {
            static constexpr array access_types{
              access_breakpoint::type::read,
              access_breakpoint::type::write,
              access_breakpoint::type::read_write
            };
            access_breakpoint breakpoint;
            breakpoint.access_width = static_cast<arm::debugger_access_width>(access_width);
            breakpoint.access_type = access_types[static_cast<u32::type>(access_type)];

            const u32 addr_min = static_cast<u32::type>(std::strtoul(address_lo_buf.data(), nullptr, 16));
            const u32 addr_max = static_cast<u32::type>(std::strtoul(address_hi_buf.data(), nullptr, 16));
            if(addr_min < addr_max) {
                breakpoint.address_range = range<u32>(addr_min, addr_max);

                if(access_type > 0 && std::strlen(data_buf.data()) != 0) {
                    breakpoint.data = static_cast<u32::type>(std::strtoul(data_buf.data(), nullptr, 16));
                }

                const auto not_exists_with_access = [&](access_breakpoint bp,
                  arm::debugger_access_width width, access_breakpoint::type type) {
                    bp.access_width = width;
                    bp.access_type = type;
                    return std::find(
                      access_breakpoints_.begin(),
                      access_breakpoints_.end(), bp) == access_breakpoints_.end();
                };

                if(not_exists_with_access(breakpoint, arm::debugger_access_width::any, access_breakpoint::type::read_write)
                  && not_exists_with_access(breakpoint, breakpoint.access_width, access_breakpoint::type::read_write)
                  && not_exists_with_access(breakpoint, arm::debugger_access_width::any, breakpoint.access_type)
                  && not_exists_with_access(breakpoint, breakpoint.access_width, breakpoint.access_type)
                ) {
                    access_breakpoints_.push_back(breakpoint);
                }
            } else {
                incorrect_range = true;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    draw_bps(access_breakpoints_, [](const access_breakpoint& bp) {
        ImGui::Text("{:08X}-{:08X} | {:^5} | {:^10} | {:0X}",
          bp.address_range.min(), bp.address_range.max(),
          to_string_view(bp.access_width), to_string_view(bp.access_type), bp.data);
    });
}

void arm_debugger::draw_disassembly() noexcept
{
    if(ImGui::BeginChild("#armdisassemblychild")) {
        vector<u8>* memory;
        const u32 pc = access_private::r15_(arm_);
        u32 offset;
        if(pc < 0x0000'3FFF_u32) {
            memory = &access_private::bios_(arm_);
        } else if(pc < 0x0203'FFFF_u32) {
            memory = &access_private::wram_(arm_);
            offset = 0x0200'0000_u32;
        } else if(pc < 0x0300'7FFF_u32) {
            memory = &access_private::iwram_(arm_);
            offset = 0x0300'0000_u32;
        } else if(pc < 0x09FF'FFFF_u32) {
            memory = &access_private::pak_data_(access_private::core_(arm_)->pak);
            offset = 0x0800'0000_u32;
        } else if(pc < 0x0BFF'FFFF_u32) {
            memory = &access_private::pak_data_(access_private::core_(arm_)->pak);
            offset = 0xA800'0000_u32;
        } else if(pc < 0x0DFF'FFFF_u32) {
            memory = &access_private::pak_data_(access_private::core_(arm_)->pak);
            offset = 0x0C00'0000_u32;
        } else {
            // probably something is broken at this point
            // or we're execution in vram, which is very unlikely
            // don't think we'll support disassembly for it
            LOG_WARN(debugger, "unknown execution page");
            return;
        }

        const u32 pc_physical_address = pc - offset;

        const u32 instr_width = access_private::cpsr_(arm_).t ? 2_u32 : 4_u32;
        usize instr_idx = pc_physical_address / instr_width;
        if(instr_idx < 9_u32) {
            instr_idx = 0_u32;
        } else {
            instr_idx -= 9_u32;
        }

        const auto get_execution_breakpoint = [&](const u32 address) {
            return std::find_if(execution_breakpoints_.begin(), execution_breakpoints_.end(),
              [&](const execution_breakpoint& bp) { return bp.address == address; });
        };

        const auto modify_execution_breakpoint = [&](const u32 address, bool toggle) {
            const auto it = get_execution_breakpoint(address);

            if(it != execution_breakpoints_.end()) {
                if(toggle) {
                    it->enabled = !it->enabled;
                } else {
                    execution_breakpoints_.erase(it);
                }
            } else {
                execution_breakpoints_.push_back(execution_breakpoint{address, 0_u32, std::nullopt, true});
            }
        };

        const usize max = std::min(15_usize + instr_idx, memory->size() / instr_width);
        for(; instr_idx < max; ++instr_idx) {
            const u32 physical_address = instr_width * narrow<u32>(instr_idx);
            const u32 virtual_address = physical_address + offset;

            ImGui::BeginGroup();
            const float radius = ImGui::GetTextLineHeight() * .5f + 1.f;
            if(const auto it = get_execution_breakpoint(virtual_address); it != execution_breakpoints_.end()) {
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                if(it->enabled) {
                    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cursor.x + radius, cursor.y + radius),
                      radius, 0xFF0000FF);
                } else {
                    ImGui::GetWindowDrawList()->AddCircle(ImVec2(cursor.x + radius, cursor.y + radius),
                      radius, 0xFF0000FF, 0, 1.5f);
                }
            }

            ImGui::Dummy(ImVec2{radius * 2, radius * 2});
            ImGui::SameLine(0.f, 5.f);

            if(access_private::cpsr_(arm_).t) {
                const u16 inst = memcpy<u16>(*memory, physical_address);
                if(virtual_address == pc - 4_u32) {
                    ImGui::TextColored(ImColor(0xFF0000FF), "%08X | %04X | %s",
                      virtual_address.get(), inst.get(),
                      disassembler::disassemble_thumb(virtual_address, inst).c_str());
                } else {
                    ImGui::Text("{:08X} | {:04X} | {}", virtual_address,
                      inst, disassembler::disassemble_thumb(virtual_address, inst));
                }
            } else {
                const u32 inst = memcpy<u32>(*memory, physical_address);
                if(virtual_address == pc - 8_u32) {
                    ImGui::TextColored(ImColor(0xFF0000FF), "%08X | %08X | %s",
                      virtual_address.get(), inst.get(),
                      disassembler::disassemble_arm(virtual_address, inst).c_str());
                } else {
                    ImGui::Text("{:08X} | {:08X} | {}", virtual_address,
                      inst, disassembler::disassemble_arm(virtual_address, inst));
                }
            }
            ImGui::EndGroup();

            if(ImGui::IsItemClicked()) {
                const ImGuiIO& io = ImGui::GetIO();
                modify_execution_breakpoint(virtual_address, io.KeyShift);
            }
        }
        ImGui::EndChild();
    }
}

arm_debugger::execution_breakpoint* arm_debugger::get_execution_breakpoint(const u32 address) noexcept
{
    const auto it = std::find_if(execution_breakpoints_.begin(), execution_breakpoints_.end(),
      [&](const execution_breakpoint& bp) {
          return bp.enabled && bp.address == address;
      });
    if(it == execution_breakpoints_.end()) {
        return nullptr;
    }
    return &*it;
}

bool arm_debugger::has_enabled_read_breakpoint(const u32 address, const arm::debugger_access_width access_width) noexcept
{
    return std::find_if(access_breakpoints_.begin(), access_breakpoints_.end(),
      [&](const access_breakpoint& bp) {
          return bp.enabled
            && (bp.access_width == arm::debugger_access_width::any || bp.access_width == access_width)
            && bp.address_range.contains(address)
            && bitflags::is_set(bp.access_type, access_breakpoint::type::read);
      }) != access_breakpoints_.end();
}

bool arm_debugger::has_enabled_write_breakpoint(const u32 address, const u32 data,
  const arm::debugger_access_width access_width) noexcept
  {
    return std::find_if(access_breakpoints_.begin(), access_breakpoints_.end(),
      [&](const access_breakpoint& bp) {
          return bp.enabled
            && (bp.access_width == arm::debugger_access_width::any || bp.access_width == access_width)
            && bp.address_range.contains(address)
            && bitflags::is_set(bp.access_type, access_breakpoint::type::write)
            && (!bp.data.has_value() || *bp.data == data);
      }) != access_breakpoints_.end();
}

} // namespace gba::debugger
