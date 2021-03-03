/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/arm_debugger.h>

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
            ImGui::Text("%08X", reg.get());
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

} // namespace

void arm_debugger::draw() const noexcept
{
    if(ImGui::Begin("ARM")) {
        if(ImGui::BeginTabBar("#arm_tab")) {
            if(ImGui::BeginTabItem("Registers")) {
                draw_regs(arm);

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::TextUnformatted("Pipeline");
                ImGui::Separator();
                const auto draw_pipeline_instr = [&](const char* name, const u32 instr, const u32 offset) {
                    ImGui::Text("%s: %08X", name, instr.get());
                    if(ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        if(access_private::cpsr_(arm).t) {
                            ImGui::TextUnformatted(disassembler::disassemble_thumb(
                              access_private::r15_(arm) - offset * 4_u32,
                              narrow<u16>(instr)).c_str());
                        } else {
                            ImGui::TextUnformatted(disassembler::disassemble_arm(
                              access_private::r15_(arm) - offset * 2_u32,
                              instr).c_str());
                        }
                        ImGui::EndTooltip();
                    }
                };

                ImGui::BeginGroup(); {
                    draw_pipeline_instr("executing", access_private::pipeline_(arm).executing, 1_u32);
                    draw_pipeline_instr("decoding", access_private::pipeline_(arm).decoding, 2_u32);
                    ImGui::Text("fetch: %s", [&]() {
                        switch(access_private::pipeline_(arm).fetch_type) {
                            case arm::mem_access::non_seq: return "non seq";
                            case arm::mem_access::seq: return "seq";
                            default: UNREACHABLE();
                        }
                    }());

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::Text("postboot: %X", access_private::post_boot_(arm).get());
                    ImGui::Text("haltcnt: %s", [&]() {
                        switch(access_private::haltcnt_(arm)) {
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
                        ImGui::Text("ie: %04X", reg.get());
                        if(ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            for(u16 idx : range(14_u16)) {
                                u16 irq = 1_u16 << idx;
                                bool s = (reg & irq) == irq;
                                ImGui::Text("%s: %s",
                                  to_string_view(to_enum<arm::interrupt_source>(irq)).data(),
                                  fmt_bool(s));
                            }
                            ImGui::EndTooltip();
                        }
                    };

                    draw_irq_reg(access_private::ie_(arm));
                    draw_irq_reg(access_private::if_(arm));
                    ImGui::Text("ime: %s", fmt_bool(access_private::ime_(arm)));
                    ImGui::EndGroup();
                }

                ImGui::SameLine(0.f, 75.f);

                ImGui::BeginGroup(); {
                    ImGui::TextUnformatted("waitcnt");
                    ImGui::Separator();
                    ImGui::Text("sram %d", access_private::waitcnt_(arm).sram.get());
                    ImGui::Text("ws0_nonseq %d", access_private::waitcnt_(arm).ws0_nonseq.get());
                    ImGui::Text("ws0_seq %d", access_private::waitcnt_(arm).ws0_seq.get());
                    ImGui::Text("ws1_nonseq %d", access_private::waitcnt_(arm).ws1_nonseq.get());
                    ImGui::Text("ws1_seq %d", access_private::waitcnt_(arm).ws1_seq.get());
                    ImGui::Text("ws2_nonseq %d", access_private::waitcnt_(arm).ws2_nonseq.get());
                    ImGui::Text("ws2_seq %d", access_private::waitcnt_(arm).ws2_seq.get());
                    ImGui::Text("phi %d", access_private::waitcnt_(arm).phi.get());
                    ImGui::Text("prefetch %s", fmt_bool(access_private::waitcnt_(arm).prefetch_buffer_enable));
                    ImGui::EndGroup();
                }

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Timers")) {
                for(auto& timer : access_private::timers_(arm)) {
                    ImGui::Text("Timer %d", access_private::id_(timer).get());
                    ImGui::Separator();
                    ImGui::Text("counter: %04X reload: %04X",
                      access_private::counter_(timer).get(),
                      access_private::reload_(timer).get());

                    auto& cnt = access_private::control_(timer);
                    constexpr array prescalar_shifts{0_u8, 6_u8, 8_u8, 10_u8};

                    ImGui::Text("prescalar: F/%d", 1_u32 << prescalar_shifts[cnt.prescalar]);
                    ImGui::Text("cascaded: %s", fmt_bool(cnt.cascaded));
                    ImGui::Text("irq: %s", fmt_bool(cnt.irq_enabled));
                    ImGui::Text("enabled: %s", fmt_bool(cnt.enabled));

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
                        s << delim << c;
                        delim = ", ";
                    }
                    return s.str();
                };

                auto& controller = access_private::dma_controller_(arm);

                ImGui::Text("Scheduled channels: %s",
                  fmt_channels(access_private::scheduled_channels_(controller)).c_str());
                ImGui::Text("Running channels: %s",
                  fmt_channels(access_private::running_channels_(controller)).c_str());

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Spacing();

                for(dma::channel& channel : controller.channels) {
                    ImGui::Text("DMA %d", channel.id.get());
                    ImGui::Separator();
                    ImGui::BeginGroup(); {
                        ImGui::Text("enabled: %s", fmt_bool(channel.cnt.enabled));
                        ImGui::Text("repeat: %s", fmt_bool(channel.cnt.repeat));
                        ImGui::Text("irq: %s", fmt_bool(channel.cnt.irq));
                        ImGui::Text("drq: %s", fmt_bool(channel.cnt.drq));
                        ImGui::Text("dst control: %s", to_string_view(channel.cnt.dst_control).data());
                        ImGui::Text("src control: %s", to_string_view(channel.cnt.src_control).data());
                        ImGui::Text("timing: %s", [&]() {
                            switch(channel.cnt.when) {
                                case dma::channel::control::timing::immediately: return "immediately";
                                case dma::channel::control::timing::vblank: return "vblank";
                                case dma::channel::control::timing::hblank: return "hblank";
                                case dma::channel::control::timing::special: return "special";
                                default:
                                    UNREACHABLE();
                            }
                        }());
                        ImGui::Text("size: %s", [&]() {
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
                        ImGui::Text("src: %08X", channel.src.get());
                        ImGui::Text("dst: %08X", channel.dst.get());
                        ImGui::Text("count: %05X", channel.count.get());

                        ImGui::Spacing();
                        ImGui::Spacing();

                        ImGui::TextUnformatted("Internal Data"); ImGui::Separator();
                        ImGui::Text("src: %08X", channel.internal.src.get());
                        ImGui::Text("dst: %08X", channel.internal.dst.get());
                        ImGui::Text("count: %05X", channel.internal.count.get());
                        ImGui::EndGroup();
                    }

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Spacing();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::End();

    ImGui::Begin("diss");
    vector<u8>* memory;
    unsigned int offset = 0;
    unsigned int pc;
    if(auto p = access_private::r15_(arm).get(); p < 0x3FFF) {
        memory = &access_private::bios_(arm);
        pc = p;
    } else if(p < 0x0203FFFF) {
        memory = &access_private::wram_(arm);
        pc = p - 0x02000000;
        offset = 0x02000000;
    } else if(p < 0x03007FFF) {
        memory = &access_private::iwram_(arm);
        pc = p - 0x03000000;
        offset = 0x03000000;
    } else if(p < 0x09FFFFFF) {
        memory = &access_private::pak_data_(access_private::core_(arm)->pak);
        pc = p - 0x08000000;
        offset = 0x08000000;
    } else if(p < 0x0BFFFFFF) {
        memory = &access_private::pak_data_(access_private::core_(arm)->pak);
        pc = p - 0x0A000000;
        offset = 0x08000000;
    } else if(p < 0x0DFFFFFF) {
        memory = &access_private::pak_data_(access_private::core_(arm)->pak);
        pc = p - 0x0C000000;
        offset = 0x08000000;
    } else {
        UNREACHABLE();
    }

    auto mult = access_private::cpsr_(arm).t ? 2u : 4u;
    auto i = pc / mult;
    if(i < 9) {
        i = 0;
    } else {
        i -= 9;
    }

    auto max = std::min(15ull + i, memory->size().get() / mult);
    for(; i < max; ++i) {
        if(access_private::cpsr_(arm).t) {
            const auto address = 2_u32 * i;
            u16 inst = memcpy<u16>(*memory, address);
            if(address == pc - 4) {
                ImGui::TextColored(ImColor(0xFF0000FF), "0x%08X|0x%04X %s", address + offset, inst.get(), disassembler::disassemble_thumb(address, inst).c_str());
            } else {
                ImGui::Text("0x%08X|0x%04X %s", address + offset, inst.get(), disassembler::disassemble_thumb(address, inst).c_str());
            }
        } else {
            const auto address = 4_u32 * i;
            u32 inst = memcpy<u32>(*memory, address);
            if(address == pc - 8) {
                ImGui::TextColored(ImColor(0xFF0000FF), "0x%08X|0x%08X %s", address + offset, inst.get(), disassembler::disassemble_arm(address, inst).c_str());
            } else {
                ImGui::Text("0x%08X|0x%08X %s", address + offset, inst.get(), disassembler::disassemble_arm(address, inst).c_str());
            }
        }
    }

    ImGui::End();
}

} // namespace gba::debugger
