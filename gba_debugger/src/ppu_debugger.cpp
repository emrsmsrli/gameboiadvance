/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/ppu_debugger.h>

#include <access_private.h>
#include <SFML/Graphics/Sprite.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include <gba/helper/range.h>
#include <gba_debugger/preferences.h>
#include <gba_debugger/debugger_helpers.h>

ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, palette_ram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, vram_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::vector<gba::u8>, oam_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::dispcnt, dispcnt_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::dispstat, dispstat_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::u8, vcount_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::bg_regular, bg0_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::bg_regular, bg1_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::bg_affine, bg2_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::bg_affine, bg3_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::window, win0_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::window, win1_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::win_in, win_in_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::win_out, win_out_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, bool, green_swap_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::mosaic, mosaic_bg_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::mosaic, mosaic_obj_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::bldcnt, bldcnt_)
ACCESS_PRIVATE_FIELD(gba::ppu::engine, gba::ppu::blend_settings, blend_settings_)

using win_buffer_t = gba::array<gba::ppu::win_enable_bits*, gba::ppu::screen_width>;
ACCESS_PRIVATE_FIELD(gba::ppu::engine, win_buffer_t, win_buffer_)

ACCESS_PRIVATE_FUN(gba::ppu::engine, gba::ppu::color(gba::u8, gba::u8) const noexcept, palette_color)
ACCESS_PRIVATE_FUN(gba::ppu::engine, gba::ppu::color(gba::u8, gba::u8) const noexcept, palette_color_opaque)

