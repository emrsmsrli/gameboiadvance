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

#include <access_private.h>
#include <imgui.h>

#include <gba/core.h>
#include <gba_debugger/breakpoint_database.h>
#include <gba_debugger/debugger_helpers.h>
#include <gba_debugger/disassembler.h>
#include <gba_debugger/disassembly_entry.h>

using regs_t = gba::array<gba::u32, 16>;

ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, regs_t, r_)
ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, gba::cpu::reg_banks, reg_banks_)
ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, gba::cpu::psr, cpsr_)
ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, gba::cpu::spsr_banks, spsr_banks_)
ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, gba::cpu::pipeline, pipeline_)
ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, gba::u16, ie_)
ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, gba::u16, if_)
ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, bool, ime_)

using cpu_stall_table_entry = gba::array<gba::u8, 16>;
using cpu_stall_table_container = gba::array<cpu_stall_table_entry, 2>;
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, cpu_stall_table_container, stall_16_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, cpu_stall_table_container, stall_32_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::cpu::waitstate_control, waitcnt_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::cpu::prefetch_buffer, prefetch_buffer_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::cpu::halt_control, haltcnt_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::u8, post_boot_)

ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::timer::controller, timer_controller_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::dma::controller, dma_controller_)

using timers_t = gba::array<gba::timer::timer, 4>;
ACCESS_PRIVATE_FIELD(gba::timer::controller, timers_t, timers_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::u32, id_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::u32, counter_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::u16, reload_)
ACCESS_PRIVATE_FIELD(gba::timer::timer, gba::timer::timer_cnt, control_)

ACCESS_PRIVATE_FUN(gba::timer::timer, gba::u32() const noexcept, calculate_counter_delta)

using channels_debugger = gba::static_vector<gba::dma::channel*, gba::dma::channel_count>;
ACCESS_PRIVATE_FIELD(gba::dma::controller, gba::u32, latch_)
ACCESS_PRIVATE_FIELD(gba::dma::controller, channels_debugger, running_channels_)
ACCESS_PRIVATE_FIELD(gba::dma::controller, channels_debugger, scheduled_channels_)

ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::vector<gba::u8>, iwram_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::vector<gba::u8>, wram_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::vector<gba::u8>, bios_)

ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::vector<gba::u8>, pak_data_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::u32, mirror_mask_)

namespace gba::debugger {

namespace {

constexpr array bp_hit_types{"log", "suspend", "log&suspend"};

void draw_reg(const u32 reg) noexcept
{
    ImGui::TableNextColumn();
    if(reg == 0_u32) {
        ImGui::TextColored(ImColor(ImGui::GetColorU32(ImGuiCol_TextDisabled)), "%08X", reg.get());
    } else {
        ImGui::Text("{:08X}", reg);
    }
};

void draw_psr_tooltip(const cpu::psr& p) noexcept
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
                case cpu::privilege_mode::usr: return "usr";
                case cpu::privilege_mode::fiq: return "fiq";
                case cpu::privilege_mode::irq: return "irq";
                case cpu::privilege_mode::svc: return "svc";
                case cpu::privilege_mode::abt: return "abt";
                case cpu::privilege_mode::und: return "und";
                case cpu::privilege_mode::sys: return "sys";
                default: return "???";
            }
        }());
        ImGui::EndTooltip();
    }
}

void draw_psr(const cpu::psr& spsr) noexcept
{
    draw_reg(static_cast<u32>(spsr));
    draw_psr_tooltip(spsr);
};

