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

// we cannot access private templates
template<typename BG>
[[nodiscard]] FORCEINLINE u32 map_entry_index_duplicate(const u32 tile_x, const u32 tile_y, const BG& bg) noexcept
{
    u32 n = tile_x + tile_y * 32_u32;
    if(tile_x >= 0x20_u32) { n += 0x03E0_u32; }
    if(tile_y >= 0x20_u32 && bg.cnt.screen_size == 3_u8) { n += 0x0400_u32; }
    return n;
}

} // namespace

ppu_debugger::ppu_debugger(ppu::engine* engine)
  : ppu_engine{engine}
{
    screen_buffer_.create(ppu::screen_width, ppu::screen_height);
    screen_texture_.create(ppu::screen_width, ppu::screen_height);
    for(u32 i = 0u; i < 4u; ++i) {
        bg_buffers_[i].create(1024, 1024);
        bg_textures_[i].create(1024, 1024);
    }

    engine->event_on_scanline.add_delegate({connect_arg<&ppu_debugger::on_scanline>, this});
    engine->event_on_vblank.add_delegate({connect_arg<&ppu_debugger::on_update_texture>, this});
}

void ppu_debugger::draw() noexcept
{
    if(ImGui::Begin("PPU")) {
        if(ImGui::BeginTabBar("#pputabs")) {
            const auto& dispcnt = access_private::dispcnt_(ppu_engine);
            const auto& palette = access_private::palette_ram_(ppu_engine);

            if(ImGui::BeginTabItem("Registers & Buffer")) {
                static int draw_scale = 2;
                ImGui::SetNextItemWidth(150.f);
                ImGui::SliderInt("render scale", &draw_scale, 1, 4);

                sf::Sprite screen{screen_texture_};
                screen.setScale(draw_scale, draw_scale);
                ImGui::Image(screen, sf::Color::White, sf::Color::White);

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("BG0")) {
                if(!dispcnt.enable_bg[0_usize]) {
                    ImGui::TextUnformatted("disabled");
                } else {
                    draw_regular_bg_map(access_private::bg0_(ppu_engine));
                }
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("BG1")) {
                if(!dispcnt.enable_bg[1_usize]) {
                    ImGui::TextUnformatted("disabled");
                } else {
                    draw_regular_bg_map(access_private::bg1_(ppu_engine));
                }
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("BG2")) {
                if(!dispcnt.enable_bg[2_usize]) {
                    ImGui::TextUnformatted("disabled");
                } else {
                    switch(dispcnt.bg_mode.get()) {
                        case 0:
                            draw_regular_bg_map(to_regular(access_private::bg2_(ppu_engine)));
                            break;
                        case 1: case 2:
                            draw_affine_bg_map(access_private::bg2_(ppu_engine));
                            break;
                        case 3: case 4: case 5:
                            draw_bitmap_bg(access_private::bg2_(ppu_engine), dispcnt.bg_mode);
                            break;
                        default:
                            ImGui::Text("invalid mode %d", dispcnt.bg_mode.get());
                            break;
                    }
                }
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("BG3")) {
                if(!dispcnt.enable_bg[3_usize]) {
                    ImGui::TextUnformatted("disabled");
                } else {
                    switch(dispcnt.bg_mode.get()) {
                        case 0:
                            draw_regular_bg_map(to_regular(access_private::bg3_(ppu_engine)));
                            break;
                        case 2:
                            draw_affine_bg_map(access_private::bg3_(ppu_engine));
                            break;
                        case 1: case 3: case 4: case 5:
                            ImGui::Text("not used in mode %d", dispcnt.bg_mode.get());
                            break;
                        default:
                            ImGui::Text("invalid mode %d", dispcnt.bg_mode.get());
                            break;
                    }
                }
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("OAM")) {
                draw_obj();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Palette View")) {
                const auto draw_palette = [&](const char* name, usize type_offset /*1 for obj*/) {
                    ImGui::TextUnformatted(name);
                    for(u32 palette_idx : range(16_u32)) {
                        for(u32 color_idx : range(16_u32)) {
                            const usize address = type_offset * 512_u32 + palette_idx * 32_u32 + color_idx * 2_u32;
                            const u16 bgr_color = memcpy<u16>(palette, address);
                            const ppu::color ppu_color{bgr_color};
                            const sf::Color sf_color(ppu_color.to_u32().get());

                            ImGui::ColorButton("", sf_color,
                              ImGuiColorEditFlags_NoBorder
                              | ImGuiColorEditFlags_NoAlpha
                              | ImGuiColorEditFlags_NoDragDrop
                              | ImGuiColorEditFlags_NoTooltip, ImVec2(20,20));
                            if(ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::ColorButton("##preview", sf_color, ImGuiColorEditFlags_NoTooltip, ImVec2{75.f, 75.f});
                                ImGui::SameLine();
                                ImGui::Text("address: %08X\nvalue: %04X\nR: %02X\nG: %02X\nB: %02X",
                                  address.get() + 0x0500'0000_u32,
                                  bgr_color.get(),
                                  (bgr_color & ppu::color::r_mask).get(),
                                  (bgr_color & ppu::color::g_mask).get() >> 5_u32,
                                  (bgr_color & ppu::color::b_mask).get() >> 10_u32);
                                ImGui::EndTooltip();
                            }

                            ImGui::SameLine(0, 4);
                        }
                        ImGui::NewLine();
                    }
                    ImGui::Spacing();
                    ImGui::Spacing();
                };

                draw_palette("BG", 0_usize);
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
    for(u32 x : range(ppu::screen_width)) {
        screen_buffer_.setPixel(x.get(), screen_y.get(), sf::Color{scanline[x].to_u32().get()});
    }
}

void ppu_debugger::on_update_texture() noexcept
{
    screen_texture_.update(screen_buffer_);
}

void ppu_debugger::draw_regular_bg_map(const ppu::bg_regular& bg) noexcept
{
    const auto& vram = access_private::vram_(ppu_engine);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine);
    sf::Image& buffer = bg_buffers_[bg.id];
    sf::Texture& texture = bg_textures_[bg.id];

    const usize tile_base = bg.cnt.char_base_block * 16_kb;
    const usize map_entry_base = bg.cnt.screen_entry_base_block * 2_kb;

    ImGui::BeginGroup();
    ImGui::Text("priority: {}", bg.cnt.priority.get());
    ImGui::Text("mosaic: {}", bg.cnt.mosaic_enabled);
    ImGui::Text("8bit depth: {}", bg.cnt.color_depth_8bit);
    ImGui::Text("tile base: {:08X}", tile_base + 0x0600'0000_u32);
    ImGui::Text("screen entry base: {:08X}", map_entry_base + 0x0600'0000_u32);
    ImGui::EndGroup();

    ImGui::SameLine(0.f, 64.f);

    ImGui::BeginGroup();
    static array<bool, 4> enable_visible_area{true, true, true, true};
    static array<bool, 4> enable_visible_border{true, true, true, true};
    static array<bool, 4> enable_window_area{true, true, true, true};
    ImGui::Checkbox("Enable visible area mask", &enable_visible_area[bg.id]);
    if(enable_visible_area[bg.id]) { ImGui::Checkbox("Enable visible area border", &enable_visible_border[bg.id]); }
    if(enable_visible_area[bg.id]) { ImGui::Checkbox("Enable window mask", &enable_window_area[bg.id]); }
    ImGui::Text("voffset: {:04X}", bg.voffset);
    ImGui::Text("hoffset: {:04X}", bg.hoffset);
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::Spacing();

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
                          sf::Color(call_private::palette_color(ppu_engine, color_idx, 0_u8).to_u32().get()));
                    }
                }
            } else {
                for(u32 py = 0_u32; py < ppu::tile_dot_count; ++py) {
                    for(u32 px = 0_u32; px < ppu::tile_dot_count; px += 2_u32) {
                        const u8 color_idxs = memcpy<u8>(vram, tile_base + entry.tile_idx() * 32_u32 + py * 4_u32 + px / 2_u32);
                        buffer.setPixel(
                          tx.get() * ppu::tile_dot_count + (entry.hflipped() ? 7 - px.get() : px.get()),
                          ty.get() * ppu::tile_dot_count + (entry.vflipped() ? 7 - py.get() : py.get()),
                          sf::Color(call_private::palette_color(ppu_engine, color_idxs & 0xF_u8, entry.palette_idx()).to_u32().get()));
                        buffer.setPixel(
                          tx.get() * ppu::tile_dot_count + (entry.hflipped() ? 6 - px.get() : px.get() + 1),
                          ty.get() * ppu::tile_dot_count + (entry.vflipped() ? 7 - py.get() : py.get()),
                          sf::Color(call_private::palette_color(ppu_engine, color_idxs >> 4_u8, entry.palette_idx()).to_u32().get()));
                    }
                }
            }
        }
    }

    static int draw_scale = 1;
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
    ImGui::Image(bg_sprite, !enable_visible_area[bg.id] ? sf::Color::White : sf::Color{0xFFFFFF7F});
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
        ImGui::Text("base: {:08X}", 0x0600'0000_u32 + tile_base
          + (tile_y * 32_u32 * block_size.h + tile_x)
            * (bg.cnt.color_depth_8bit ? 64_u32 : 32_u32));
        ImGui::Text("tile: {:02X} palette {:02X}", entry.tile_idx(), entry.palette_idx());
        ImGui::Text("x: {:02X} y: {:02X}", tile_x, tile_y);
        ImGui::Text("hflip: {}\nvflip: {}", entry.hflipped(), entry.vflipped());
        ImGui::EndGroup();

        ImGui::EndTooltip();
    }

    if(!enable_visible_area[bg.id]) {
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
        ImGui::Image(bg_sprite, sf::Color::White, enable_visible_border[bg.id] ? sf::Color::White : sf::Color::Transparent);
        ImGui::SetCursorScreenPos(img_end);
    };

    draw_visible_area(first_part);
    draw_visible_area(second_part);
    draw_visible_area(third_part);
    draw_visible_area(fourth_part);

    if(!enable_window_area[bg.id]) {
        ImGui::EndChild();
        return;
    }

    // todo draw the window tint

    ImGui::EndChild();
}

