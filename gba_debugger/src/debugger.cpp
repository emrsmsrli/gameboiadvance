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
#include <imgui_internal.h>
#include <implot.h>
#include <SFML/OpenGL.hpp>

#include <gba/core.h>
#include <gba_debugger/debugger_helpers.h>

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::arm::psr, cpsr_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, wram_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, iwram_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::vector<gba::u8>, pak_data_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, gba::u32, mirror_mask_)
ACCESS_PRIVATE_FIELD(gba::cartridge::gamepak, std::unique_ptr<gba::cartridge::backup>, backup_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, palette_ram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, vram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, oam_)
ACCESS_PRIVATE_FIELD(gba::scheduler, gba::vector<gba::scheduler::hw_event>, heap_)

namespace gba::debugger {

namespace {

std::string_view to_string_view(const arm::debugger_access_width access_type) noexcept
{
    switch(access_type) {
        case arm::debugger_access_width::byte: return "byte";
        case arm::debugger_access_width::hword: return "hword";
        case arm::debugger_access_width::word: return "word";
        default: UNREACHABLE();
    }
}

void debugger_settings_hander_clear_all(ImGuiContext*, ImGuiSettingsHandler* handler)
{
    auto* prefs = static_cast<preferences*>(handler->UserData);
    *prefs = preferences{};
}

void* debugger_settings_hander_read_open(ImGuiContext*, ImGuiSettingsHandler* handler, const char*) { return handler; }

void debugger_settings_hander_read_line(ImGuiContext*, ImGuiSettingsHandler* handler, void*, const char* line)
{
    auto* prefs = static_cast<preferences*>(handler->UserData);
    int dummy1;
    int dummy2;
    int dummy3;
    array<float, 4> dummy4{1.f, 1.f, 1.f, 1.f};
         if(sscanf(line, "ppu_framebuffer_render_scale=%i", &dummy1) == 1) { prefs->ppu_framebuffer_render_scale = dummy1; }
    else if(sscanf(line, "bg0=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs->ppu_bg_preferences[0_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "bg1=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs->ppu_bg_preferences[1_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "bg2=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs->ppu_bg_preferences[2_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "bg3=%i,%i,%i", &dummy1, &dummy2, &dummy3) == 3) { prefs->ppu_bg_preferences[3_u32] = {dummy1 == 1, dummy2 == 1, dummy3}; }
    else if(sscanf(line, "ppu_bg_tiles_render_scale=%i", &dummy1) == 1) { prefs->ppu_bg_tiles_render_scale = dummy1; }
    else if(sscanf(line, "ppu_obj_tiles_render_scale=%i", &dummy1) == 1) { prefs->ppu_obj_tiles_render_scale = dummy1; }
    else if(sscanf(line, "ppu_win_render_scale=%i", &dummy1) == 1) { prefs->ppu_win_render_scale = dummy1; }
    else if(sscanf(line, "ppu_winout_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs->ppu_winout_color = dummy4; }
    else if(sscanf(line, "ppu_win0_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs->ppu_win0_color = dummy4; }
    else if(sscanf(line, "ppu_win1_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs->ppu_win1_color = dummy4; }
    else if(sscanf(line, "ppu_winobj_color=%f,%f,%f", dummy4.ptr(0_u32), dummy4.ptr(1_u32), dummy4.ptr(2_u32)) == 3) { prefs->ppu_winobj_color = dummy4; }

    else if(sscanf(line, "apu_enabled_channel_graphs=%i", &dummy1) == 1) { prefs->apu_enabled_channel_graphs = dummy1; }
}

void debugger_settings_hander_write_all(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
{
    auto* prefs = static_cast<preferences*>(handler->UserData);
    out_buf->appendf("[%s][%s]\n", handler->TypeName, handler->TypeName);
    out_buf->appendf("ppu_framebuffer_render_scale=%i\n", prefs->ppu_framebuffer_render_scale);
    out_buf->appendf("bg0=%i,%i,%i\n",
        prefs->ppu_bg_preferences[0_u32].enable_visible_area,
        prefs->ppu_bg_preferences[0_u32].enable_visible_area_border,
        prefs->ppu_bg_preferences[0_u32].render_scale);
    out_buf->appendf("bg1=%i,%i,%i\n",
        prefs->ppu_bg_preferences[1_u32].enable_visible_area,
        prefs->ppu_bg_preferences[1_u32].enable_visible_area_border,
        prefs->ppu_bg_preferences[1_u32].render_scale);
    out_buf->appendf("bg2=%i,%i,%i\n",
        prefs->ppu_bg_preferences[2_u32].enable_visible_area,
        prefs->ppu_bg_preferences[2_u32].enable_visible_area_border,
        prefs->ppu_bg_preferences[2_u32].render_scale);
    out_buf->appendf("bg3=%i,%i,%i\n",
        prefs->ppu_bg_preferences[3_u32].enable_visible_area,
        prefs->ppu_bg_preferences[3_u32].enable_visible_area_border,
        prefs->ppu_bg_preferences[3_u32].render_scale);
    out_buf->appendf("ppu_bg_tiles_render_scale=%i\n", prefs->ppu_bg_tiles_render_scale);
    out_buf->appendf("ppu_obj_tiles_render_scale=%i\n", prefs->ppu_obj_tiles_render_scale);
    out_buf->appendf("ppu_win_render_scale=%i\n", prefs->ppu_win_render_scale);
    out_buf->appendf("ppu_winout_color=%f,%f,%f\n", prefs->ppu_winout_color[0_u32], prefs->ppu_winout_color[1_u32], prefs->ppu_winout_color[2_u32]);
    out_buf->appendf("ppu_win0_color=%f,%f,%f\n", prefs->ppu_win0_color[0_u32], prefs->ppu_win0_color[1_u32], prefs->ppu_win0_color[2_u32]);
    out_buf->appendf("ppu_win1_color=%f,%f,%f\n", prefs->ppu_win1_color[0_u32], prefs->ppu_win1_color[1_u32], prefs->ppu_win1_color[2_u32]);
    out_buf->appendf("ppu_winobj_color=%f,%f,%f\n", prefs->ppu_winobj_color[0_u32], prefs->ppu_winobj_color[1_u32], prefs->ppu_winobj_color[2_u32]);

    out_buf->appendf("apu_enabled_channel_graphs=%i\n", prefs->apu_enabled_channel_graphs);
}

} // namespace

window::window(core* core) noexcept
  : window_{sf::VideoMode{1920, 1080}, "GBA Debugger"},
    window_event_{},
    audio_device_{2, sdl::audio_device::format::f32, 48000, 2048},
    core_{core},
    disassembly_view_{&breakpoint_database_},
    gamepak_debugger_{&core->pak},
    cpu_debugger_{&core_->timer_controller, &core_->dma_controller, &core->arm, &breakpoint_database_, access_private::mirror_mask_(core_->pak)},
    ppu_debugger_{&core->ppu, &prefs_},
    apu_debugger_{&core->apu, &prefs_},
    keypad_debugger_{&core->keypad}
{
    window_.setFramerateLimit(60);

    using namespace std::string_view_literals;
    disassembly_view_.add_entry(memory_view_entry{"ROM"sv, view<u8>{access_private::pak_data_(core->pak)}, 0x0800'0000_u32});
    disassembly_view_.add_entry(memory_view_entry{"EWRAM"sv, view<u8>{access_private::wram_(core->arm)}, 0x0200'0000_u32});
    disassembly_view_.add_entry(memory_view_entry{"IWRAM"sv, view<u8>{access_private::iwram_(core->arm)}, 0x0300'0000_u32});
    disassembly_view_.add_custom_disassembly_entry();

    memory_view_.add_entry(memory_view_entry{"ROM"sv, view<u8>{access_private::pak_data_(core->pak)}, 0x0800'0000_u32});
    memory_view_.add_entry(memory_view_entry{"EWRAM"sv, view<u8>{access_private::wram_(core->arm)}, 0x0200'0000_u32});
    memory_view_.add_entry(memory_view_entry{"IWRAM"sv, view<u8>{access_private::iwram_(core->arm)}, 0x0300'0000_u32});
    memory_view_.add_entry(memory_view_entry{"PALETTE"sv, view<u8>{access_private::palette_ram_(core->ppu)}, 0x0500'0000_u32});
    memory_view_.add_entry(memory_view_entry{"VRAM"sv, view<u8>{access_private::vram_(core->ppu)}, 0x0600'0000_u32});
    memory_view_.add_entry(memory_view_entry{"OAM"sv, view<u8>{access_private::oam_(core->ppu)}, 0x0700'0000_u32});
    switch(core->pak.backup_type()) {
        case cartridge::backup::type::eeprom_undetected:
        case cartridge::backup::type::eeprom_4:
        case cartridge::backup::type::eeprom_64:
            memory_view_.add_entry(memory_view_entry{
              "EEPROM"sv,
              view<u8>{
                access_private::backup_(core->pak)->data().data(),
                access_private::backup_(core->pak)->data().size()
              },
              0x0DFF'FF00_u32
            });
            break;
        case cartridge::backup::type::sram:
            memory_view_.add_entry(memory_view_entry{
              "SRAM"sv,
              view<u8>{
                access_private::backup_(core->pak)->data().data(),
                access_private::backup_(core->pak)->data().size()
              },
              0x0E00'0000_u32
            });
            break;
        case cartridge::backup::type::flash_64:
        case cartridge::backup::type::flash_128:
            memory_view_.add_entry(memory_view_entry{
              "FLASH"sv,
              view<u8>{
                access_private::backup_(core->pak)->data().data(),
                access_private::backup_(core->pak)->data().size()
              },
              0x0E00'0000_u32
            });
        case cartridge::backup::type::none:
        case cartridge::backup::type::detect:
            break;
    }

    window_.resetGLStates();
    window_.setVerticalSyncEnabled(false);
    window_.setFramerateLimit(60);
    ImGui::SFML::Init(window_, true);

    ImGuiSettingsHandler debugger_settings_handler;
    debugger_settings_handler.UserData = &prefs_;
    debugger_settings_handler.TypeName = "DebuggerSettings";
    debugger_settings_handler.TypeHash = ImHashStr("DebuggerSettings");
    debugger_settings_handler.ReadOpenFn = debugger_settings_hander_read_open;
    debugger_settings_handler.ClearAllFn = debugger_settings_hander_clear_all;
    debugger_settings_handler.ReadLineFn = debugger_settings_hander_read_line;
    debugger_settings_handler.WriteAllFn = debugger_settings_hander_write_all;
    ImGui::GetCurrentContext()->SettingsHandlers.push_back(debugger_settings_handler);

    audio_device_.resume();

    [[maybe_unused]] const sf::ContextSettings& settings = window_.getSettings();
    LOG_TRACE(debugger, "OpenGL {}.{}, attr: 0x{:X}", settings.majorVersion, settings.minorVersion, settings.attributeFlags);
    LOG_TRACE(debugger, "{} {}", glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    core_->arm.on_instruction_execute.connect<&window::on_instruction_execute>(this);
    core_->arm.on_io_read.connect<&window::on_io_read>(this);
    core_->arm.on_io_write.connect<&window::on_io_write>(this);

    cpu_debugger_.on_execution_requested.add_delegate({connect_arg<&window::on_execution_requested>, this});
    core_->ppu.event_on_scanline.add_delegate({connect_arg<&window::on_scanline>, this});
    core_->ppu.event_on_vblank.add_delegate({connect_arg<&window::on_vblank>, this});
    core_->apu.get_buffer_overflow_event().add_delegate({connect_arg<&window::on_audio_buffer_full>, this});

    core_->apu.set_dst_sample_rate(audio_device_.frequency());
    core_->apu.set_buffer_capacity(audio_device_.sample_count());
    apu_debugger_.set_buffer_capacity(audio_device_.sample_count());
}

bool window::draw() noexcept
{
    while(window_.pollEvent(window_event_)) {
        if(window_event_.type == sf::Event::Closed) {
            ImGui::SaveIniSettingsToDisk(ImGui::GetCurrentContext()->IO.IniFilename);
            return false;
        }

        ImGui::SFML::ProcessEvent(window_event_);

        if(window_event_.type == sf::Event::KeyPressed) {
            switch(window_event_.key.code) {
                case sf::Keyboard::W: core_->press_key(keypad::key::up); break;
                case sf::Keyboard::A: core_->press_key(keypad::key::left); break;
                case sf::Keyboard::S: core_->press_key(keypad::key::down); break;
                case sf::Keyboard::D: core_->press_key(keypad::key::right); break;
                case sf::Keyboard::K: core_->press_key(keypad::key::b); break;
                case sf::Keyboard::O: core_->press_key(keypad::key::a); break;
                case sf::Keyboard::B: core_->press_key(keypad::key::select); break;
                case sf::Keyboard::N: core_->press_key(keypad::key::start); break;
                case sf::Keyboard::T: core_->press_key(keypad::key::left_shoulder); break;
                case sf::Keyboard::U: core_->press_key(keypad::key::right_shoulder); break;
                case sf::Keyboard::F7: core_->arm.tick(); break;
                default:
                    break;
            }
        } else if(window_event_.type == sf::Event::KeyReleased) {
            switch(window_event_.key.code) {
                case sf::Keyboard::W: core_->release_key(keypad::key::up); break;
                case sf::Keyboard::A: core_->release_key(keypad::key::left); break;
                case sf::Keyboard::S: core_->release_key(keypad::key::down); break;
                case sf::Keyboard::D: core_->release_key(keypad::key::right); break;
                case sf::Keyboard::K: core_->release_key(keypad::key::b); break;
                case sf::Keyboard::O: core_->release_key(keypad::key::a); break;
                case sf::Keyboard::B: core_->release_key(keypad::key::select); break;
                case sf::Keyboard::N: core_->release_key(keypad::key::start); break;
                case sf::Keyboard::T: core_->release_key(keypad::key::left_shoulder); break;
                case sf::Keyboard::U: core_->release_key(keypad::key::right_shoulder); break;
                case sf::Keyboard::F9: tick_allowed_ = !tick_allowed_; break;
                default:
                    break;
            }
        }
    }

    if(!window_.hasFocus()) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
        return true;
    }

    const u64 until = core_->schdlr.now() + ppu::engine::cycles_per_frame;
    while (tick_allowed_ && core_->schdlr.now() < until) {
        core_->arm.tick();
    }

    const sf::Time dt = dt_.restart();
    ImGui::SFML::Update(window_, dt);

    disassembly_view_.draw_with_mode(access_private::cpsr_(core_->arm).t);
    memory_view_.draw();
    gamepak_debugger_.draw();
    cpu_debugger_.draw();
    ppu_debugger_.draw();
    apu_debugger_.draw();
    keypad_debugger_.draw();

    if(ImGui::Begin("Scheduler", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const scheduler& scheduler = core_->schdlr;
        vector<scheduler::hw_event> events = access_private::heap_(scheduler);
        std::sort(events.begin(), events.end(), [](const scheduler::hw_event& e1, const scheduler::hw_event& e2) {
            return e1.timestamp < e2.timestamp;
        });

        ImGui::Text("Cycles: {}", scheduler.now());
        ImGui::Text("Scheduled event count: {}", events.size());

        ImGui::Spacing();
        ImGui::Spacing();
        for(const scheduler::hw_event& event : events) {
            ImGui::Text("{}, timestamp: {} ({})", event.name, event.timestamp, event.timestamp - scheduler.now());
        }
    }

    ImGui::End();

    if(ImGui::Begin("Stats")) {
        static array framerates{0, 30, 60, 120, 144, 0};
        static array framerate_names{"unlimited", "30", "60", "120", "144", "vsync"};
        static int framerate_idx = 2;
        ImGui::SetNextItemWidth(150.f);
        if(ImGui::Combo("framerate", &framerate_idx, framerate_names.data(), framerate_names.size().get())) {
            window_.setFramerateLimit(framerates[static_cast<u32::type>(framerate_idx)]);
            window_.setVerticalSyncEnabled(framerate_idx == 5);

            total_frames_ = 0_usize;
            total_frame_time_ = 0.f;
        }

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

void window::on_io_read(const u32 address, const arm::debugger_access_width access_type) noexcept
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

void window::on_io_write(const u32 address, const u32 data, const arm::debugger_access_width access_type) noexcept
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
    const usize buffer_size_in_bytes = sizeof(apu::stereo_sample<float>) * buffer.size();
    audio_device_.enqueue(reinterpret_cast<const void*>(buffer.data()), buffer_size_in_bytes.get()); // NOLINT
    while(audio_device_.queue_size() > buffer_size_in_bytes) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }
}

} // namespace gba::debugger