void draw_regs(cpu::cpu* c) noexcept
{
    ImGui::TextUnformatted("Registers");
    ImGui::Separator();

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.f, 4.f));
    if(ImGui::BeginTable("#cpu_registers_new", 2,
      ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg,
      ImVec2(0.f, 0.f))) {

        const regs_t& regs = access_private::r_(c);
        const cpu::spsr_banks& spsr = access_private::spsr_banks_(c);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted([&]() {
            switch(access_private::cpsr_(c).mode) {
                case cpu::privilege_mode::sys: return "SYS";
                case cpu::privilege_mode::usr: return "USR";
                case cpu::privilege_mode::fiq: return "FIQ";
                case cpu::privilege_mode::irq: return "IRQ";
                case cpu::privilege_mode::svc: return "SVC";
                case cpu::privilege_mode::abt: return "ABT";
                case cpu::privilege_mode::und: return "UND";
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
        draw_psr(access_private::cpsr_(c));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("SPSR");
        switch(access_private::cpsr_(c).mode) {
            case cpu::privilege_mode::sys:
            case cpu::privilege_mode::usr: break;
            case cpu::privilege_mode::fiq: draw_psr(spsr[cpu::register_bank::fiq]); break;
            case cpu::privilege_mode::irq: draw_psr(spsr[cpu::register_bank::irq]); break;
            case cpu::privilege_mode::svc: draw_psr(spsr[cpu::register_bank::svc]); break;
            case cpu::privilege_mode::abt: draw_psr(spsr[cpu::register_bank::abt]); break;
            case cpu::privilege_mode::und: draw_psr(spsr[cpu::register_bank::und]); break;
            default: UNREACHABLE();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();
}

void draw_banked_regs(cpu::cpu* c) noexcept
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.f, 4.f));
    if(ImGui::BeginTable("#cpu_registers", 7,
      ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg,
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

        const cpu::reg_banks& banks = access_private::reg_banks_(c);

        const auto draw_banked_reg = [&](auto&& reg_selector) {
            for(u32 b : range(from_enum<u32>(cpu::register_bank::und) + 1)) {
                draw_reg(reg_selector(banks[to_enum<cpu::register_bank>(b)].named));
            }
        };

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R8");
        draw_banked_reg([&](const cpu::banked_regs::regs& regs) { return regs.r8; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R9");
        draw_banked_reg([&](const cpu::banked_regs::regs& regs) { return regs.r9; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R10");
        draw_banked_reg([&](const cpu::banked_regs::regs& regs) { return regs.r10; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R11");
        draw_banked_reg([&](const cpu::banked_regs::regs& regs) { return regs.r11; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R12");
        draw_banked_reg([&](const cpu::banked_regs::regs& regs) { return regs.r12; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R13");
        draw_banked_reg([&](const cpu::banked_regs::regs& regs) { return regs.r13; });
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); ImGui::TextUnformatted("R14");
        draw_banked_reg([&](const cpu::banked_regs::regs& regs) { return regs.r14; });
        ImGui::TableNextRow();

        const cpu::spsr_banks& spsr = access_private::spsr_banks_(c);
        ImGui::TableNextColumn(); ImGui::TextUnformatted("SPSR");
        /* no spsr in usr/sys */ ImGui::TableNextColumn();
        draw_psr(spsr[cpu::register_bank::fiq]);
        draw_psr(spsr[cpu::register_bank::svc]);
        draw_psr(spsr[cpu::register_bank::abt]);
        draw_psr(spsr[cpu::register_bank::irq]);
        draw_psr(spsr[cpu::register_bank::und]);

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

std::string_view to_string_view(const cpu::interrupt_source irq) noexcept
{
    switch(irq) {
        case cpu::interrupt_source::vblank: return "vblank";
        case cpu::interrupt_source::hblank: return "hblank";
        case cpu::interrupt_source::vcounter_match: return "vcounter_match";
        case cpu::interrupt_source::timer_0_overflow: return "timer_0_overflow";
        case cpu::interrupt_source::timer_1_overflow: return "timer_1_overflow";
        case cpu::interrupt_source::timer_2_overflow: return "timer_2_overflow";
        case cpu::interrupt_source::timer_3_overflow: return "timer_3_overflow";
        case cpu::interrupt_source::serial_io: return "serial_io";
        case cpu::interrupt_source::dma_0: return "dma_0";
        case cpu::interrupt_source::dma_1: return "dma_1";
        case cpu::interrupt_source::dma_2: return "dma_2";
        case cpu::interrupt_source::dma_3: return "dma_3";
        case cpu::interrupt_source::keypad: return "keypad";
        case cpu::interrupt_source::gamepak: return "gamepak";
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

std::string_view to_string_view(const cpu::debugger_access_width width) noexcept
{
    switch(width) {
        case cpu::debugger_access_width::byte: return "byte";
        case cpu::debugger_access_width::hword: return "hword";
        case cpu::debugger_access_width::word: return "word";
        case cpu::debugger_access_width::any: return "any";
        default:
            UNREACHABLE();
    }
}

} // namespace

cpu_debugger::cpu_debugger(cartridge::gamepak* gamepak, cpu::cpu* c, breakpoint_database* bp_db) noexcept
  : gamepak_{gamepak},
    dma_controller_{&access_private::dma_controller_(c)},
    timer_controller_{&access_private::timer_controller_(c)},
    cpu_{c},
    bp_db_{bp_db} {}

void cpu_debugger::draw() noexcept
{
    if(ImGui::Begin("CPU")) {
        if(ImGui::BeginTabBar("#cpu_tab")) {
            if(ImGui::BeginTabItem("ARM")) {
                draw_regs(cpu_); ImGui::SameLine();

                ImGui::BeginGroup(); {
                    ImGui::TextUnformatted("Pipeline");
                    ImGui::Separator();

                    regs_t& r = access_private::r_(cpu_);
                    const auto draw_pipeline_instr = [&](const char* name, const u32 instr, const u32 offset) {
                        ImGui::Text(access_private::cpsr_(cpu_).t ? "{}: {:04X}" : "{}: {:08X}", name, instr);
                        if(ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            if(access_private::cpsr_(cpu_).t) {
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
                        ImGui::BeginGroup(); {
                            draw_pipeline_instr("executing", access_private::pipeline_(cpu_).executing, 1_u32);
                            draw_pipeline_instr("decoding", access_private::pipeline_(cpu_).decoding, 2_u32);
                            ImGui::Text("fetch: {}", [&]() {
                                switch(access_private::pipeline_(cpu_).fetch_type) {
                                    case cpu::mem_access::non_seq: return "non seq";
                                    case cpu::mem_access::seq: return "seq";
                                    default: UNREACHABLE();
                                }
                            }());

                            ImGui::Spacing();
                            ImGui::Spacing();
                            ImGui::Spacing();

                            ImGui::Text("postboot: {:X}", access_private::post_boot_(cpu_).get());
                            ImGui::Text("haltcnt: {}", [&]() {
                                switch(access_private::haltcnt_(cpu_)) {
                                    case cpu::halt_control::halted: return "halted";
                                    case cpu::halt_control::stopped: return "stopped";
                                    case cpu::halt_control::running: return "running";
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
                                        ImGui::Text("{}: {}", to_string_view(to_enum<cpu::interrupt_source>(irq)), s);
                                    }
                                    ImGui::EndTooltip();
                                }
                            };

                            draw_irq_reg("ie", access_private::ie_(cpu_));
                            draw_irq_reg("if", access_private::if_(cpu_));
                            ImGui::Text("ime: {}", access_private::ime_(cpu_));
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
                            ImGui::Text("sram   {} ({})", ws_nonseq[access_private::waitcnt_(cpu_).sram], access_private::waitcnt_(cpu_).sram);
                            ImGui::Text("ws0 ns {} ({})", ws_nonseq[access_private::waitcnt_(cpu_).ws0_nonseq], access_private::waitcnt_(cpu_).ws0_nonseq);
                            ImGui::Text("ws0  s {} ({})", ws0_seq[access_private::waitcnt_(cpu_).ws0_seq], access_private::waitcnt_(cpu_).ws0_seq);
                            ImGui::Text("ws1 ns {} ({})", ws_nonseq[access_private::waitcnt_(cpu_).ws1_nonseq], access_private::waitcnt_(cpu_).ws1_nonseq);
                            ImGui::Text("ws1  s {} ({})", ws1_seq[access_private::waitcnt_(cpu_).ws1_seq], access_private::waitcnt_(cpu_).ws1_seq);
                            ImGui::Text("ws2 ns {} ({})", ws_nonseq[access_private::waitcnt_(cpu_).ws2_nonseq], access_private::waitcnt_(cpu_).ws2_nonseq);
                            ImGui::Text("ws2  s {} ({})", ws2_seq[access_private::waitcnt_(cpu_).ws2_seq], access_private::waitcnt_(cpu_).ws2_seq);
                            ImGui::Text("phi    {}", access_private::waitcnt_(cpu_).phi);
                            ImGui::Text("prefetch {}", access_private::waitcnt_(cpu_).prefetch_buffer_enable);
                            ImGui::EndGroup();
                        }
                        ImGui::EndGroup();
                    }

                    ImGui::Spacing();
                    ImGui::TextUnformatted("Prefetch Buffer");
                    ImGui::Separator();

                    auto& prefetch = access_private::prefetch_buffer_(cpu_);
                    ImGui::Text("active: {}", prefetch.active);
                    ImGui::Text("begin:  {:08X}", prefetch.begin);
                    ImGui::Text("end:    {:08X}", prefetch.end);
                    ImGui::Text("size:   {}/{} ({} byte instr)", prefetch.size,
                      prefetch.capacity, prefetch.addr_increment);
                    ImGui::Text("cycles: {}/{}", prefetch.cycles_left, prefetch.cycles_needed);

                    ImGui::EndGroup();
                }
                static bool show_banked_registers = false;
                ImGui::Checkbox("Show banked registers", &show_banked_registers);
                if(show_banked_registers) {
                    draw_banked_regs(cpu_);
                }

                auto& stall16 = access_private::stall_16_(cpu_);
                auto& stall32 = access_private::stall_32_(cpu_);
                static bool show_wait_table = false;
                ImGui::Checkbox("Show wait table", &show_wait_table);

                if(show_wait_table && ImGui::BeginTable("waittable", 17,
                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    ImGui::TableSetColumnIndex(0);

                    for(u32::type page : range(0x10)) {
                        ImGui::TableNextColumn();
                        ImGui::Text("{:02X}", page);
                        if(ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted([=]() {
                                switch(page) {
                                    case 0x00: return "bios";
                                    case 0x01: return "N/A";
                                    case 0x02: return "ewram";
                                    case 0x03: return "iwram";
                                    case 0x04: return "io";
                                    case 0x05: return "palette ram";
                                    case 0x06: return "vram";
                                    case 0x07: return "oam";
                                    case 0x08: return "pak0_1";
                                    case 0x09: return "pak0_2";
                                    case 0x0A: return "pak1_1";
                                    case 0x0B: return "pak1_2";
                                    case 0x0C: return "pak2_1";
                                    case 0x0D: return "pak2_2";
                                    case 0x0E: return "sram_1";
                                    case 0x0F: return "sram_2";
                                    default:
                                        UNREACHABLE();
                                }
                            }());
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("16n");
                    for(u32::type page : range(0x10)) {
                        ImGui::TableNextColumn();
                        ImGui::Text("{}", stall16[from_enum<u32>(cpu::mem_access::non_seq)][page]);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("16s");
                    for(u32::type page : range(0x10)) {
                        ImGui::TableNextColumn();
                        ImGui::Text("{}", stall16[from_enum<u32>(cpu::mem_access::seq)][page]);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("32n");
                    for(u32::type page : range(0x10)) {
                        ImGui::TableNextColumn();
                        ImGui::Text("{}", stall32[from_enum<u32>(cpu::mem_access::non_seq)][page]);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("32s");
                    for(u32::type page : range(0x10)) {
                        ImGui::TableNextColumn();
                        ImGui::Text("{}", stall32[from_enum<u32>(cpu::mem_access::seq)][page]);
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
                const auto fmt_channels = [](channels_debugger& channels) -> std::string {
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

                for(const u32 i : range(dma::channel_count)) {
                    dma::channel& channel = (*dma_controller_)[i];
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
        ImGui::SetNextItemWidth(250.f);
        ImGui::Combo("", &breakpoint_type, breakpoint_types.data(), breakpoint_types.size().get());

        ImGui::SameLine();

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

    static int hit_type = 1;
    ImGui::SetNextItemWidth(120.f);
    ImGui::Combo("type", &hit_type, bp_hit_types.data(), bp_hit_types.size().get());

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
            breakpoint.hit_type = static_cast<breakpoint_hit_type>(hit_type + 1);
            breakpoint.address = static_cast<u32::type>(std::strtoul(address_buf.data(), nullptr, 16));
            if(hit_count_target_buf != 0) {
                breakpoint.hit_count_target = static_cast<u32::type>(hit_count_target_buf);
                breakpoint.hit_type |= breakpoint_hit_type::suspend;
            }

            if(bp_db_->add_execution_breakpoint(breakpoint)) {
                hit_count_target_buf = 0;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    draw_bps(bp_db_->get_execution_breakpoints(), [](const execution_breakpoint& bp) {
        ImGui::Text("{:08X} | hit {}/{} | {}", bp.address,
          bp.hit_count, bp.hit_count_target, bp_hit_types[from_enum<u32>(bp.hit_type) - 1_u32]);
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

    static int hit_type = 1;
    ImGui::SetNextItemWidth(120.f);
    ImGui::Combo("type", &hit_type, bp_hit_types.data(), bp_hit_types.size().get());

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
            breakpoint.hit_type = static_cast<breakpoint_hit_type>(hit_type + 1);
            breakpoint.access_width = static_cast<cpu::debugger_access_width>(access_width);
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
            ImGui::Text("    {:08X}      | {:^5} | {:^10} | {:0X} | {}",
              bp.address_range.min(),
              to_string_view(bp.access_width), to_string_view(bp.access_type), bp.data,
              bp_hit_types[from_enum<u32>(bp.hit_type) - 1_u32]);
        } else {
            ImGui::Text("{:08X}-{:08X} | {:^5} | {:^10} | {:0X} | {}",
              bp.address_range.min(), bp.address_range.max(),
              to_string_view(bp.access_width), to_string_view(bp.access_type), bp.data,
              bp_hit_types[from_enum<u32>(bp.hit_type) - 1_u32]);
        }
    });
}

void cpu_debugger::draw_disassembly() noexcept
{
    if(ImGui::BeginChild("#armdisassemblychild")) {
        vector<u8>* memory;
        const u32 pc = access_private::r_(cpu_)[15_u32];
        u32 offset;
        u32 mask = access_private::mirror_mask_(gamepak_);
        if(pc < 0x0000'3FFF_u32) {
            memory = &access_private::bios_(cpu_);
            mask = 0x0000'3FFF_u32;
        } else if(pc < 0x0300'0000_u32) {
            memory = &access_private::wram_(cpu_);
            offset = 0x0200'0000_u32;
            mask = 0x0003'FFFF_u32;
        } else if(pc < 0x0400'0000_u32) {
            memory = &access_private::iwram_(cpu_);
            offset = 0x0300'0000_u32;
            mask = 0x0000'7FFF_u32;
        } else if(pc < 0x0A00'0000_u32) {
            memory = &access_private::pak_data_(gamepak_);
            offset = 0x0800'0000_u32;
        } else if(pc < 0x0C00'0000_u32) {
            memory = &access_private::pak_data_(gamepak_);
            offset = 0x0A00'0000_u32;
        } else if(pc < 0x0E00'0000_u32) {
            memory = &access_private::pak_data_(gamepak_);
            offset = 0x0C00'0000_u32;
        } else {
            // probably something is broken at this point
            // or we're execution in vram, which is very unlikely
            // don't think we'll support disassembly for it
            LOG_WARN(debugger, "unknown execution page {:02X}", pc >> 24_u32);
            return;
        }

        const u32 pc_physical_address = pc & mask;

        const u32 instr_width = access_private::cpsr_(cpu_).t ? 2_u32 : 4_u32;
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

            const bool is_thumb = access_private::cpsr_(cpu_).t;
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