namespace gba::debugger {

namespace {

ppu::bg_regular to_regular(const ppu::bg_affine& affine) noexcept
{
    ppu::bg_regular reg{affine.id};
    reg.voffset = affine.voffset;
    reg.hoffset = affine.hoffset;
    reg.cnt.priority = affine.cnt.priority;
    reg.cnt.char_base_block = affine.cnt.char_base_block;
    reg.cnt.mosaic_enabled = affine.cnt.mosaic_enabled;
    reg.cnt.color_depth_8bit = affine.cnt.color_depth_8bit;
    reg.cnt.screen_entry_base_block = affine.cnt.screen_entry_base_block;
    reg.cnt.screen_size = affine.cnt.screen_size;
    return reg;
}

sf::Color to_sf_color(const ppu::color color) noexcept
{
    return sf::Color{color.to_u32().get()};
}

// we cannot access private templates
template<typename BG>
[[nodiscard]] FORCEINLINE u32 map_entry_index_duplicate(const u32 tile_x, const u32 tile_y, const BG& bg) noexcept
{
    u32 n = tile_x + tile_y * 32_u32;
    if(tile_x >= 0x20_u32) { n += 0x03E0_u32; }
    if(tile_y >= 0x20_u32 && bg.cnt.screen_size == 3_u8) { n += 0x0400_u32; }
    return n;
}

std::string_view to_string_view(const ppu::obj_attr0::rendering_mode mode) noexcept
{
    switch(mode) {
        case ppu::obj_attr0::rendering_mode::normal: return "normal";
        case ppu::obj_attr0::rendering_mode::affine: return "affine";
        case ppu::obj_attr0::rendering_mode::hidden: return "hidden";
        case ppu::obj_attr0::rendering_mode::affine_double: return "affine_double";
        default: UNREACHABLE();
    }
}

std::string_view to_string_view(const ppu::obj_attr0::blend_mode mode) noexcept
{
    switch(mode) {
        case ppu::obj_attr0::blend_mode::normal: return "normal";
        case ppu::obj_attr0::blend_mode::alpha_blending: return "alpha_blending";
        case ppu::obj_attr0::blend_mode::obj_window: return "obj_window";
        case ppu::obj_attr0::blend_mode::prohibited: return "prohibited";
        default: UNREACHABLE();
    }
}

float matrix_elem_to_float(const i16 e) noexcept
{
    return static_cast<float>(e.get() >> 8_i16)
      + ((e.get() & 0xFF_i16) / static_cast<float>(0xFF_i16));
}

} // namespace

ppu_debugger::ppu_debugger(ppu::engine* engine, preferences* p)
  : prefs_{p}, ppu_engine_{engine},
    win_types_{ppu::screen_height * ppu::screen_width}
{
    screen_buffer_.create(ppu::screen_width, ppu::screen_height);
    screen_texture_.create(ppu::screen_width, ppu::screen_height);
    for(usize i : range(bg_buffers_.size())) {
        bg_buffers_[i].create(1024, 1024);
        bg_textures_[i].create(1024, 1024);
    }
    for(usize i : range(obj_buffers_.size())) {
        obj_buffers_[i].create(64, 64);
        obj_textures_[i].create(64, 64);
    }

    tiles_buffer_.create(256, 512);
    tiles_texture_.create(256, 512);

    win_buffer_.create(ppu::screen_width, ppu::screen_height);
    win_texture_.create(ppu::screen_width, ppu::screen_height);

    engine->event_on_scanline.add_delegate({connect_arg<&ppu_debugger::on_scanline>, this});
    engine->event_on_vblank.add_delegate({connect_arg<&ppu_debugger::on_update_texture>, this});
}

void ppu_debugger::draw() noexcept
{
    if(ImGui::Begin("Framebuffer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        int& draw_scale = prefs_->ppu_framebuffer_render_scale;
        ImGui::SetNextItemWidth(150.f);
        ImGui::SliderInt("render scale", &draw_scale, 1, 4);

        sf::Sprite screen{screen_texture_};
        screen.setScale(draw_scale, draw_scale);
        const ImVec2 img_start = ImGui::GetCursorScreenPos();
        ImGui::Image(screen);
        if(ImGui::IsItemHovered()) {
            const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            const u32::type y = (mouse_pos.y - img_start.y) / draw_scale;
            const u32::type x = (mouse_pos.x - img_start.x) / draw_scale;

            ImGui::BeginTooltip();
            ImGui::Text("x: {}, y {}", x, y);
            ImGui::EndTooltip();
        }
    }

    ImGui::End();

    if(ImGui::Begin("PPU")) {
        if(ImGui::BeginTabBar("#pputabs")) {
            const auto& dispcnt = access_private::dispcnt_(ppu_engine_);
            const auto& palette = access_private::palette_ram_(ppu_engine_);

            if(ImGui::BeginTabItem("Registers")) {
                if(!ImGui::BeginChild("#ppuregschild")) {
                    ImGui::EndChild();
                    return;
                }

                ImGui::Text("vcount: {}", access_private::vcount_(ppu_engine_));
                ImGui::SameLine(0.f, 50.f);
                ImGui::Text("greenswap: {}", access_private::green_swap_(ppu_engine_));

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10.f, 10.f));
                if(ImGui::BeginTable("#regtables", 2, ImGuiTableFlags_BordersInner)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "DISPCNT");
                    ImGui::Spacing();
                    ImGui::Text("mode: {}\n"
                      "bitmap frame: {}\n"
                      "interval free: {}\n"
                      "obj 1d mapping: {}\n"
                      "forced blank: {}\n"
                      "bg enables: {}\n"
                      "obj enable: {}\n"
                      "win enables: {}\n"
                      "winobj enable: {}\n",
                      dispcnt.bg_mode.get(),
                      dispcnt.frame_select.get(),
                      dispcnt.hblank_interval_free,
                      dispcnt.obj_mapping_1d,
                      dispcnt.forced_blank,
                      fmt::join(dispcnt.bg_enabled, ", "),
                      dispcnt.obj_enabled,
                      fmt::join(array<bool, 2>{dispcnt.win0_enabled, dispcnt.win1_enabled}, ", "),
                      dispcnt.win_obj_enabled);

                    ImGui::TableNextColumn();

                    const auto& dispstat = access_private::dispstat_(ppu_engine_);
                    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "DISPSTAT");
                    ImGui::Spacing();
                    ImGui::Text("vblank: {}\n"
                      "hblank: {}\n"
                      "vcounter: {}\n"
                      "vblank irq enabled: {}\n"
                      "hblank irq enabled: {}\n"
                      "vcounter irq enabled: {}\n"
                      "vcount setting: {:02X}",
                      dispstat.vblank, dispstat.hblank, dispstat.vcounter,
                      dispstat.vblank_irq_enabled, dispstat.hblank_irq_enabled, dispstat.vcounter_irq_enabled,
                      dispstat.vcount_setting);


                    const auto draw_bg_regs = [](const char* name, const auto& bg) {
                        using bg_type = std::remove_cv_t<std::remove_reference_t<decltype(bg)>>;

                        const usize tile_base = bg.cnt.char_base_block * 16_kb;
                        const usize map_entry_base = bg.cnt.screen_entry_base_block * 2_kb;

                        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s", name);
                        ImGui::Spacing();
                        ImGui::Text("priority: {}", bg.cnt.priority.get());
                        if constexpr(std::is_same_v<bg_type, ppu::bg_affine>) {
                            ImGui::Text("wraparound: {}", bg.cnt.wraparound);
                        }
                        ImGui::Text("mosaic: {}", bg.cnt.mosaic_enabled);
                        ImGui::Text("8bit depth: {}", bg.cnt.color_depth_8bit);
                        ImGui::Text("tile base: {:08X}", tile_base + 0x0600'0000_u32);
                        ImGui::Text("screen entry base: {:08X}", map_entry_base + 0x0600'0000_u32);
                        ImGui::Text("voffset: {:04X}", bg.voffset);
                        ImGui::Text("hoffset: {:04X}", bg.hoffset);
                        if constexpr(std::is_same_v<bg_type, ppu::bg_affine>) {
                            ImGui::Text("yref: {:08X}, internal {:08X}", bg.y_ref.ref, bg.y_ref.internal);
                            ImGui::Text("xref: {:08X}, internal {:08X}", bg.x_ref.ref, bg.x_ref.internal);
                            ImGui::Text("pa: {:04X} ({})\npb: {:04X} ({})",
                              bg.pa, matrix_elem_to_float(make_signed(bg.pa)), bg.pb, matrix_elem_to_float(make_signed(bg.pb)));
                            ImGui::SameLine(0, 100.f);
                            ImGui::Text("pc: {:04X} ({})\npd: {:04X} ({})",
                              bg.pc, matrix_elem_to_float(make_signed(bg.pc)), bg.pd, matrix_elem_to_float(make_signed(bg.pd)));
                        }
                    };

                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    draw_bg_regs("BG0", access_private::bg0_(ppu_engine_)); ImGui::TableNextColumn();
                    draw_bg_regs("BG1", access_private::bg1_(ppu_engine_)); ImGui::TableNextRow(); ImGui::TableNextColumn();
                    draw_bg_regs("BG2", access_private::bg2_(ppu_engine_)); ImGui::TableNextColumn();
                    draw_bg_regs("BG3", access_private::bg3_(ppu_engine_));

                    const auto draw_win_regs = [](const char* name, const ppu::window& win) {
                        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s", name);
                        ImGui::Spacing();
                        ImGui::Text("top left     x: {:02X}, y: {:02X}", win.top_left.x, win.top_left.y);
                        ImGui::Text("bottom right x: {:02X}, y: {:02X}", win.bottom_right.x, win.bottom_right.y);
                    };

                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    draw_win_regs("WIN0", access_private::win0_(ppu_engine_)); ImGui::TableNextColumn();
                    draw_win_regs("WIN1", access_private::win1_(ppu_engine_));

                    const auto draw_win_enable_regs = [](const char* name, const ppu::win_enable_bits& e) {
                        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s", name);
                        ImGui::Spacing();
                        ImGui::Text("bg enables: {}\nobj enable: {}\nblending enable: {}",
                          fmt::join(e.bg_enabled, ", "), e.obj_enabled, e.blend_enabled);
                    };

                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    draw_win_enable_regs("WININ_WIN0", access_private::win_in_(ppu_engine_).win0); ImGui::TableNextColumn();
                    draw_win_enable_regs("WININ_WIN1", access_private::win_in_(ppu_engine_).win1); ImGui::TableNextRow(); ImGui::TableNextColumn();

                    draw_win_enable_regs("WINOUT_OUT", access_private::win_out_(ppu_engine_).outside); ImGui::TableNextColumn();
                    draw_win_enable_regs("WINOUT_OBJ", access_private::win_out_(ppu_engine_).obj);

                    const auto draw_mosaic_regs = [](const char* name, const ppu::mosaic& m) {
                        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s", name);
                        ImGui::Spacing();
                        ImGui::Text("v: {:02X} h: {:02X}\nv: {:02X} h: {:02X} (internal)",
                          m.v, m.h, m.internal.v, m.internal.h);
                    };

                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    draw_mosaic_regs("MOSAIC_BG", access_private::mosaic_bg_(ppu_engine_)); ImGui::TableNextColumn();
                    draw_mosaic_regs("MOSAIC_OBJ", access_private::mosaic_obj_(ppu_engine_));

                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    auto& bldcnt = access_private::bldcnt_(ppu_engine_);
                    const auto draw_bldcnt_target_regs = [](const char* name, const ppu::bldcnt::target& t) {
                        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s", name);
                        ImGui::Spacing();
                        ImGui::Text("bg: {}\nobj: {}\nbackdrop: {}",
                          fmt::join(t.bg, ", "), t.obj, t.backdrop);
                    };
                    ImGui::Text("blend type: {}", [&]() {
                        switch(bldcnt.type) {
                            case ppu::bldcnt::effect::none: return "none";
                            case ppu::bldcnt::effect::alpha_blend: return "alpha_blend";
                            case ppu::bldcnt::effect::brightness_inc: return "brightness_inc";
                            case ppu::bldcnt::effect::brightness_dec: return "brightness_dec";
                            default: UNREACHABLE();
                        }
                    }());
                    if(ImGui::BeginTable("#bldcntable", 2)) {
                        ImGui::TableNextRow(); ImGui::TableNextColumn();
                        draw_bldcnt_target_regs("first", bldcnt.first);
                        ImGui::TableNextColumn();
                        draw_bldcnt_target_regs("second", bldcnt.second);
                        ImGui::EndTable();
                    }

                    ImGui::TableNextColumn();
                    auto& bldset = access_private::blend_settings_(ppu_engine_);
                    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "BLEND SETTINGS");
                    ImGui::Spacing();
                    ImGui::Text("eva {:02X}\nevb {:02X}\nevy {:02X}",
                      bldset.eva, bldset.evb, bldset.evy);

                    ImGui::EndTable();
                }

                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            const auto draw_tab = [](const char* name, const bool can_be_opened, auto&& draw_func) {
                if(!can_be_opened) {
                    ImGui::PushStyleColor(ImGuiCol_Tab, 0x212529FF);
                    ImGui::PushStyleColor(ImGuiCol_TabHovered, 0x343A40FF);
                    ImGui::PushStyleColor(ImGuiCol_TabActive, 0x495057DC);
                }
                if(ImGui::BeginTabItem(name)) {
                    draw_func();
                    ImGui::EndTabItem();
                }
                if(!can_be_opened) {
                    ImGui::PopStyleColor(3);
                }
            };

            draw_tab("BG0", dispcnt.bg_enabled[0_usize], [&]() {
                draw_regular_bg_map(access_private::bg0_(ppu_engine_));
            });

            draw_tab("BG1", dispcnt.bg_enabled[1_usize], [&]() {
                draw_regular_bg_map(access_private::bg1_(ppu_engine_));
            });

            draw_tab("BG2", dispcnt.bg_enabled[2_usize] && dispcnt.bg_mode < 6_u32, [&]() {
                switch(dispcnt.bg_mode.get()) {
                    case 0:
                        draw_regular_bg_map(to_regular(access_private::bg2_(ppu_engine_)));
                        break;
                    case 1: case 2:
                        draw_affine_bg_map(access_private::bg2_(ppu_engine_));
                        break;
                    case 3: case 4: case 5:
                        draw_bitmap_bg(access_private::bg2_(ppu_engine_), dispcnt.bg_mode);
                        break;
                    default:
                        UNREACHABLE();
                }
            });

            draw_tab("BG3", dispcnt.bg_enabled[3_usize] && dispcnt.bg_mode == 0_u32 || dispcnt.bg_mode == 2_u32, [&]() {
                switch(dispcnt.bg_mode.get()) {
                    case 0:
                        draw_regular_bg_map(to_regular(access_private::bg3_(ppu_engine_)));
                        break;
                    case 2:
                        draw_affine_bg_map(access_private::bg3_(ppu_engine_));
                        break;
                    default:
                        UNREACHABLE();
                }
            });

            if(ImGui::BeginTabItem("BG Tiles")) {
                draw_bg_tiles();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("OBJ Tiles")) {
                draw_obj_tiles();
                ImGui::EndTabItem();
            }

            draw_tab("OAM", dispcnt.obj_enabled, [&]() {
                draw_obj();
            });

            const bool any_window_enabled = dispcnt.win0_enabled || dispcnt.win1_enabled || dispcnt.win_obj_enabled;
            draw_tab("Window View", any_window_enabled, [&]() {
                draw_win_buffer();
            });

            if(ImGui::BeginTabItem("Palette View")) {
                const auto draw_palette = [&](const char* name, usize type_offset /*1 for obj*/) {
                    ImGui::BeginGroup();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(name);
                    for(u32 palette_idx : range(16_u32)) {
                        for(u32 color_idx : range(16_u32)) {
                            const usize address = type_offset * 512_u32 + palette_idx * 32_u32 + color_idx * 2_u32;
                            const u16 bgr_color = memcpy<u16>(palette, address);
                            const ppu::color ppu_color{bgr_color};
                            const sf::Color sf_color = to_sf_color(ppu_color);

                            ImGui::ColorButton("", sf_color,
                              ImGuiColorEditFlags_NoBorder
                              | ImGuiColorEditFlags_NoAlpha
                              | ImGuiColorEditFlags_NoDragDrop
                              | ImGuiColorEditFlags_NoTooltip, ImVec2(25, 25));
                            if(ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::ColorButton("##preview", sf_color, ImGuiColorEditFlags_NoTooltip, ImVec2{75.f, 75.f});
                                ImGui::SameLine();
                                ImGui::Text("address: {:08X}\nvalue: {:04X}\nR: {:02X}\nG: {:02X}\nB: {:02X}",
                                  address + 0x0500'0000_u32,
                                  bgr_color,
                                  (bgr_color & ppu::color::r_mask),
                                  (bgr_color & ppu::color::g_mask) >> 5_u16,
                                  (bgr_color & ppu::color::b_mask) >> 10_u16);
                                ImGui::EndTooltip();
                            }

                            ImGui::SameLine(0, 4);
                        }
                        ImGui::NewLine();
                    }
                    ImGui::EndGroup();
                };

                draw_palette("BG", 0_usize); ImGui::SameLine();
                draw_palette("OBJ", 1_usize);

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}

void ppu_debugger::on_scanline(u8 screen_y, const ppu::scanline_buffer& scanline) noexcept
{
    auto& win_buffer = access_private::win_buffer_(ppu_engine_);
    for(u32 x : range(ppu::screen_width)) {
        screen_buffer_.setPixel(x.get(), screen_y.get(), to_sf_color(scanline[x]));

        win_types_[ppu::screen_width * screen_y + x] = [&]() {
            if(win_buffer[x] == &access_private::win_out_(ppu_engine_).outside) {
                return win_type::out;
            }
            if(win_buffer[x] == &access_private::win_in_(ppu_engine_).win0) {
                return win_type::w0;
            }
            if(win_buffer[x] == &access_private::win_in_(ppu_engine_).win1) {
                return win_type::w1;
            }
            return win_type::obj;
        }();
    }
}

void ppu_debugger::on_update_texture() noexcept
{
    screen_texture_.update(screen_buffer_);
}

void ppu_debugger::draw_regular_bg_map(const ppu::bg_regular& bg) noexcept
{
    ppu_bg_preferences& bg_prefs = prefs_->ppu_bg_preferences[bg.id];

    const auto& vram = access_private::vram_(ppu_engine_);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine_);
    sf::Image& buffer = bg_buffers_[bg.id];
    sf::Texture& texture = bg_textures_[bg.id];

    const usize tile_base = bg.cnt.char_base_block * 16_kb;
    const usize map_entry_base = bg.cnt.screen_entry_base_block * 2_kb;

    ImGui::Checkbox("Enable visible area mask", &bg_prefs.enable_visible_area);
    if(bg_prefs.enable_visible_area) { ImGui::SameLine(); ImGui::Checkbox("Enable visible area border", &bg_prefs.enable_visible_area_border); }

    static constexpr array map_block_sizes{
      ppu::dimension<u8>{1_u8, 1_u8},
      ppu::dimension<u8>{2_u8, 1_u8},
      ppu::dimension<u8>{1_u8, 2_u8},
      ppu::dimension<u8>{2_u8, 2_u8},
    };

    constexpr auto regular_bg_map_block_tile_count = 32_u32;
    const ppu::dimension block_size = map_block_sizes[bg.cnt.screen_size];
    const sf::Vector2u map_total_block_dimensions{
      regular_bg_map_block_tile_count * block_size.h.get(),
      regular_bg_map_block_tile_count * block_size.v.get()
    };
    const sf::Vector2u map_total_dot_dimensions = map_total_block_dimensions * ppu::tile_dot_count;

    for(u32 ty = 0_u32; ty < map_total_block_dimensions.y; ++ty) {
        for(u32 tx = 0_u32; tx < map_total_block_dimensions.x; ++tx) {
            const ppu::bg_map_entry entry{memcpy<u16>(vram,
              map_entry_base + map_entry_index_duplicate(tx, ty, bg) * 2_u32)};

            if(bg.cnt.color_depth_8bit) {
                for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                    for(u32 px = 0_u32; px < ppu::tile_dot_count; ++px) {
                        const u8 color_idx = memcpy<u8>(vram, tile_base + entry.tile_idx() * 64_u32 + py * 8_u32 + px);
                        buffer.setPixel(
                          tx.get() * ppu::tile_dot_count + (entry.hflipped() ? 7 - px.get() : px.get()),
                          ty.get() * ppu::tile_dot_count + (entry.vflipped() ? 7 - py.get() : py.get()),
                          to_sf_color(call_private::palette_color_opaque(ppu_engine_, color_idx, 0_u8)));
                    }
                }
            } else {
                for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                    for(u32 px = 0_u32; px < ppu::tile_dot_count; px += 2_u32) {
                        const u8 color_idxs = memcpy<u8>(vram, tile_base + entry.tile_idx() * 32_u32 + py * 4_u32 + px / 2_u32);
                        buffer.setPixel(
                          tx.get() * ppu::tile_dot_count + (entry.hflipped() ? 7 - px.get() : px.get()),
                          ty.get() * ppu::tile_dot_count + (entry.vflipped() ? 7 - py.get() : py.get()),
                          to_sf_color(call_private::palette_color_opaque(ppu_engine_, color_idxs & 0xF_u8, entry.palette_idx())));
                        buffer.setPixel(
                          tx.get() * ppu::tile_dot_count + (entry.hflipped() ? 6 - px.get() : px.get() + 1),
                          ty.get() * ppu::tile_dot_count + (entry.vflipped() ? 7 - py.get() : py.get()),
                          to_sf_color(call_private::palette_color_opaque(ppu_engine_, color_idxs >> 4_u8, entry.palette_idx())));
                    }
                }
            }
        }
    }

