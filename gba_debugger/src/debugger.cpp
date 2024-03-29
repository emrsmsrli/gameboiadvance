/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/debugger.h>

#include <chrono>
#include <string_view>
#include <thread>

#include <access_private.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <imgui_filebrowser/imfilebrowser.h>
#include <imgui_internal.h>
#include <implot.h>
#include <SFML/OpenGL.hpp>

#include <gba/core.h>
#include <gba_debugger/debugger_helpers.h>

ACCESS_PRIVATE_FIELD(gba::debugger::window, gba::debugger::preferences, prefs_)
ACCESS_PRIVATE_FIELD(gba::debugger::window, gba::core*, core_)

ACCESS_PRIVATE_FIELD(gba::core, gba::scheduler, scheduler_)
ACCESS_PRIVATE_FIELD(gba::core, gba::cartridge::gamepak, gamepak_)
ACCESS_PRIVATE_FIELD(gba::core, gba::cpu::cpu, cpu_)
ACCESS_PRIVATE_FIELD(gba::core, gba::ppu::engine, ppu_engine_)
ACCESS_PRIVATE_FIELD(gba::core, gba::apu::engine, apu_engine_)
ACCESS_PRIVATE_FIELD(gba::core, gba::keypad::keypad, keypad_)

ACCESS_PRIVATE_FIELD(gba::cpu::arm7tdmi, gba::cpu::psr, cpsr_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::vector<gba::u8>, wram_)
ACCESS_PRIVATE_FIELD(gba::cpu::cpu, gba::vector<gba::u8>, iwram_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::vector<gba::u8>, pak_data_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, std::unique_ptr<gba::cartridge::backup>, backup_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, palette_ram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, vram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, oam_)
ACCESS_PRIVATE_FIELD(gba::scheduler, gba::vector<gba::scheduler::hw_event>, heap_)

