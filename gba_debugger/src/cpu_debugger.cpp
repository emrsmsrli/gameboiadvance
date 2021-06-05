/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/cpu_debugger.h>

#include <algorithm>
#include <sstream>
#include <string_view>

#include <imgui.h>
#include <access_private.h>

#include <gba/core.h>
#include <gba_debugger/debugger_helpers.h>
#include <gba_debugger/breakpoint_database.h>
#include <gba_debugger/disassembler.h>
#include <gba_debugger/disassembly_entry.h>

using regs_t = gba::array<gba::u32, 16>;

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, regs_t, r_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::reg_banks, reg_banks_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::psr, cpsr_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::spsr_banks, spsr_banks_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::pipeline, pipeline_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u16, ie_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u16, if_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, bool, ime_)

using arm_waitstate_container = gba::array<gba::u8, 32>;

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, arm_waitstate_container, wait_16)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, arm_waitstate_container, wait_32)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::waitstate_control, waitcnt_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::halt_control, haltcnt_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u8, post_boot_)

using timers_t = gba::array<gba::timer::timer, 4>;
ACCESS_PRIVATE_FIELD(gba::timer::controller, timers_t, timers_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::u32, id_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::u32, counter_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::u16, reload_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::timer::timer_cnt, control_)

ACCESS_PRIVATE_FUN(gba::timer::timer, gba::u32() const noexcept, calculate_counter_delta)

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

void draw_reg(const u32 reg) noexcept
{
    ImGui::TableNextColumn();
    if(reg == 0_u32) {
        ImGui::TextColored(ImColor(ImGui::GetColorU32(ImGuiCol_TextDisabled)), "%08X", reg.get());
    } else {
        ImGui::Text("{:08X}", reg);
    }
};

void draw_psr_tooltip(const arm::psr& p) noexcept
{
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
                default: return "???";
            }
        }());
        ImGui::EndTooltip();
    }
}

void draw_psr(const arm::psr& spsr) noexcept
{
    draw_reg(static_cast<u32>(spsr));
    draw_psr_tooltip(spsr);
};

void draw_regs(arm::arm7tdmi* arm) noexcept
{
    ImGui::TextUnformatted("Registers");
    ImGui::Separator();

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.f, 4.f));
    if(ImGui::BeginTable("#arm_registers_new", 2,
      ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg,
      ImVec2(0.f, 0.f))) {

        const regs_t& regs = access_private::r_(arm);
        const arm::spsr_banks& spsr = access_private::spsr_banks_(arm);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted([&]() {
            switch(access_private::cpsr_(arm).mode) {
                case arm::privilege_mode::sys: return "SYS";
                case arm::privilege_mode::usr: return "USR";
                case arm::privilege_mode::fiq: return "FIQ";
                case arm::privilege_mode::irq: return "IRQ";
                case arm::privilege_mode::svc: return "SVC";
                case arm::privilege_mode::abt: return "ABT";
                case arm::privilege_mode::und: return "UND";
                default: return "???";
            }
        }());

        enumerate(regs, [&](usize idx, u32 r) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("R{}", idx);
            draw_reg(r);
        });

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("CPSR");
        draw_psr(access_private::cpsr_(arm));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("SPSR");
        switch(access_private::cpsr_(arm).mode) {
            case arm::privilege_mode::sys:
            case arm::privilege_mode::usr: break;
            case arm::privilege_mode::fiq: draw_psr(spsr[arm::register_bank::fiq]); break;
            case arm::privilege_mode::irq: draw_psr(spsr[arm::register_bank::irq]); break;
            case arm::privilege_mode::svc: draw_psr(spsr[arm::register_bank::svc]); break;
            case arm::privilege_mode::abt: draw_psr(spsr[arm::register_bank::abt]); break;
            case arm::privilege_mode::und: draw_psr(spsr[arm::register_bank::und]); break;
            default: UNREACHABLE();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();
}