    int& draw_scale = bg_prefs.render_scale;
    ImGui::SetNextItemWidth(150.f);
    ImGui::SliderInt("render scale", &draw_scale, 1, 4);

    if(!ImGui::BeginChild("#bgtexture", ImVec2{}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        return;
    }

    texture.update(buffer);
    sf::Sprite bg_sprite{texture, sf::IntRect(0, 0, map_total_dot_dimensions.x, map_total_dot_dimensions.y)};
    bg_sprite.setScale(draw_scale, draw_scale);

    // draw the whole map in half alpha
    const ImVec2 img_start = ImGui::GetCursorScreenPos();
    ImGui::Image(bg_sprite, !bg_prefs.enable_visible_area ? sf::Color::White : sf::Color{0xFFFFFF7F});
    const ImVec2 img_end = ImGui::GetCursorScreenPos();

    if(ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();

        const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        const u32::type tile_y = (mouse_pos.y - img_start.y) / (draw_scale * ppu::tile_dot_count);
        const u32::type tile_x = (mouse_pos.x - img_start.x) / (draw_scale * ppu::tile_dot_count);

        const sf::Sprite zoomed_tile{texture, sf::IntRect(
          tile_x * ppu::tile_dot_count,
          tile_y * ppu::tile_dot_count,
          ppu::tile_dot_count,
          ppu::tile_dot_count)};
        ImGui::Image(zoomed_tile, {128, 128});

        const ppu::bg_map_entry entry{memcpy<u16>(vram,
          map_entry_base + map_entry_index_duplicate(tile_x, tile_y, bg) * 2_u32)};
        ImGui::BeginGroup();
        ImGui::Text("map base addr: {:08X}", 0x0600'0000_u32 + map_entry_base
          + map_entry_index_duplicate(tile_x, tile_y, bg) * 2_u32);
        ImGui::Text("tile base addr: {:08X}", 0x0600'0000_u32 + tile_base
          + entry.tile_idx() * (bg.cnt.color_depth_8bit ? 64_u32 : 32_u32));
        ImGui::Text("tile: {:02X} palette {:02X}", entry.tile_idx(), entry.palette_idx());
        ImGui::Text("x: {:02X} y: {:02X}", tile_x, tile_y);
        ImGui::Text("hflip: {}\nvflip: {}", entry.hflipped(), entry.vflipped());
        ImGui::EndGroup();

        ImGui::EndTooltip();
    }

    if(!bg_prefs.enable_visible_area) {
        ImGui::EndChild();
        return;
    }

    // draw the actual visible area
    const u32::type xoffset = bg.hoffset.get() % map_total_dot_dimensions.x;
    const u32::type yoffset = bg.voffset.get() % map_total_dot_dimensions.y;

    sf::IntRect first_part(
      xoffset, yoffset,
      std::min(map_total_dot_dimensions.x - xoffset, ppu::screen_width),
      std::min(map_total_dot_dimensions.y - yoffset, ppu::screen_height)
    );
    sf::IntRect second_part(
      0, 0,
      ppu::screen_width - first_part.width,
      ppu::screen_height - first_part.height
    );
    sf::IntRect third_part(first_part.left, second_part.top, first_part.width, second_part.height);
    sf::IntRect fourth_part(second_part.left, first_part.top, second_part.width, first_part.height);

    const auto draw_visible_area = [&](const sf::IntRect& area) {
        if(area.width == 0 || area.height == 0) { return; }

        ImGui::SetCursorScreenPos(ImVec2(
          img_start.x + area.left * draw_scale,
          img_start.y + area.top * draw_scale));
        bg_sprite.setTextureRect(area);
        ImGui::Image(bg_sprite, sf::Color::White, bg_prefs.enable_visible_area_border ? sf::Color::White : sf::Color::Transparent);
        ImGui::SetCursorScreenPos(img_end);
    };

    draw_visible_area(first_part);
    draw_visible_area(second_part);
    draw_visible_area(third_part);
    draw_visible_area(fourth_part);

    ImGui::EndChild();
}

void ppu_debugger::draw_affine_bg_map(const ppu::bg_affine& bg) noexcept
{
    ppu_bg_preferences& bg_prefs = prefs_->ppu_bg_preferences[bg.id];

    const auto& vram = access_private::vram_(ppu_engine_);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine_);
    sf::Image& buffer = bg_buffers_[bg.id];
    sf::Texture& texture = bg_textures_[bg.id];