namespace gba::debugger {

namespace {

ImGui::FileBrowser rom_picker;

std::string_view to_string_view(const cpu::debugger_access_width access_type) noexcept
{
    switch(access_type) {
        case cpu::debugger_access_width::byte: return "byte";
        case cpu::debugger_access_width::hword: return "hword";
        case cpu::debugger_access_width::word: return "word";
        default: UNREACHABLE();
    }
}

void debugger_settings_hander_clear_all(ImGuiContext*, ImGuiSettingsHandler* handler)
{
    auto& prefs = access_private::prefs_(static_cast<window*>(handler->UserData));
    prefs = preferences{};
}

void* debugger_settings_hander_read_open(ImGuiContext*, ImGuiSettingsHandler* handler, const char*) { return handler; }

void debugger_settings_hander_read_line(ImGuiContext*, ImGuiSettingsHandler* handler, void*, const char* line)
{
    auto& prefs = access_private::prefs_(static_cast<window*>(handler->UserData));
    int dummy1;
    int dummy2;
    int dummy3;
    array<float, 4> dummy4{1.f, 1.f, 1.f, 1.f};
         if(sscanf(line, "ppu_framebuffer_render_scale=%i", &dummy1) == 1) { prefs.ppu_framebuffer_render_scale = dummy1; }
    else if(sscanf(line, "bg0=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs.ppu_bg_prefs[0_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "bg1=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs.ppu_bg_prefs[1_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "bg2=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs.ppu_bg_prefs[2_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "bg3=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs.ppu_bg_prefs[3_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "ppu_bg_tiles_render_scale=%i", &dummy1) == 1) { prefs.ppu_bg_tiles_render_scale = dummy1; }
    else if(sscanf(line, "ppu_obj_tiles_render_scale=%i", &dummy1) == 1) { prefs.ppu_obj_tiles_render_scale = dummy1; }
    else if(sscanf(line, "ppu_win_render_scale=%i", &dummy1) == 1) { prefs.ppu_win_render_scale = dummy1; }
    else if(sscanf(line, "ppu_winout_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs.ppu_winout_color = dummy4; }
    else if(sscanf(line, "ppu_win0_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs.ppu_win0_color = dummy4; }
    else if(sscanf(line, "ppu_win1_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs.ppu_win1_color = dummy4; }
    else if(sscanf(line, "ppu_winobj_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs.ppu_winobj_color = dummy4; }

    else if(sscanf(line, "apu_audio_enabled=%i", &dummy1) == 1) { prefs.apu_audio_enabled = dummy1; }
    else if(sscanf(line, "apu_audio_volume=%f", dummy4.ptr(0_u32)) == 1) { prefs.apu_audio_volume = dummy4.front(); }
    else if(sscanf(line, "apu_enabled_channel_graphs=%i", &dummy1) == 1) { prefs.apu_enabled_channel_graphs = dummy1; }

    else if(sscanf(line, "debugger_background_emulate=%i", &dummy1) == 1) { prefs.debugger_background_emulate = dummy1 == 1; }
    else if(sscanf(line, "debugger_bios_skip=%i", &dummy1) == 1) { prefs.debugger_bios_skip = dummy1 == 1; }
    else if(sscanf(line, "debugger_framerate=%i", &dummy1) == 1) { prefs.debugger_framerate = dummy1; }
}

void debugger_settings_hander_write_all(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
{
    auto& prefs = access_private::prefs_(static_cast<window*>(handler->UserData));
    out_buf->appendf("[%s][%s]\n", handler->TypeName, handler->TypeName);
    out_buf->appendf("ppu_framebuffer_render_scale=%i\n", prefs.ppu_framebuffer_render_scale);
    out_buf->appendf("bg0=%i,%i,%i\n",
        prefs.ppu_bg_prefs[0_u32].enable_visible_area,
        prefs.ppu_bg_prefs[0_u32].enable_visible_area_border,
        prefs.ppu_bg_prefs[0_u32].render_scale);
    out_buf->appendf("bg1=%i,%i,%i\n",
        prefs.ppu_bg_prefs[1_u32].enable_visible_area,
        prefs.ppu_bg_prefs[1_u32].enable_visible_area_border,
        prefs.ppu_bg_prefs[1_u32].render_scale);
    out_buf->appendf("bg2=%i,%i,%i\n",
        prefs.ppu_bg_prefs[2_u32].enable_visible_area,
        prefs.ppu_bg_prefs[2_u32].enable_visible_area_border,
        prefs.ppu_bg_prefs[2_u32].render_scale);
    out_buf->appendf("bg3=%i,%i,%i\n",
        prefs.ppu_bg_prefs[3_u32].enable_visible_area,
        prefs.ppu_bg_prefs[3_u32].enable_visible_area_border,
        prefs.ppu_bg_prefs[3_u32].render_scale);
    out_buf->appendf("ppu_bg_tiles_render_scale=%i\n", prefs.ppu_bg_tiles_render_scale);
    out_buf->appendf("ppu_obj_tiles_render_scale=%i\n", prefs.ppu_obj_tiles_render_scale);
    out_buf->appendf("ppu_win_render_scale=%i\n", prefs.ppu_win_render_scale);
    out_buf->appendf("ppu_winout_color=%f,%f,%f\n", prefs.ppu_winout_color[0_u32], prefs.ppu_winout_color[1_u32], prefs.ppu_winout_color[2_u32]);
    out_buf->appendf("ppu_win0_color=%f,%f,%f\n", prefs.ppu_win0_color[0_u32], prefs.ppu_win0_color[1_u32], prefs.ppu_win0_color[2_u32]);
    out_buf->appendf("ppu_win1_color=%f,%f,%f\n", prefs.ppu_win1_color[0_u32], prefs.ppu_win1_color[1_u32], prefs.ppu_win1_color[2_u32]);
    out_buf->appendf("ppu_winobj_color=%f,%f,%f\n", prefs.ppu_winobj_color[0_u32], prefs.ppu_winobj_color[1_u32], prefs.ppu_winobj_color[2_u32]);

    out_buf->appendf("apu_audio_enabled=%d\n", prefs.apu_audio_enabled);
    out_buf->appendf("apu_audio_volume=%f\n", prefs.apu_audio_volume);
    out_buf->appendf("apu_enabled_channel_graphs=%i\n", prefs.apu_enabled_channel_graphs);

    out_buf->appendf("debugger_background_emulate=%i\n", prefs.debugger_background_emulate);
    out_buf->appendf("debugger_bios_skip=%i\n", prefs.debugger_bios_skip);
    out_buf->appendf("debugger_framerate=%i\n", prefs.debugger_framerate);
}

void debugger_settings_hander_apply_all(ImGuiContext*, ImGuiSettingsHandler* handler)
{
    const auto* w = static_cast<window*>(handler->UserData);
    const preferences& prefs = access_private::prefs_(w);

    access_private::core_(w)->set_volume(prefs.apu_audio_volume);
    if(prefs.debugger_bios_skip) {
        access_private::core_(w)->skip_bios();
    }
}

} // namespace

window::window(core* core) noexcept
  : window_{sf::VideoMode{1920, 1080}, "GBA Debugger"},
    window_event_{},
    audio_device_{2, sdl::audio_device::format::f32, 48000, 2048},
    core_{core},
    cpu_{&access_private::cpu_(core)},
    scheduler_{&access_private::scheduler_(core)},
    disassembly_view_{&breakpoint_database_},
    gamepak_debugger_{&access_private::gamepak_(core)},
    cpu_debugger_{&access_private::gamepak_(core), cpu_, &breakpoint_database_},
    ppu_debugger_{&access_private::ppu_engine_(core), &prefs_},
    apu_debugger_{&access_private::apu_engine_(core), &prefs_},
    keypad_debugger_{&access_private::keypad_(core)}
{
    generate_memory_debugger_entries();

    window_.resetGLStates();
    set_framerate_limit(framerate_limit::vsync);
    ImGui::SFML::Init(window_, true);

    ImGuiSettingsHandler debugger_settings_handler;
    debugger_settings_handler.UserData = this;
    debugger_settings_handler.TypeName = "DebuggerSettings";
    debugger_settings_handler.TypeHash = ImHashStr("DebuggerSettings");
    debugger_settings_handler.ReadOpenFn = debugger_settings_hander_read_open;
    debugger_settings_handler.ClearAllFn = debugger_settings_hander_clear_all;
    debugger_settings_handler.ReadLineFn = debugger_settings_hander_read_line;
    debugger_settings_handler.ApplyAllFn = debugger_settings_hander_apply_all;
    debugger_settings_handler.WriteAllFn = debugger_settings_hander_write_all;
    ImGui::GetCurrentContext()->SettingsHandlers.push_back(debugger_settings_handler);

    audio_device_.resume();

    const fs::path& gamepak_path = access_private::gamepak_(core).path();
    rom_picker.SetPwd(fs::is_directory(gamepak_path) ? gamepak_path : gamepak_path.parent_path());
    rom_picker.SetTypeFilters({".gba", ".gz"});
    rom_picker.SetTitle("Pick ROM file");

    [[maybe_unused]] const sf::ContextSettings& settings = window_.getSettings();
    LOG_INFO(debugger, "OpenGL {}.{}, attr: 0x{:X}", settings.majorVersion, settings.minorVersion, settings.attributeFlags);
    LOG_INFO(debugger, "{} {} {}", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    core_->on_io_read.connect<&window::on_io_read>(this);
    core_->on_io_write.connect<&window::on_io_write>(this);
    core_->on_scanline_event().add_delegate({connect_arg<&window::on_scanline>, this});
    core_->on_vblank_event().add_delegate({connect_arg<&window::on_vblank>, this});
    core_->sound_buffer_overflow_event().add_delegate({connect_arg<&window::on_audio_buffer_full>, this});
    cpu_->on_instruction_execute.connect<&window::on_instruction_execute>(this);

    core_->set_dst_sample_rate(audio_device_.frequency());
    core_->set_sound_buffer_capacity(audio_device_.sample_count());
    apu_debugger_.set_buffer_capacity(audio_device_.sample_count());
    cpu_debugger_.on_execution_requested.add_delegate({connect_arg<&window::on_execution_requested>, this});
}

bool window::draw() noexcept
{
    if(!core_->pak_loaded() && !rom_picker.IsOpened()) {
        rom_picker.Open();
    }

    while(window_.pollEvent(window_event_)) {
        if(window_event_.type == sf::Event::Closed) {
            ImGui::SaveIniSettingsToDisk(ImGui::GetCurrentContext()->IO.IniFilename);
            return false;
        }

        ImGui::SFML::ProcessEvent(window_event_);

        if(window_event_.type == sf::Event::KeyPressed) {
            switch(window_event_.key.code) {
                case sf::Keyboard::W: if(tick_allowed_) { core_->press_key(keypad::key::up); } break;
                case sf::Keyboard::A: if(tick_allowed_) { core_->press_key(keypad::key::left); } break;
                case sf::Keyboard::S: if(tick_allowed_) { core_->press_key(keypad::key::down); } break;
                case sf::Keyboard::D: if(tick_allowed_) { core_->press_key(keypad::key::right); } break;
                case sf::Keyboard::K: if(tick_allowed_) { core_->press_key(keypad::key::b); } break;
                case sf::Keyboard::O: if(tick_allowed_) { core_->press_key(keypad::key::a); } break;
                case sf::Keyboard::B: if(tick_allowed_) { core_->press_key(keypad::key::select); } break;
                case sf::Keyboard::N: if(tick_allowed_) { core_->press_key(keypad::key::start); } break;
                case sf::Keyboard::T: if(tick_allowed_) { core_->press_key(keypad::key::left_shoulder); } break;
                case sf::Keyboard::U: if(tick_allowed_) { core_->press_key(keypad::key::right_shoulder); } break;
                case sf::Keyboard::F7: if(core_->pak_loaded()) { cpu_->tick(); } break;
                default:
                    break;
            }
        } else if(window_event_.type == sf::Event::KeyReleased) {
            const auto save_load_state = [&](const state_slot slot) {
                if(window_event_.key.control) {
                    core_->load_state(slot);
                } else {
                    core_->save_state(slot);
                }
            };

            const auto try_close_rom_picker = [&]() { if(core_->pak_loaded()) { rom_picker.Close(); } };

            switch(window_event_.key.code) {
                case sf::Keyboard::W: if(tick_allowed_) { core_->release_key(keypad::key::up); } break;
                case sf::Keyboard::A: if(tick_allowed_) { core_->release_key(keypad::key::left); } break;
                case sf::Keyboard::S: if(tick_allowed_) { core_->release_key(keypad::key::down); } break;
                case sf::Keyboard::D: if(tick_allowed_) { core_->release_key(keypad::key::right); } break;
                case sf::Keyboard::K: if(tick_allowed_) { core_->release_key(keypad::key::b); } break;
                case sf::Keyboard::O: if(tick_allowed_) { core_->release_key(keypad::key::a); } break;
                case sf::Keyboard::B: if(tick_allowed_) { core_->release_key(keypad::key::select); } break;
                case sf::Keyboard::N: if(tick_allowed_) { core_->release_key(keypad::key::start); } break;
                case sf::Keyboard::T: if(tick_allowed_) { core_->release_key(keypad::key::left_shoulder); } break;
                case sf::Keyboard::U: if(tick_allowed_) { core_->release_key(keypad::key::right_shoulder); } break;
                case sf::Keyboard::R: reset_core(); break;
                case sf::Keyboard::F9:
                    if(core_->pak_loaded()) {
                        tick_allowed_ = !tick_allowed_;
                        if(!tick_allowed_) {
                            set_framerate_limit(framerate_limit::vsync);
                        } else {
                            set_framerate_limit(static_cast<framerate_limit>(prefs_.debugger_framerate));
                        }
                    }
                    break;
                case sf::Keyboard::F1: save_load_state(state_slot::slot1); break;
                case sf::Keyboard::F2: save_load_state(state_slot::slot2); break;
                case sf::Keyboard::F3: save_load_state(state_slot::slot3); break;
                case sf::Keyboard::F4: save_load_state(state_slot::slot4); break;
                case sf::Keyboard::F5: save_load_state(state_slot::slot5); break;
                case sf::Keyboard::Tab:
                    if(window_event_.key.control) {
                        if(rom_picker.IsOpened()) {
                            try_close_rom_picker();
                        } else {
                            rom_picker.Open();
                        }
                    }
                    break;
                case sf::Keyboard::Escape: if(rom_picker.IsOpened()) { try_close_rom_picker(); } break;
                default:
                    break;
            }
        }
    }

    if((!tick_allowed_ || !prefs_.debugger_background_emulate) && !window_.hasFocus()) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
        return true;
    }

    const u64 until = scheduler_->now() + ppu::engine::cycles_per_frame;
    while(tick_allowed_ && scheduler_->now() < until) {
        cpu_->tick();
    }

    const sf::Time dt = dt_.restart();
    ImGui::SFML::Update(window_, dt);

    disassembly_view_.draw_with_mode(access_private::cpsr_(cpu_).t);
    memory_view_.draw();
    gamepak_debugger_.draw();
    cpu_debugger_.draw();
    ppu_debugger_.draw();
    apu_debugger_.draw();
    keypad_debugger_.draw();

    if(ImGui::Begin("Scheduler", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        vector<scheduler::hw_event> events = access_private::heap_(scheduler_);
        std::sort(events.begin(), events.end(), [](const scheduler::hw_event& e1, const scheduler::hw_event& e2) {
            return e1.timestamp < e2.timestamp;
        });

        ImGui::Text("Cycles: {}", scheduler_->now());
        ImGui::Text("Scheduled event count: {}", events.size());

        ImGui::Spacing();
        ImGui::Spacing();

        const hw_event_registry& registry = hw_event_registry::get();
        for(const scheduler::hw_event& event : events) {
            const hw_event_registry::entry* entry = registry.find_by_callback(event.callback);
            ImGui::Text("{}, timestamp: {} ({})", entry ? entry->name : "????",
              event.timestamp, event.timestamp - scheduler_->now());
        }
    }

    ImGui::End();

    if(ImGui::Begin("Debugger", nullptr, ImGuiWindowFlags_MenuBar)) {
        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Open", "CTRL+TAB")) {
                    rom_picker.Open();
                }
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Settings")) {
                const auto draw_state_menu = [](const char* name, const char* shortcut, auto&& on_select) {
                    if (ImGui::BeginMenu(name)) {
                        for(const state_slot slot : range(state_slot::max)) {
                            const u32 slot_idx = from_enum<u32>(slot);
                            if(slot_idx == 0_u32) { continue; }

                            const std::string menu_label = fmt::format("Slot {}", slot_idx);
                            const std::string menu_shortcut = fmt::format("{}{}", shortcut, slot_idx);
                            if(ImGui::MenuItem(menu_label.c_str(), menu_shortcut.c_str())) {
                                on_select(slot);
                            }
                        }
                        ImGui::EndMenu();
                    }
                };

                draw_state_menu("Save state", "F", [&](const state_slot slot) {
                    core_->save_state(slot);
                });
                draw_state_menu("Load state", "CTRL+F", [&](const state_slot slot) {
                    core_->load_state(slot);
                });

                ImGui::PushItemWidth(150.f);

                ImGui::Checkbox("Audio Enable", &prefs_.apu_audio_enabled);
                ImGui::BeginDisabled(!prefs_.apu_audio_enabled);
                if(ImGui::SliderFloat("Volume", &prefs_.apu_audio_volume, 0.f, 1.f, "%.1f")) {
                    core_->set_volume(prefs_.apu_audio_volume);
                }
                ImGui::EndDisabled();

                static array framerate_names{"unlimited", "30", "60", "120", "144", "vsync"};
                if(ImGui::Combo("Framerate", &prefs_.debugger_framerate, framerate_names.data(), framerate_names.size().get())) {
                    set_framerate_limit(static_cast<framerate_limit>(prefs_.debugger_framerate));

                    total_frames_ = 0_usize;
                    total_frame_time_ = 0.f;
                }
                ImGui::PopItemWidth();

                ImGui::MenuItem("Allow emulation when unfocused", nullptr, &prefs_.debugger_background_emulate);
                ImGui::MenuItem("Skip bios on start", nullptr, &prefs_.debugger_bios_skip);

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::Text("audio device id: {}\nsample rate: {}", audio_device_.id(), audio_device_.frequency());
        ImGui::Text("{}\n {}\n {}", glGetString(GL_RENDERER), glGetString(GL_VENDOR), glGetString(GL_VERSION));

        const usize frame_time_idx = total_frames_ % frame_time_history_.size();
        frame_time_history_[frame_time_idx] = dt.asSeconds();

        ImGui::Text("frame count: {}", total_frames_);
        ImGui::Text("instruction count: {}", total_instructions_); ImGui::SameLine();
        if(ImGui::Button("reset")) {
            total_instructions_ = 0_usize;
        }
        ImGui::Text("current FPS: {}", 1.f / dt.asSeconds());
        ImGui::Text("current frametime: {}", dt.asSeconds());
        ImGui::Text("avg frametime: {}", total_frames_ == 0_usize
          ? 0.f
          : total_frame_time_ / total_frames_.get());

        ImPlot::SetNextPlotLimitsY(-.0075, .03f, ImGuiCond_Always);
        if(ImPlot::BeginPlot("Frametime", nullptr, nullptr, ImVec2{300.f, 150.f},
          ImPlotFlags_NoMenus | ImPlotFlags_NoLegend, ImPlotAxisFlags_RangeFit | ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_Lock)) {
            ImPlot::PlotLine("frametime_line", frame_time_history_.data(), frame_time_history_.size().get(),
              1.f, 0.f, frame_time_idx.get());
            const float ideal_frame_time = 1.f / 60.f;
            ImPlot::PlotHLines("ideal_frame_time", &ideal_frame_time, 1);
            ImPlot::EndPlot();
        }
    }

    ImGui::End();

    rom_picker.Display();

    if(rom_picker.HasSelected()) {
        reset_core();
        core_->load_pak(rom_picker.GetSelected());
        generate_memory_debugger_entries();

        rom_picker.ClearSelected();
        rom_picker.Close();
    }

    window_.clear(sf::Color::Black);
    ImGui::SFML::Render();
    window_.display();
    return true;
}

bool window::on_instruction_execute(const u32 address) noexcept
{
    ++total_instructions_;

    bool should_break = last_executed_addr_ != address;
    last_executed_addr_ = address;

    if(execution_request_ == cpu_debugger::execution_request::instruction) {
        tick_allowed_ = false;
        execution_request_ = cpu_debugger::execution_request::none;
        return false;
    }

    execution_breakpoint* exec_bp = breakpoint_database_.get_execution_breakpoint(address);
    if(!exec_bp || !exec_bp->enabled || !should_break) {
        return false;
    }

    ++exec_bp->hit_count;
    if(bitflags::is_set(exec_bp->hit_type, breakpoint_hit_type::log)) {
        LOG_INFO(debugger, "execution breakpoint hit: {:08X}", address);
    }

    if(!tick_allowed_) {
        return false;
    }

    if(exec_bp->hit_count_target.has_value()) {
        should_break = *exec_bp->hit_count_target == exec_bp->hit_count;
    } else {
        should_break = false;
    }

    should_break |= bitflags::is_set(exec_bp->hit_type, breakpoint_hit_type::suspend);
    if(should_break) {
        tick_allowed_ = false;
    }

    return should_break;
}

void window::on_io_read(const u32 address, const cpu::debugger_access_width access_type) noexcept
{
    if(tick_allowed_) {
        const access_breakpoint* access_bp = breakpoint_database_.get_enabled_read_breakpoint(address, access_type);
        if(!access_bp) {
            return;
        }

        if(bitflags::is_set(access_bp->hit_type, breakpoint_hit_type::log)) {
            LOG_INFO(debugger, "read breakpoint hit: {:08X}, {} access", address, to_string_view(access_type));
        }

        if(bitflags::is_set(access_bp->hit_type, breakpoint_hit_type::suspend)) {
            tick_allowed_ = false;
        }
    }
}

void window::on_io_write(const u32 address, const u32 data, const cpu::debugger_access_width access_type) noexcept
{
    if(tick_allowed_) {
        const access_breakpoint* access_bp = breakpoint_database_.get_enabled_write_breakpoint(address, data, access_type);
        if(!access_bp) {
            return;
        }

        if(bitflags::is_set(access_bp->hit_type, breakpoint_hit_type::log)) {
            LOG_INFO(debugger, "write breakpoint hit: {:08X} <- {:0X}, {} access",
              address, data, to_string_view(access_type));
        }

        if(bitflags::is_set(access_bp->hit_type, breakpoint_hit_type::suspend)) {
            tick_allowed_ = false;
        }
    }
}

void window::on_execution_requested(const cpu_debugger::execution_request request) noexcept
{
    if(execution_request_ != cpu_debugger::execution_request::none) {
        return;
    }

    execution_request_ = request;
    tick_allowed_ = true;
}

void window::on_scanline(u8, const ppu::scanline_buffer&) noexcept
{
    if(execution_request_ == cpu_debugger::execution_request::scanline) {
        tick_allowed_ = false;
        execution_request_ = cpu_debugger::execution_request::none;
    }
}

void window::on_vblank() noexcept
{
    if(const float frame_time = frame_dt_.restart().asSeconds(); frame_time < .3f) {
        ++total_frames_;
        total_frame_time_ += frame_time;
    }

    if(execution_request_ == cpu_debugger::execution_request::frame) {
        tick_allowed_ = false;
        execution_request_ = cpu_debugger::execution_request::none;
    }
}

void window::on_audio_buffer_full(const vector<apu::stereo_sample<float>>& buffer) noexcept
{
    if(!prefs_.apu_audio_enabled) {
        return;
    }

    const usize buffer_size_in_bytes = sizeof(apu::stereo_sample<float>) * buffer.size();
    audio_device_.enqueue(reinterpret_cast<const void*>(buffer.data()), buffer_size_in_bytes.get()); // NOLINT
    while(audio_device_.queue_size() > buffer_size_in_bytes) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }
}

void window::on_eeprom_bus_width_detected() noexcept
{
    cartridge::gamepak& pak = access_private::gamepak_(core_);
    memory_view_.add_entry(memory_view_entry{
      "EEPROM",
      view<u8>{
        access_private::backup_(pak)->data().data(),
        access_private::backup_(pak)->data().size()
      },
      0x0DFF'FF00_u32
    });
}

void window::reset_core() noexcept
{
    if(!core_->pak_loaded()) {
        return;
    }

    core_->reset(prefs_.debugger_bios_skip);
    std::fill(frame_time_history_.begin(),  frame_time_history_.end(), 0.f);
    total_instructions_ = 0_usize;
    total_frames_ = 0_usize;
    total_frame_time_ = 0.f;
    last_executed_addr_ = 0_u32;
}

void window::generate_memory_debugger_entries() noexcept
{
    disassembly_view_.clear();
    memory_view_.clear();

    cartridge::gamepak& pak = access_private::gamepak_(core_);
    ppu::engine& ppu_engine = access_private::ppu_engine_(core_);

    if(pak.loaded()) {
        disassembly_view_.add_entry<memory_view_entry>("ROM", view<u8>{access_private::pak_data_(pak)}, 0x0800'0000_u32);
        memory_view_.add_entry(memory_view_entry{"ROM", view<u8>{access_private::pak_data_(pak)}, 0x0800'0000_u32});
    }

    disassembly_view_.add_entry<memory_view_entry>("EWRAM", view<u8>{access_private::wram_(cpu_)}, 0x0200'0000_u32);
    disassembly_view_.add_entry<memory_view_entry>("IWRAM", view<u8>{access_private::iwram_(cpu_)}, 0x0300'0000_u32);
    disassembly_view_.add_entry<custom_disassembly_entry>();

    memory_view_.add_entry(memory_view_entry{"EWRAM", view<u8>{access_private::wram_(cpu_)}, 0x0200'0000_u32});
    memory_view_.add_entry(memory_view_entry{"IWRAM", view<u8>{access_private::iwram_(cpu_)}, 0x0300'0000_u32});
    memory_view_.add_entry(memory_view_entry{"PALETTE", view<u8>{access_private::palette_ram_(ppu_engine)}, 0x0500'0000_u32});
    memory_view_.add_entry(memory_view_entry{"VRAM", view<u8>{access_private::vram_(ppu_engine)}, 0x0600'0000_u32});
    memory_view_.add_entry(memory_view_entry{"OAM", view<u8>{access_private::oam_(ppu_engine)}, 0x0700'0000_u32});

    switch(pak.backup_type()) {
        case cartridge::backup::type::eeprom_undetected:
            pak.on_eeprom_width_detected_event.add_delegate({connect_arg<&window::on_eeprom_bus_width_detected>, this});
            break;
        case cartridge::backup::type::eeprom_4:
        case cartridge::backup::type::eeprom_64:
            on_eeprom_bus_width_detected();
            break;
        case cartridge::backup::type::sram:
            memory_view_.add_entry(memory_view_entry{
              "SRAM",
              view<u8>{
                access_private::backup_(pak)->data().data(),
                access_private::backup_(pak)->data().size()
              },
              0x0E00'0000_u32
            });
            break;
        case cartridge::backup::type::flash_64:
        case cartridge::backup::type::flash_128:
            memory_view_.add_entry(memory_view_entry{
              "FLASH",
              view<u8>{
                access_private::backup_(pak)->data().data(),
                access_private::backup_(pak)->data().size()
              },
              0x0E00'0000_u32
            });
        case cartridge::backup::type::none:
        case cartridge::backup::type::detect:
            break;
    }
}

void window::set_framerate_limit(const framerate_limit limit) noexcept
{
    static constexpr array framerates{0, 30, 60, 120, 144, 0};

    window_.setFramerateLimit(framerates[from_enum<u32>(limit)]);
    window_.setVerticalSyncEnabled(limit == framerate_limit::vsync);
}

} // namespace gba::debugger