void ppu_debugger::draw_affine_bg_map(const ppu::bg_affine& bg) noexcept
{
    const auto& vram = access_private::vram_(ppu_engine);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine);
    sf::Image& buffer = bg_buffers_[bg.id];
    sf::Texture& texture = bg_textures_[bg.id];

    // todo
}

void ppu_debugger::draw_bitmap_bg(const ppu::bg_affine& bg, const u32 mode) noexcept
{
    const sf::Color backdrop(call_private::palette_color_opaque(ppu_engine, 0_u8, 0_u8).to_u32().get());
    const auto& vram = access_private::vram_(ppu_engine);
    const auto& dispcnt = access_private::dispcnt_(ppu_engine);
    auto& buffer = bg_buffers_[bg.id];
    auto& texture = bg_textures_[bg.id];

    ImGui::Text("priority: {}", bg.cnt.priority);
    ImGui::Text("mosaic: {}", bg.cnt.mosaic_enabled);
    ImGui::Text("8bit depth: {}", mode == 4_u8);

    ImGui::Spacing();
    ImGui::Spacing();

    const auto draw_page = [&](u32 w, u32 h, u8 page, bool depth8bit) {
        const u32::type yoffset = page.get() * (ppu::screen_height + 2);

        for(u32 y : range(h)) {
            if(!depth8bit) {
                for(u32 x : range(w)) {
                    buffer.setPixel(x.get(), yoffset + y.get(), sf::Color(ppu::color{
                      memcpy<u16>(vram, page * 40_kb + (y * w + x) * 2_u32)
                    }.to_u32().get()));
                }
            } else {
                for(u32 x : range(w)) {
                    buffer.setPixel(x.get(), yoffset + y.get(),
                      sf::Color(call_private::palette_color_opaque(ppu_engine, memcpy<u8>(vram,
                        page * 40_kb + (y * w + x)), 0_u8).to_u32().get()));
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
        spr.setScale(2, 2);
        return spr;
    };

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
            ImGui::Text("Page 0");
            ImGui::Image(p1);
            ImGui::Text("Page 1");
            ImGui::Image(p2);
            break;
        }
        case 5: {
            const sf::Sprite p1 = draw_page(160_u32, 128_u32, 0_u8, false);
            const sf::Sprite p2 = draw_page(160_u32, 128_u32, 1_u8, false);

            texture.update(buffer);
            ImGui::Text("Page 0");
            ImGui::Image(p1);
            ImGui::Text("Page 1");
            ImGui::Image(p2);
            break;
        }
        default:
            UNREACHABLE();
    }
}

void ppu_debugger::draw_obj() noexcept
{
    // todo
}

} // namespace gba::debugger