    const usize tile_base = bg.cnt.char_base_block * 16_kb;
    const usize map_entry_base = bg.cnt.screen_entry_base_block * 2_kb;

    const sf::Vector2u map_total_block_dimensions{
      16_u32 * (1_u32 << bg.cnt.screen_size.get()),
      16_u32 * (1_u32 << bg.cnt.screen_size.get())
    };
    const sf::Vector2u map_total_dot_dimensions = map_total_block_dimensions * ppu::tile_dot_count;

    for(u32 ty = 0_u32; ty < map_total_block_dimensions.y; ++ty) {
        for(u32 tx = 0_u32; tx < map_total_block_dimensions.x; ++tx) {
            const u32 entry_idx = ty * map_total_block_dimensions.x + tx;
            const u8 tile_idx{memcpy<u8>(vram, map_entry_base + entry_idx)};

            for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                for(u32 px = 0_u32; px < ppu::tile_dot_count; ++px) {
                    const u8 color_idx = memcpy<u8>(vram, tile_base + tile_idx * 64_u32 + py * 8_u32 + px);
                    buffer.setPixel(
                      tx.get() * ppu::tile_dot_count + px.get(),
                      ty.get() * ppu::tile_dot_count + py.get(),
                      to_sf_color(call_private::palette_color_opaque(ppu_engine_, color_idx, 0_u8)));
                }
            }
        }
    }

    int& draw_scale = bg_prefs.render_scale;
    ImGui::SetNextItemWidth(150.f);
    ImGui::SliderInt("render scale", &draw_scale, 1, 4);

    if(!ImGui::BeginChild("#bgtexture", ImVec2{}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }

    texture.update(buffer);
    sf::Sprite bg_sprite{texture, sf::IntRect(0, 0, map_total_dot_dimensions.x, map_total_dot_dimensions.y)};
    bg_sprite.setScale(draw_scale, draw_scale);

    // draw the whole map in half alpha
    const ImVec2 img_start = ImGui::GetCursorScreenPos();
    ImGui::Image(bg_sprite);

    if(ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();

        const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        const u32::type tile_y = (mouse_pos.y - img_start.y) / (draw_scale * ppu::tile_dot_count);
        const u32::type tile_x = (mouse_pos.x - img_start.x) / (draw_scale * ppu::tile_dot_count);

        const sf::Sprite zoomed_tile{texture, sf::IntRect(
          tile_x * ppu::tile_dot_count,
          tile_y * ppu::tile_dot_count,
          ppu::tile_dot_count,
          ppu::tile_dot_count)};
        ImGui::Image(zoomed_tile, {128, 128});

        const u32 entry_idx = tile_y * map_total_block_dimensions.x + tile_x;
        const u8 tile_idx{memcpy<u8>(vram, map_entry_base + entry_idx)};

        ImGui::BeginGroup();
        ImGui::Text("map base addr: {:08X}", 0x0600'0000_u32 + map_entry_base + entry_idx);
        ImGui::Text("tile base addr: {:08X}", 0x0600'0000_u32 + tile_base + tile_idx * 64_u32);
        ImGui::Text("tile: {:02X}", tile_idx);
        ImGui::Text("x: {:02X} y: {:02X}", tile_x, tile_y);
        ImGui::EndGroup();

        ImGui::EndTooltip();
    }

    ImGui::EndChild();
}

void ppu_debugger::draw_bitmap_bg(const ppu::bg_affine& bg, const u32 mode) noexcept
{
    ppu_bg_preferences& bg_prefs = prefs_->ppu_bg_preferences[bg.id];

    const sf::Color backdrop = to_sf_color(call_private::palette_color_opaque(ppu_engine_, 0_u8, 0_u8));
    const auto& vram = access_private::vram_(ppu_engine_);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine_);
    auto& buffer = bg_buffers_[bg.id];
    auto& texture = bg_textures_[bg.id];

    ImGui::SetNextItemWidth(150.f);
    ImGui::SliderInt("render scale", &bg_prefs.render_scale, 1, 4);

    const auto draw_page = [&](u32 w, u32 h, u8 page, bool depth8bit) {
        const u32::type yoffset = page.get() * (ppu::screen_height + 2);

        for(u32 y : range(h)) {
            if(!depth8bit) {
                for(u32 x : range(w)) {
                    buffer.setPixel(x.get(), yoffset + y.get(), to_sf_color(ppu::color{
                      memcpy<u16>(vram, page * 40_kb + (y * w + x) * 2_u32)
                    }));
                }
            } else {
                for(u32 x : range(w)) {
                    buffer.setPixel(x.get(), yoffset + y.get(),
                      to_sf_color(call_private::palette_color_opaque(
                        ppu_engine_,
                        memcpy<u8>(vram, page * 40_kb + (y * w + x)), 0_u8)));
                }
            }

            if(w != ppu::screen_width) {
                for(u32 x : range(w, u32(ppu::screen_width))) {
                    buffer.setPixel(x.get(), yoffset + y.get(), backdrop);
                }
            }
        }

        if(h != ppu::screen_height) {
            for(u32 y : range(h, u32(ppu::screen_height))) {
                for(u32 x : range(ppu::screen_width)) {
                    buffer.setPixel(x.get(), yoffset + y.get(), backdrop);
                }
            }
        }

        sf::Sprite spr{texture, sf::IntRect(0, yoffset, ppu::screen_width, ppu::screen_height)};
        spr.setScale(bg_prefs.render_scale, bg_prefs.render_scale);
        return spr;
    };

    if(!ImGui::BeginChild("#bitmapbg", ImVec2{}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }

    switch(mode.get()) {
        case 3: {
            const sf::Sprite p = draw_page(240_u32, 160_u32, 0_u8, false);
            texture.update(buffer);
            ImGui::Image(p);
            break;
        }
        case 4: {
            const sf::Sprite p1 = draw_page(240_u32, 160_u32, 0_u8, true);
            const sf::Sprite p2 = draw_page(240_u32, 160_u32, 1_u8, true);

            texture.update(buffer);
            ImGui::BeginGroup();
            ImGui::Text("Page 0");
            ImGui::Image(p1);
            ImGui::EndGroup();

            if(bg_prefs.render_scale < 3) {
                ImGui::SameLine();
            }

            ImGui::BeginGroup();
            ImGui::Text("Page 1");
            ImGui::Image(p2);
            ImGui::EndGroup();
            break;
        }
        case 5: {
            const sf::Sprite p1 = draw_page(160_u32, 128_u32, 0_u8, false);
            const sf::Sprite p2 = draw_page(160_u32, 128_u32, 1_u8, false);

            texture.update(buffer);

            ImGui::BeginGroup();
            ImGui::Text("Page 0");
            ImGui::Image(p1);
            ImGui::EndGroup();

            if(bg_prefs.render_scale < 3) {
                ImGui::SameLine();
            }

            ImGui::BeginGroup();
            ImGui::Text("Page 1");
            ImGui::Image(p2);
            ImGui::EndGroup();
            break;
        }
        default:
            UNREACHABLE();
    }

    ImGui::EndChild();
}

void ppu_debugger::draw_bg_tiles() noexcept
{
    const auto& vram = access_private::vram_(ppu_engine_);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine_);

    static bool depth8bit = false;
    static int palette_idx = 0_u8;
    ImGui::Checkbox("8bit depth", &depth8bit);
    if(!depth8bit) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.f);
        ImGui::SliderInt("Palette idx", &palette_idx, 0, 15);
    }

    int& draw_scale = prefs_->ppu_bg_tiles_render_scale;
    ImGui::SetNextItemWidth(150.f);
    ImGui::SliderInt("render scale", &draw_scale, 1, 4);

    for(u32 ty : range(depth8bit ? 32_u32 : 64_u32)) {
        for(u32 tx : range(32_u32)) {
            const u32 t_idx = ty * 32_u32 + tx;
            if(depth8bit) {
                for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                    for(u32 px = 0_u32; px < ppu::tile_dot_count; ++px) {
                        const u8 color_idx = memcpy<u8>(vram, t_idx * 64_u32 + py * 8_u32 + px);
                        tiles_buffer_.setPixel(
                          tx.get() * ppu::tile_dot_count + px.get(),
                          ty.get() * ppu::tile_dot_count + py.get(),
                          to_sf_color(call_private::palette_color_opaque(ppu_engine_, color_idx, 0_u8)));
                    }
                }
            } else {
                for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                    for(u32 px = 0_u32; px < ppu::tile_dot_count; px += 2_u32) {
                        const u8 color_idxs = memcpy<u8>(vram, t_idx * 32_u32 + py * 4_u32 + px / 2_u32);
                        tiles_buffer_.setPixel(
                          tx.get() * ppu::tile_dot_count + px.get(),
                          ty.get() * ppu::tile_dot_count + py.get(),
                          to_sf_color(call_private::palette_color_opaque(
                            ppu_engine_, color_idxs & 0xF_u8, static_cast<u8::type>(palette_idx))));
                        tiles_buffer_.setPixel(
                          tx.get() * ppu::tile_dot_count + px.get() + 1,
                          ty.get() * ppu::tile_dot_count + py.get(),
                          to_sf_color(call_private::palette_color_opaque(
                            ppu_engine_, color_idxs >> 4_u8, static_cast<u8::type>(palette_idx))));
                    }
                }
            }
        }
    }
    tiles_texture_.update(tiles_buffer_);

    sf::Sprite tiles_sprite{tiles_texture_, sf::IntRect{0, 0, 256, depth8bit ? 256 : 512}};
    tiles_sprite.setScale(draw_scale, draw_scale);

    const ImVec2 img_start = ImGui::GetCursorScreenPos();
    if(ImGui::BeginChild("#tilestexture", ImVec2{}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Image(tiles_sprite, sf::Color::White, sf::Color::White);
        const float scroll_y = ImGui::GetScrollY();
        const float scroll_x = ImGui::GetScrollX();

        if(ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();

            const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            const u32::type tile_y = (mouse_pos.y - img_start.y + scroll_y) / (draw_scale * ppu::tile_dot_count);
            const u32::type tile_x = (mouse_pos.x - img_start.x + scroll_x) / (draw_scale * ppu::tile_dot_count);
            const u32 t_idx = tile_y * 32_u32 + tile_x;

            const sf::Sprite zoomed_tile{tiles_texture_, sf::IntRect(
              tile_x * ppu::tile_dot_count,
              tile_y * ppu::tile_dot_count,
              ppu::tile_dot_count,
              ppu::tile_dot_count)};
            ImGui::Image(zoomed_tile, {128, 128});
            ImGui::Text("tile idx: {:03X}", t_idx);
            ImGui::Text("address: {:08X}", 0x0600'0000_u32 + t_idx * (depth8bit ? 64_u32 : 32_u32));

            ImGui::EndTooltip();
        }

        ImGui::EndChild();
    }
}

void ppu_debugger::draw_obj_tiles() noexcept
{
    const auto& vram = access_private::vram_(ppu_engine_);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine_);

    static bool depth8bit = false;
    static int palette_idx = 0_u8;
    ImGui::Checkbox("8bit depth", &depth8bit);
    if(!depth8bit) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.f);
        ImGui::SliderInt("Palette idx", &palette_idx, 0, 15);
    }

    int& draw_scale = prefs_->ppu_obj_tiles_render_scale;
    ImGui::SetNextItemWidth(150.f);
    ImGui::SliderInt("render scale", &draw_scale, 1, 4);

    for(u32 ty : range(depth8bit ? 16_u32 : 32_u32)) {
        for(u32 tx : range(32_u32)) {
            const u32 t_idx = ty * 32_u32 + tx;

            if(depth8bit) {
                for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                    for(u32 px = 0_u32; px < ppu::tile_dot_count; ++px) {
                        const u8 color_idx = memcpy<u8>(vram, 0x1'0000_u32 + t_idx * 64_u32 + py * 8_u32 + px);
                        tiles_buffer_.setPixel(
                          tx.get() * ppu::tile_dot_count + px.get(),
                          ty.get() * ppu::tile_dot_count + py.get(),
                          to_sf_color(call_private::palette_color_opaque(ppu_engine_, color_idx, 16_u8)));
                    }
                }
            } else {
                for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                    for(u32 px = 0_u32; px < ppu::tile_dot_count; px += 2_u32) {
                        const u8 color_idxs = memcpy<u8>(vram, 0x1'0000_u32 + t_idx * 32_u32 + py * 4_u32 + px / 2_u32);
                        tiles_buffer_.setPixel(
                          tx.get() * ppu::tile_dot_count + px.get(),
                          ty.get() * ppu::tile_dot_count + py.get(),
                          to_sf_color(call_private::palette_color_opaque(
                            ppu_engine_, color_idxs & 0xF_u8, static_cast<u8::type>(palette_idx + 16_u8))));
                        tiles_buffer_.setPixel(
                          tx.get() * ppu::tile_dot_count + px.get() + 1,
                          ty.get() * ppu::tile_dot_count + py.get(),
                          to_sf_color(call_private::palette_color_opaque(
                            ppu_engine_, color_idxs >> 4_u8, static_cast<u8::type>(palette_idx + 16_u8))));
                    }
                }
            }
        }
    }
    tiles_texture_.update(tiles_buffer_);

    sf::Sprite tiles_sprite{tiles_texture_, sf::IntRect{0, 0, 256, depth8bit ? 128 : 256}};
    tiles_sprite.setScale(draw_scale, draw_scale);

    const ImVec2 img_start = ImGui::GetCursorScreenPos();
    if(ImGui::BeginChild("#tilestexture", ImVec2{}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Image(tiles_sprite, sf::Color::White, sf::Color::White);
        const float scroll_y = ImGui::GetScrollY();
        const float scroll_x = ImGui::GetScrollX();

        if(ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();

            const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            const u32::type tile_y = (mouse_pos.y - img_start.y + scroll_y) / (draw_scale * ppu::tile_dot_count);
            const u32::type tile_x = (mouse_pos.x - img_start.x + scroll_x) / (draw_scale * ppu::tile_dot_count);
            const u32 t_idx = tile_y * 32_u32 + tile_x;

            const sf::Sprite zoomed_tile{tiles_texture_, sf::IntRect(
              tile_x * ppu::tile_dot_count,
              tile_y * ppu::tile_dot_count,
              ppu::tile_dot_count,
              ppu::tile_dot_count)};
            ImGui::Image(zoomed_tile, {128, 128});
            ImGui::Text("tile idx: {:03X}", t_idx);
            ImGui::Text("address: {:08X}", 0x0601'0000_u32 + t_idx * (depth8bit ? 64_u32 : 32_u32));

            ImGui::EndTooltip();
        }

        ImGui::EndChild();
    }
}

void ppu_debugger::draw_obj() noexcept
{
    const auto& vram = access_private::vram_(ppu_engine_);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine_);
    const bool mapping_1d = dispcnt.obj_mapping_1d;

    const view<ppu::obj> obj_view{access_private::oam_(ppu_engine_)};
    const view<ppu::obj_affine> obj_affine_view{access_private::oam_(ppu_engine_)};

    for(u32 obj_y : range(16_u32)) {
        for(u32 obj_x : range(8_u32)) {
            u32 obj_idx = obj_y * 8_u32 + obj_x;

            sf::Texture& texture = obj_textures_[obj_idx];
            sf::Image& buffer = obj_buffers_[obj_idx];

            const ppu::obj& obj = obj_view[obj_idx];

            const u32 shape_idx = obj.attr0.shape_idx();
            auto dimension = shape_idx == 3_u32
              ? ppu::dimension<u8>{}
              : ppu::obj::dimensions[shape_idx][obj.attr1.size_idx()];
            const ppu::obj_attr0::blend_mode blend_mode = obj.attr0.blending();
            const ppu::obj_attr0::rendering_mode rendering_mode = obj.attr0.render_mode();
            const bool color_depth_8_bit = obj.attr0.color_depth_8bit();

            const bool hflip = obj.attr1.h_flipped();
            const bool vflip = obj.attr1.v_flipped();

            const u16 tile_idx = obj.attr2.tile_idx();
            const u8 palette_idx = obj.attr2.palette_idx();

            const bool is_rendered = rendering_mode != ppu::obj_attr0::rendering_mode::hidden
              && blend_mode != ppu::obj_attr0::blend_mode::prohibited
              && shape_idx != 3_u32;

            const ppu::dimension<u32> tile_dimens{
              dimension.h / ppu::tile_dot_count,
              dimension.v / ppu::tile_dot_count
            };

            if(is_rendered) {
                for(u32 ty = 0_u32; ty < tile_dimens.v; ++ty) {
                    for(u32 tx = 0_u32; tx < tile_dimens.h; ++tx) {
                        if(color_depth_8_bit) {
                            const u32 tile_num = mapping_1d
                              ? tile_idx + ty * (dimension.h / 4_u8) + tx * 2_u32
                              : tile_idx + ty * 32_u32 + tx * 2_u32;

                            for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                                for(u32 px = 0_u32; px < ppu::tile_dot_count; ++px) {
                                    const u8 color_idx = memcpy<u8>(vram, 0x1'0000_u32 + tile_num * 32_u32 + py * ppu::tile_dot_count + px);
                                    buffer.setPixel(
                                      tx.get() * ppu::tile_dot_count + px.get(),
                                      ty.get() * ppu::tile_dot_count + py.get(),
                                      to_sf_color(call_private::palette_color_opaque(ppu_engine_, color_idx, 16_u8)));
                                }
                            }
                        } else {
                            const u32 tile_num = mapping_1d
                              ? tile_idx + ty * (dimension.h / 8_u8) + tx
                              : tile_idx + ty * 32_u32 + tx;

                            for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                                for(u32 px = 0_u32; px < ppu::tile_dot_count; px += 2_u32) {
                                    const u8 color_idxs = memcpy<u8>(vram, 0x1'0000_u32 + tile_num * 32_u32 + (py * ppu::tile_dot_count / 2_u32) + (px / 2_u32));
                                    buffer.setPixel(
                                      tx.get() * ppu::tile_dot_count + px.get(),
                                      ty.get() * ppu::tile_dot_count + py.get(),
                                      to_sf_color(call_private::palette_color_opaque(
                                        ppu_engine_, color_idxs & 0xF_u8, palette_idx)));
                                    buffer.setPixel(
                                      tx.get() * ppu::tile_dot_count + px.get() + 1,
                                      ty.get() * ppu::tile_dot_count + py.get(),
                                      to_sf_color(call_private::palette_color_opaque(
                                        ppu_engine_, color_idxs >> 4_u8, palette_idx)));
                                }
                            }
                        }
                    }
                }
                texture.update(buffer);
            }

            if(dimension.v == 0) { dimension.v = 64_u8; }
            if(dimension.h == 0) { dimension.h = 64_u8; }

            const u8 magnification = 64_u8 / std::max(dimension.v, dimension.h);
            sf::Sprite obj_sprite{texture, sf::IntRect{0, 0, dimension.h.get(), dimension.v.get()}};
            obj_sprite.setScale(magnification.get() * (hflip ? -1 : 1), magnification.get() * (vflip ? -1 : 1));

            ImGui::Image(obj_sprite, is_rendered ? sf::Color::White : sf::Color{0xFFFFFF7F}, sf::Color::White);
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                obj_sprite.setScale(10.f * (hflip ? -1 : 1), 10.f * (vflip ? -1 : 1));
                ImGui::Image(obj_sprite);

                const u8 y = obj.attr0.y();
                const bool mosaic = obj.attr0.mosaic_enabled();
                const u16 x = obj.attr1.x();
                const u32 affine_idx = obj.attr1.affine_idx();
                const u32 priority = obj.attr2.priority();
                const ppu::obj_affine& obj_affine = obj_affine_view[affine_idx];

                ImGui::Text("size: {}x{}\n"
                  "y: {:02X}, x: {:03X}\n"
                  "hflip: {}, vflip: {}\n"
                  "rendering mode: {}\n"
                  "blending mode: {}\n"
                  "color depth: {}\n"
                  "tile idx: {:03X}\n"
                  "palette idx: {:02X}\n"
                  "affine idx: {:02X}\n"
                  "priority: {}\n"
                  "pa: {:04X} ({})\npb: {:04X} ({})\npc: {:04X} ({})\npd: {:04X} ({})",
                  dimension.v, dimension.h,
                  y, x, hflip, vflip, to_string_view(rendering_mode), to_string_view(blend_mode),
                  color_depth_8_bit ? "8bit" : "4bit", tile_idx, palette_idx - 16_u8,
                  affine_idx, priority,
                  make_unsigned(obj_affine.pa), matrix_elem_to_float(obj_affine.pa), make_unsigned(obj_affine.pb), matrix_elem_to_float(obj_affine.pb),
                  make_unsigned(obj_affine.pc), matrix_elem_to_float(obj_affine.pc), make_unsigned(obj_affine.pd), matrix_elem_to_float(obj_affine.pd));

                ImGui::EndTooltip();
            }

            if(float s = float(dimension.h.get()) / dimension.v.get(); s < 1.f) {
                ImGui::SameLine(0.f, 0.f);
                ImGui::Dummy(ImVec2((1.f - s) * 64.f, 0.f));
            }

            ImGui::SameLine(0.f, 10.f);
        }

        ImGui::NewLine();
    }
}