void draw_banked_regs(arm::arm7tdmi* arm) noexcept
{
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

        const regs_t& r = access_private::r_(arm);
        const arm::reg_banks& banks = access_private::reg_banks_(arm);

        const auto draw_banked_reg = [&](auto&& reg_selector) {
            for(u32 b : range(from_enum<u32>(arm::register_bank::und) + 1)) {
                draw_reg(reg_selector(banks[to_enum<arm::register_bank>(b)].named));
            }
        };

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R8");
        draw_banked_reg([&](const arm::banked_regs::regs& regs) { return regs.r8; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R9");
        draw_banked_reg([&](const arm::banked_regs::regs& regs) { return regs.r9; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R10");
        draw_banked_reg([&](const arm::banked_regs::regs& regs) { return regs.r10; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R11");
        draw_banked_reg([&](const arm::banked_regs::regs& regs) { return regs.r11; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R12");
        draw_banked_reg([&](const arm::banked_regs::regs& regs) { return regs.r12; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R13");
        draw_banked_reg([&](const arm::banked_regs::regs& regs) { return regs.r13; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R14");
        draw_banked_reg([&](const arm::banked_regs::regs& regs) { return regs.r14; });
        ImGui::TableNextRow();

        const arm::spsr_banks& spsr = access_private::spsr_banks_(arm);
        ImGui::TableNextColumn(); ImGui::TextUnformatted("SPSR");
        /* no spsr in usr/sys */ ImGui::TableNextColumn();
        draw_psr(spsr[arm::register_bank::fiq]);
        draw_psr(spsr[arm::register_bank::svc]);
        draw_psr(spsr[arm::register_bank::abt]);
        draw_psr(spsr[arm::register_bank::irq]);
        draw_psr(spsr[arm::register_bank::und]);

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

template<typename T, typename BPContainer>
void draw_bps(BPContainer&& container, T&& f) noexcept
{
    int idx_to_delete = -1;
    ImGuiListClipper clipper(static_cast<int>(container.size().get()));
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

} // namespace

void cpu_debugger::draw() noexcept
{
    if(ImGui::Begin("CPU")) {
        if(ImGui::BeginTabBar("#arm_tab")) {
            if(ImGui::BeginTabItem("ARM")) {
                draw_regs(arm_); ImGui::SameLine();

                ImGui::BeginGroup();
                ImGui::TextUnformatted("Pipeline");
                ImGui::Separator();

                regs_t& r = access_private::r_(arm_);
                const auto draw_pipeline_instr = [&](const char* name, const u32 instr, const u32 offset) {
                    ImGui::Text(access_private::cpsr_(arm_).t ? "{}: {:04X}" : "{}: {:08X}", name, instr);
                    if(ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        if(access_private::cpsr_(arm_).t) {
                            ImGui::TextUnformatted(disassembler::disassemble_thumb(
                              r[15_u32] - offset * 4_u32,
                              narrow<u16>(instr)).c_str());
                        } else {
                            ImGui::TextUnformatted(disassembler::disassemble_arm(
                              r[15_u32] - offset * 2_u32,
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

                    const auto draw_irq_reg = [](const char* name, const u16 reg) {
                        ImGui::Text("{}: {:04X}", name, reg);
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

                    draw_irq_reg("ie", access_private::ie_(arm_));
                    draw_irq_reg("if", access_private::if_(arm_));
                    ImGui::Text("ime: {}", access_private::ime_(arm_));
                    ImGui::EndGroup();
                }

                ImGui::SameLine(160.f, 0.f);

                ImGui::BeginGroup(); {
                    static constexpr array<u8, 4> ws_nonseq{4_u8, 3_u8, 2_u8, 8_u8};
                    static constexpr array<u8, 2> ws0_seq{2_u8, 1_u8};
                    static constexpr array<u8, 2> ws1_seq{4_u8, 1_u8};
                    static constexpr array<u8, 2> ws2_seq{8_u8, 1_u8};

                    ImGui::TextUnformatted("waitcnt");
                    ImGui::Separator();
                    ImGui::Text("sram {} ({})", ws_nonseq[access_private::waitcnt_(arm_).sram], access_private::waitcnt_(arm_).sram);
                    ImGui::Text("ws0_nonseq {} ({})", ws_nonseq[access_private::waitcnt_(arm_).ws0_nonseq], access_private::waitcnt_(arm_).ws0_nonseq);
                    ImGui::Text("ws0_seq {} ({})", ws0_seq[access_private::waitcnt_(arm_).ws0_seq], access_private::waitcnt_(arm_).ws0_seq);
                    ImGui::Text("ws1_nonseq {} ({})", ws_nonseq[access_private::waitcnt_(arm_).ws1_nonseq], access_private::waitcnt_(arm_).ws1_nonseq);
                    ImGui::Text("ws1_seq {} ({})", ws1_seq[access_private::waitcnt_(arm_).ws1_seq], access_private::waitcnt_(arm_).ws1_seq);
                    ImGui::Text("ws2_nonseq {} ({})", ws_nonseq[access_private::waitcnt_(arm_).ws2_nonseq], access_private::waitcnt_(arm_).ws2_nonseq);
                    ImGui::Text("ws2_seq {} ({})", ws2_seq[access_private::waitcnt_(arm_).ws2_seq], access_private::waitcnt_(arm_).ws2_seq);
                    ImGui::Text("phi {}", access_private::waitcnt_(arm_).phi);
                    ImGui::Text("prefetch {}", access_private::waitcnt_(arm_).prefetch_buffer_enable);
                    ImGui::EndGroup();
                }
                ImGui::EndGroup();

                static bool show_banked_registers = false;
                ImGui::Checkbox("Show banked registers", &show_banked_registers);
                if(show_banked_registers) {
                    draw_banked_regs(arm_);
                }

                auto& w16 = access_private::wait_16(arm_);
                auto& w32 = access_private::wait_32(arm_);
                static bool show_wait_table = false;
                ImGui::Checkbox("Show wait table", &show_wait_table);

                if(show_wait_table && ImGui::BeginTable("waittable", 17,
                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    ImGui::TableSetColumnIndex(1);

                    for(u32::type page : range(0x10)) {
                        ImGui::TextUnformatted([=]() {
                            switch(page) {
                                case 0x00: return "bios";
                                case 0x01: return "N/A";
                                case 0x02: return "ewram";
                                case 0x03: return "iwram";
                                case 0x04: return "io";
                                case 0x05: return "palram";
                                case 0x06: return "vram";
                                case 0x07: return "oam";
                                case 0x08: return "pak0_l";
                                case 0x09: return "pak0_u";
                                case 0x0A: return "pak1_l";
                                case 0x0B: return "pak1_u";
                                case 0x0C: return "pak2_l";
                                case 0x0D: return "pak2_u";
                                case 0x0E: return "sram_1";
                                case 0x0F: return "sram_2";
                                default:
                                    UNREACHABLE();
                            }
                        }());
                        ImGui::TableNextColumn();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("16 ns");
                    ImGui::TableNextColumn();
                    for(u32::type page : range(0x10)) {
                        ImGui::Text("{}", w16[page]);
                        ImGui::TableNextColumn();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("16  s");
                    ImGui::TableNextColumn();
                    for(u32::type page : range(0x10)) {
                        ImGui::Text("{}", w16[page + 16]);
                        ImGui::TableNextColumn();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("32 ns");
                    ImGui::TableNextColumn();
                    for(u32::type page : range(0x10)) {
                        ImGui::Text("{}", w32[page]);
                        ImGui::TableNextColumn();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("32  s");
                    ImGui::TableNextColumn();
                    for(u32::type page : range(0x10)) {
                        ImGui::Text("{}", w32[page + 16]);
                        ImGui::TableNextColumn();
                    }

                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Timers")) {
                for(auto& timer : access_private::timers_(timer_controller_)) {
                    constexpr array prescalar_shifts{0_u8, 6_u8, 8_u8, 10_u8};
                    auto& cnt = access_private::control_(timer);

                    ImGui::Text("Timer {}", access_private::id_(timer));
                    ImGui::Separator();
                    ImGui::Text("counter: {:04X} reload: {:04X}",
                      cnt.enabled && !cnt.cascaded
                        // as accurate as possible
                        ? access_private::counter_(timer) + call_private::calculate_counter_delta(timer)
                        : access_private::counter_(timer),
                      access_private::reload_(timer));

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

                ImGui::Text("Scheduled channels: {}",
                  fmt_channels(access_private::scheduled_channels_(dma_controller_)));
                ImGui::Text("Running channels: {}",
                  fmt_channels(access_private::running_channels_(dma_controller_)));

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Spacing();

                for(dma::channel& channel : dma_controller_->channels) {
                    ImGui::Text("DMA {}", channel.id);
                    ImGui::Separator();
                    ImGui::BeginGroup(); {
                        ImGui::Text("enabled: {}", channel.cnt.enabled);
                        ImGui::Text("repeat: {}", channel.cnt.repeat);
                        ImGui::Text("irq: {}", channel.cnt.irq);
                        ImGui::Text("drq: {}", channel.cnt.drq);
                        ImGui::Text("dst control: {}", to_string_view(channel.cnt.dst_control));
                        ImGui::Text("src control: {}", to_string_view(channel.cnt.src_control));
                        ImGui::Text("timing: {}", [&]() -> std::string {
                            static constexpr array special_name{"prohibited", "fifo", "fifo", "video"};
                            switch(channel.cnt.when) {
                                case dma::channel::control::timing::immediately: return "immediately";
                                case dma::channel::control::timing::vblank: return "vblank";
                                case dma::channel::control::timing::hblank: return "hblank";
                                case dma::channel::control::timing::special:
                                    return fmt::format("special ({})", special_name[channel.id]);
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

void cpu_debugger::draw_breakpoints() noexcept
{
    if(ImGui::BeginChild("#breakpointtypeschild", ImVec2(0, 300), true)) {
        ImGui::TextUnformatted("Run:"); ImGui::SameLine();
        if(ImGui::Button("One frame")) {
            on_execution_requested(execution_request::frame);
        }
        ImGui::SameLine();
        if(ImGui::Button("One scanline")) {
            on_execution_requested(execution_request::scanline);
        }
        ImGui::SameLine();
        if(ImGui::Button("One instruction")) {
            on_execution_requested(execution_request::instruction);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        static constexpr array breakpoint_types{"Execution breakpoint", "Access breakpoint"};
        static int breakpoint_type = 0;
        ImGui::TextUnformatted("Breakpoints:");
        ImGui::Combo("", &breakpoint_type, breakpoint_types.data(), breakpoint_types.size().get());

        if(breakpoint_type == 0) {
            draw_execution_breakpoints();
        } else if(breakpoint_type == 1) {
            draw_access_breakpoints();
        }
        ImGui::EndChild();
    }
}

void cpu_debugger::draw_execution_breakpoints() noexcept
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

            if(bp_db_->add_execution_breakpoint(breakpoint)) {
                hit_count_target_buf = 0;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    draw_bps(bp_db_->get_execution_breakpoints(), [](const execution_breakpoint& bp) {
        ImGui::Text("{:08X} | hit count: {} | hit target: {}", bp.address, bp.hit_count, bp.hit_count_target);
    });
}

void cpu_debugger::draw_access_breakpoints() noexcept
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
        if(std::strlen(address_lo_buf.data()) != 0) {
            static constexpr array access_types{
              access_breakpoint::type::read,
              access_breakpoint::type::write,
              access_breakpoint::type::read_write
            };
            access_breakpoint breakpoint;
            breakpoint.access_width = static_cast<arm::debugger_access_width>(access_width);
            breakpoint.access_type = access_types[static_cast<u32::type>(access_type)];

            const u32 addr_min = static_cast<u32::type>(std::strtoul(address_lo_buf.data(), nullptr, 16));
            const u32 addr_max = std::strlen(address_hi_buf.data()) != 0
              ? static_cast<u32::type>(std::strtoul(address_hi_buf.data(), nullptr, 16))
              : addr_min + 1_u32;
            if(addr_min < addr_max) {
                breakpoint.address_range = range<u32>(addr_min, addr_max);

                if(access_type > 0 && std::strlen(data_buf.data()) != 0) {
                    breakpoint.data = static_cast<u32::type>(std::strtoul(data_buf.data(), nullptr, 16));
                }

                bp_db_->add_access_breakpoint(breakpoint);
            } else {
                incorrect_range = true;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    draw_bps(bp_db_->get_access_breakpoints(), [](const access_breakpoint& bp) {
        if(bp.address_range.size() == 1_u32) {
            ImGui::Text("    {:08X}      | {:^5} | {:^10} | {:0X}",
              bp.address_range.min(),
              to_string_view(bp.access_width), to_string_view(bp.access_type), bp.data);
        } else {
            ImGui::Text("{:08X}-{:08X} | {:^5} | {:^10} | {:0X}",
              bp.address_range.min(), bp.address_range.max(),
              to_string_view(bp.access_width), to_string_view(bp.access_type), bp.data);
        }
    });
}

void cpu_debugger::draw_disassembly() noexcept
{
    if(ImGui::BeginChild("#armdisassemblychild")) {
        vector<u8>* memory;
        const u32 pc = access_private::r_(arm_)[15_u32];
        u32 offset;
        u32 mask = rom_mirror_mask_;
        if(pc < 0x0000'3FFF_u32) {
            memory = &access_private::bios_(arm_);
            mask = 0x0000'3FFF_u32;
        } else if(pc < 0x0300'0000_u32) {
            memory = &access_private::wram_(arm_);
            offset = 0x0200'0000_u32;
            mask = 0x0003'FFFF_u32;
        } else if(pc < 0x0400'0000_u32) {
            memory = &access_private::iwram_(arm_);
            offset = 0x0300'0000_u32;
            mask = 0x0000'7FFF_u32;
        } else if(pc < 0x0A00'0000_u32) {
            memory = &access_private::pak_data_(access_private::core_(arm_)->pak);
            offset = 0x0800'0000_u32;
        } else if(pc < 0x0C00'0000_u32) {
            memory = &access_private::pak_data_(access_private::core_(arm_)->pak);
            offset = 0x0A00'0000_u32;
        } else if(pc < 0x0E00'0000_u32) {
            memory = &access_private::pak_data_(access_private::core_(arm_)->pak);
            offset = 0x0C00'0000_u32;
        } else {
            // probably something is broken at this point
            // or we're execution in vram, which is very unlikely
            // don't think we'll support disassembly for it
            LOG_WARN(debugger, "unknown execution page {:02X}", pc >> 24_u32);
            return;
        }

        const u32 pc_physical_address = pc & mask;

        const u32 instr_width = access_private::cpsr_(arm_).t ? 2_u32 : 4_u32;
        usize instr_idx = pc_physical_address / instr_width;
        if(instr_idx < 9_u32) {
            instr_idx = 0_u32;
        } else {
            instr_idx -= 9_u32;
        }

        const usize max = std::min(15_usize + instr_idx, memory->size() / instr_width);
        for(; instr_idx < max; ++instr_idx) {
            const u32 physical_address = instr_width * narrow<u32>(instr_idx);
            const u32 virtual_address = physical_address + offset;

            const bool is_thumb = access_private::cpsr_(arm_).t;
            const bool is_pc = virtual_address == pc_physical_address + offset - (2_u32 * instr_width);
            const u32 instr = is_thumb
              ? memcpy<u16>(*memory, physical_address)
              : memcpy<u32>(*memory, physical_address);

            disassembly_entry entry{bp_db_, virtual_address, instr, is_thumb, is_pc};
            entry.draw();
        }
        ImGui::EndChild();
    }
}

} // namespace gba::debugger