void ppu_debugger::draw_win_buffer() noexcept
{
    ImGui::ColorEdit3("OUT", prefs_->ppu_winout_color.data(), ImGuiColorEditFlags_NoInputs); ImGui::SameLine();
    ImGui::ColorEdit3("WIN0", prefs_->ppu_win0_color.data(), ImGuiColorEditFlags_NoInputs); ImGui::SameLine();
    ImGui::ColorEdit3("WIN1", prefs_->ppu_win1_color.data(), ImGuiColorEditFlags_NoInputs); ImGui::SameLine();
    ImGui::ColorEdit3("OBJ", prefs_->ppu_winobj_color.data(), ImGuiColorEditFlags_NoInputs);

    constexpr auto to_sf_color = [](array<float, 4>& f) {
        return sf::Color{
            static_cast<sf::Uint8>(f[0_u32] * 255.f),
            static_cast<sf::Uint8>(f[1_u32] * 255.f),
            static_cast<sf::Uint8>(f[2_u32] * 255.f),
            static_cast<sf::Uint8>(f[3_u32] * 255.f)
        };
    };

    for(u32 y : range(ppu::screen_height)) {
        for(u32 x : range(ppu::screen_width)) {
            const u32 idx = ppu::screen_width * y + x;
            switch(win_types_[idx]) {
                case win_type::out:
                    win_buffer_.setPixel(x.get(), y.get(), to_sf_color(prefs_->ppu_winout_color));
                    break;
                case win_type::w0:
                    win_buffer_.setPixel(x.get(), y.get(), to_sf_color(prefs_->ppu_win0_color));
                    break;
                case win_type::w1:
                    win_buffer_.setPixel(x.get(), y.get(), to_sf_color(prefs_->ppu_win1_color));
                    break;
                case win_type::obj:
                    win_buffer_.setPixel(x.get(), y.get(), to_sf_color(prefs_->ppu_winobj_color));
                    break;
            }
        }
    }

    win_texture_.update(win_buffer_);

    int& draw_scale = prefs_->ppu_win_render_scale;
    ImGui::SetNextItemWidth(150.f);
    ImGui::SliderInt("render scale", &draw_scale, 1, 4);

    sf::Sprite win_sprite{win_texture_};
    win_sprite.setScale(draw_scale, draw_scale);
    ImGui::Image(win_sprite);
}

} // namespace gba::debugger
