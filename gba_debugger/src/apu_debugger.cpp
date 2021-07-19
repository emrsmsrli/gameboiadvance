/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/apu_debugger.h>

#include <access_private.h>
#include <imgui.h>
#include <implot.h>

#include <gba/apu/apu.h>
#include <gba_debugger/debugger_helpers.h>
#include <gba_debugger/preferences.h>

ACCESS_PRIVATE_FIELD(gba::apu::engine, bool, power_on_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::pulse_channel, channel_1_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::pulse_channel, channel_2_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::wave_channel, channel_3_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::noise_channel, channel_4_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::fifo, fifo_a_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::fifo, fifo_b_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::cnt, control_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::soundbias, soundbias_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::u8, frame_sequencer_)
ACCESS_PRIVATE_FIELD(gba::apu::engine, gba::apu::sound_buffer<gba::apu::stereo_sample<float>>, buffer_)

ACCESS_PRIVATE_FIELD(gba::apu::sound_buffer<gba::apu::stereo_sample<float>>, gba::usize, capacity_)
ACCESS_PRIVATE_FIELD(gba::apu::sound_buffer<gba::apu::stereo_sample<float>>, gba::usize, write_idx_)
ACCESS_PRIVATE_FIELD(gba::apu::sound_buffer<gba::apu::stereo_sample<float>>, gba::vector<gba::apu::stereo_sample<float>>, buffer_)

using fifo_data_t = gba::array<gba::u8, 32>;
ACCESS_PRIVATE_FIELD(gba::apu::fifo, fifo_data_t, data_)
ACCESS_PRIVATE_FIELD(gba::apu::fifo, gba::u32, read_idx_)
ACCESS_PRIVATE_FIELD(gba::apu::fifo, gba::u32, write_idx_)
ACCESS_PRIVATE_FIELD(gba::apu::fifo, gba::u32, size_)
ACCESS_PRIVATE_FIELD(gba::apu::fifo, gba::u8, latch_)

namespace gba::debugger {

namespace {

void draw_channel_buffer(const char* name, const vector<apu::stereo_sample<float>>& buffer,
  const usize capacity, const usize write_idx, const u32 terminal) noexcept
{
    const float* data = reinterpret_cast<const float*>(buffer.data());
    ImPlot::PlotLine(name, data, capacity.get(), 1.0, 0.0,
      write_idx.get() + terminal.get(), sizeof(float) * 2);
}

void draw_envelope(const apu::envelope& env) noexcept
{
    ImGui::Spacing();
    ImGui::TextUnformatted("Envelope"); ImGui::Separator();
    if(ImGui::BeginTable("apu_env", 2)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("timer:       {:08X}", env.timer);
        ImGui::TableNextColumn(); ImGui::Text("period:      {:02X}", env.period);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("initial vol: {:02X}", env.initial_volume);
        ImGui::TableNextColumn(); ImGui::Text("direction:   {}", [&]() {
            switch(env.direction) {
                case apu::envelope::mode::increase: return "increase";
                case apu::envelope::mode::decrease: return "decrease";
                default: UNREACHABLE();
            }
        }());
        ImGui::EndTable();
    }
}

void draw_freq_data(const apu::frequency_data& freq_data) noexcept
{
    ImGui::Spacing();
    ImGui::TextUnformatted("Frequency Data"); ImGui::Separator();
    if(ImGui::BeginTable("apu_freq_data", 2)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("samplerate:  {:04X}", freq_data.sample_rate);
        ImGui::TableNextColumn(); ImGui::Text("use counter: {}", freq_data.freq_control.use_counter);
        ImGui::EndTable();
    }
}

void draw_freq_control(const apu::frequency_control& freq_ctrl) noexcept
{
    ImGui::Spacing();
    ImGui::TextUnformatted("Frequency Data"); ImGui::Separator();
    ImGui::Text("use counter: {}", freq_ctrl.use_counter);
}

void draw_pulse(const apu::pulse_channel& ch, const bool no_sweep) noexcept
{
    if(ImGui::BeginTable("apu_pulse", 2)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("volume:       {:02X}", ch.volume);
        ImGui::TableNextColumn(); ImGui::Text("length:       {:02X}", ch.length_counter);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("wave idx:     {:02X}", ch.waveform_duty_index);
        ImGui::TableNextColumn(); ImGui::Text("output:       {:02X}", ch.output);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("enable:       {}", ch.enabled);
        ImGui::TableNextColumn(); ImGui::Text("dac enable:   {}", ch.dac_enabled);

        ImGui::EndTable();
    }

    if(!no_sweep) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Sweep"); ImGui::Separator();

        if(ImGui::BeginTable("apu_sweep", 2)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("timer:       {:08X}", ch.swp.timer);
            ImGui::TableNextColumn(); ImGui::Text("shadow:      {:02X}", ch.swp.shadow);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("period:      {:02X}", ch.swp.period);
            ImGui::TableNextColumn(); ImGui::Text("direction:   {}", [&]() {
                switch(ch.swp.direction) {
                    case apu::sweep::mode::increase: return "increase";
                    case apu::sweep::mode::decrease: return "decrease";
                    default: UNREACHABLE();
                }
            }());
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("shift count: {}", ch.swp.shift_count);
            ImGui::TableNextColumn(); ImGui::Text("enable:      {}", ch.swp.enabled);

            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Wave Data"); ImGui::Separator();
    if(ImGui::BeginTable("apu_wave_data", 2)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("duty:        {:02X}", ch.wav_data.duty);
        ImGui::TableNextColumn(); ImGui::Text("length:      {:02X}", ch.wav_data.sound_length);
        ImGui::EndTable();
    }

    draw_envelope(ch.env);
    draw_freq_data(ch.freq_data);
}

} // namespace

apu_debugger::apu_debugger(apu::engine* apu_engine, preferences* prefs) noexcept
  : prefs_{prefs},
    apu_engine_{apu_engine}
{
    ram_viewer_.Cols = 8;
    ram_viewer_.OptMidColsCount = 4;
    ram_viewer_.ReadOnly = true;
    ram_viewer_.OptShowOptions = false;
    ram_viewer_.OptShowDataPreview = false;
    ram_viewer_.OptShowHexII = false;
    ram_viewer_.OptShowAscii = false;

    apu::sound_buffer<apu::stereo_sample<float>>& sound_buffer = access_private::buffer_(apu_engine_);
    sound_buffer.on_write.add_delegate({connect_arg<&apu_debugger::on_sample_written>, this});
}

void apu_debugger::draw() noexcept
{
    if(!ImGui::Begin("APU", nullptr, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }

    if(ImGui::BeginTabBar("#apubars")) {
        const apu::cnt& control = access_private::control_(apu_engine_);
        const apu::sound_buffer<apu::stereo_sample<float>>& sound_buffer = access_private::buffer_(apu_engine_);
        const usize sound_buffer_capacity = access_private::capacity_(sound_buffer);
        const usize sound_buffer_write_idx = access_private::write_idx_(sound_buffer);

        if(ImGui::BeginTabItem("Mixer")) {
            ImGui::Text("frame sequencer: {}", access_private::frame_sequencer_(apu_engine_));
            ImGui::Text("buffer write idx: {}", sound_buffer_write_idx);
            ImGui::Text("buffer capacity: {}", sound_buffer_capacity);
            ImGui::Separator();
            ImGui::Text("power: {}", access_private::power_on_(apu_engine_));
            ImGui::Text("psg master volume: {:02X}", control.psg_volume);

            if(ImGui::BeginTable("mixertable", 2)) {
                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

                ImGui::TableNextColumn(); ImGui::TextUnformatted("left");
                ImGui::TableNextColumn(); ImGui::TextUnformatted("right");

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("volume: {:02X}", control.volumes[apu::terminal::left]);
                ImGui::TableNextColumn(); ImGui::Text("volume: {:02X}", control.volumes[apu::terminal::right]);
                for(u32 i = 0_u32; i < 4_u32; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("ch {} enable: {}", i + 1_u32, control.psg_enables[apu::terminal::left][i]);
                    ImGui::TableNextColumn(); ImGui::Text("ch {} enable: {}", i + 1_u32, control.psg_enables[apu::terminal::right][i]);
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("fifo a enable: {}", control.fifo_a.enables[apu::terminal::left]);
                ImGui::TableNextColumn(); ImGui::Text("fifo a enable: {}", control.fifo_a.enables[apu::terminal::right]);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("fifo b enable: {}", control.fifo_b.enables[apu::terminal::left]);
                ImGui::TableNextColumn(); ImGui::Text("fifo b enable: {}", control.fifo_b.enables[apu::terminal::right]);

                ImGui::EndTable();
            }

            struct channel_draw_opts {
                const char* name;
                const vector<apu::stereo_sample<float>>& buffer;
            };

            static array channel_draw_options{
              channel_draw_opts{"Buffer", access_private::buffer_(sound_buffer)},
              channel_draw_opts{"Channel1", sound_buffer_1_},
              channel_draw_opts{"Channel2", sound_buffer_2_},
              channel_draw_opts{"Channel3", sound_buffer_3_},
              channel_draw_opts{"Channel4", sound_buffer_4_},
              channel_draw_opts{"FIFO A", sound_buffer_fifo_a_},
              channel_draw_opts{"FIFO B", sound_buffer_fifo_b_},
            };

            ImGui::Spacing();
            if(ImGui::Button("Select all")) {
                prefs_->apu_enabled_channel_graphs = 0b1111111;
            }

            ImGui::SameLine();
            enumerate(channel_draw_options, [&](usize idx, channel_draw_opts& opt) {
                bool enabled = bit::test(u32(prefs_->apu_enabled_channel_graphs), narrow<u8>(idx));
                ImGui::Checkbox("", &enabled);
                if(ImGui::IsItemClicked()) { // hack
                    prefs_->apu_enabled_channel_graphs ^= 1 << idx.get();
                }
                if(ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(opt.name);
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
            });
            ImGui::NewLine();

            const float draw_width = ImGui::GetWindowContentRegionWidth() / 2.f - 2.f;
            if(draw_width > 100.f) {
                for(u32 terminal : range(apu::terminal::count)) {
                    ImPlot::SetNextPlotLimitsX(0.0, sound_buffer_capacity.get(), ImGuiCond_Always);
                    ImPlot::SetNextPlotLimitsY(-1.0, 1.0, ImGuiCond_Always);
                    if(ImPlot::BeginPlot(terminal == apu::terminal::right ? "Right" : "Left",
                      nullptr, nullptr, ImVec2{draw_width, 250.f},
                      ImPlotFlags_NoMenus, ImPlotAxisFlags_RangeFit, ImPlotAxisFlags_Lock)) {
                        enumerate(channel_draw_options, [&](usize idx, channel_draw_opts& opt) {
                            const bool enabled = bit::test(u32(prefs_->apu_enabled_channel_graphs), narrow<u8>(idx));
                            if(enabled) {
                                draw_channel_buffer(opt.name, opt.buffer, sound_buffer_capacity, sound_buffer_write_idx, terminal);
                            }
                        });
                        ImPlot::EndPlot();
                    }

                    ImGui::SameLine(0.f, 2.f);
                }
            }

            ImGui::EndTabItem();
        }

        const float draw_width = ImGui::GetWindowContentRegionWidth();
        if(ImGui::BeginTabItem("Channel 1")) {
            draw_pulse(access_private::channel_1_(apu_engine_), false);

            ImPlot::SetNextPlotLimitsX(0.0, sound_buffer_capacity.get(), ImGuiCond_Always);
            ImPlot::SetNextPlotLimitsY(-1.0, 1.0, ImGuiCond_Always);
            if(ImPlot::BeginPlot("Channel 1", nullptr, nullptr, ImVec2{draw_width, 250.f},
              ImPlotFlags_NoMenus, ImPlotAxisFlags_RangeFit, ImPlotAxisFlags_Lock)) {
                draw_channel_buffer("Left", sound_buffer_1_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::left);
                draw_channel_buffer("Right", sound_buffer_1_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::right);
                ImPlot::EndPlot();
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Channel 2")) {
            draw_pulse(access_private::channel_1_(apu_engine_), true);

            ImPlot::SetNextPlotLimitsX(0.0, sound_buffer_capacity.get(), ImGuiCond_Always);
            ImPlot::SetNextPlotLimitsY(-1.0, 1.0, ImGuiCond_Always);
            if(ImPlot::BeginPlot("Channel 2", nullptr, nullptr, ImVec2{draw_width, 250.f},
              ImPlotFlags_NoMenus, ImPlotAxisFlags_RangeFit, ImPlotAxisFlags_Lock)) {
                draw_channel_buffer("Left", sound_buffer_2_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::left);
                draw_channel_buffer("Right", sound_buffer_2_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::right);
                ImPlot::EndPlot();
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Channel 3")) {
            apu::wave_channel& ch = access_private::channel_3_(apu_engine_);
            if(ImGui::BeginTable("apu_ch3", 2)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("length:         {:02X}", ch.sound_length);
                ImGui::TableNextColumn(); ImGui::Text("output level:   {:02X}", ch.output_level);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("force 75% vol:  {}", ch.force_output_level);
                ImGui::TableNextColumn(); ImGui::Text("length counter: {:08X}", ch.length_counter);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("sample idx:     {:02X}", ch.sample_index);
                ImGui::TableNextColumn(); ImGui::Text("output:         {:02X}", ch.output);

                ImGui::EndTable();
            }

            draw_freq_data(ch.freq_data);

            if(ImGui::BeginTable("apu_ch3_pattern", 2)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("wave bank dim:  {}D", bit::from_bool(ch.wave_bank_2d) + 1_u32);
                ImGui::TableNextColumn(); ImGui::Text("wave bank:      {:02X}", ch.wave_bank);
                ImGui::EndTable();
            }

            static constexpr array bank_names{"auto", "0", "1"};
            static int bank_name = 0;

            ImGui::SetNextItemWidth(200.f);
            ImGui::Combo("select bank to view", &bank_name, bank_names.data(), bank_names.size().get());
            const u8 bank = bank_name == 0 ? ch.wave_bank : static_cast<u8::type>(bank_name - 1_u8);
            auto& contents = ch.wave_ram[bank];
            ImGui::BeginChild("ch3", ImVec2{ImGui::GetContentRegionAvailWidth(), 30.f}, false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
            ram_viewer_.DrawContents(contents.data(), contents.size().get(), 0x0400'0090);
            ImGui::EndChild();

            ImPlot::SetNextPlotLimitsX(0.0, sound_buffer_capacity.get(), ImGuiCond_Always);
            ImPlot::SetNextPlotLimitsY(-1.0, 1.0, ImGuiCond_Always);
            if(ImPlot::BeginPlot("Channel 3", nullptr, nullptr, ImVec2{draw_width, 250.f},
              ImPlotFlags_NoMenus, ImPlotAxisFlags_RangeFit, ImPlotAxisFlags_Lock)) {
                draw_channel_buffer("Left", sound_buffer_3_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::left);
                draw_channel_buffer("Right", sound_buffer_3_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::right);
                ImPlot::EndPlot();
            }

            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Channel 4")) {
            apu::noise_channel& ch = access_private::channel_4_(apu_engine_);
            if(ImGui::BeginTable("apu_ch4", 2)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("length:       {:08X}", ch.sound_length);
                ImGui::TableNextColumn(); ImGui::Text("lfsr:         {:04X}", ch.lfsr);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("volume:       {:02X}", ch.volume);
                ImGui::TableNextColumn(); ImGui::Text("output:       {:02X}", ch.output);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("enable:       {}", ch.enabled);
                ImGui::TableNextColumn(); ImGui::Text("dac enable:   {}", ch.dac_enabled);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("sound length: {:02X}", ch.sound_length);
                ImGui::EndTable();
            }

            draw_envelope(ch.env);
            draw_freq_control(ch.freq_control);

            ImGui::Spacing();
            ImGui::TextUnformatted("Polynomial Counter"); ImGui::Separator();
            if(ImGui::BeginTable("apu_poly", 2)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("divider:      {:02X}", ch.polynomial_cnt.dividing_ratio);
                ImGui::TableNextColumn(); ImGui::Text("shift freq:   {:02X}", ch.polynomial_cnt.shift_clock_frequency);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("7bit counter: {}", ch.polynomial_cnt.has_7_bit_counter_width);
                ImGui::EndTable();
            }

            ImPlot::SetNextPlotLimitsX(0.0, sound_buffer_capacity.get(), ImGuiCond_Always);
            ImPlot::SetNextPlotLimitsY(-1.0, 1.0, ImGuiCond_Always);
            if(ImPlot::BeginPlot("Channel 4", nullptr, nullptr, ImVec2{draw_width, 250.f},
              ImPlotFlags_NoMenus, ImPlotAxisFlags_RangeFit, ImPlotAxisFlags_Lock)) {
                draw_channel_buffer("Left", sound_buffer_4_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::left);
                draw_channel_buffer("Right", sound_buffer_4_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::right);
                ImPlot::EndPlot();
            }

            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("FIFO")) {
            if(ImGui::BeginTable("apu_fifo", 2)) {
                apu::fifo& fifo_a = access_private::fifo_a_(apu_engine_);
                apu::fifo& fifo_b = access_private::fifo_b_(apu_engine_);
                fifo_data_t& fifo_a_data = access_private::data_(fifo_a);
                fifo_data_t& fifo_b_data = access_private::data_(fifo_b);

                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                ImGui::TableNextColumn(); ImGui::TextUnformatted("FIFO A");
                ImGui::TableNextColumn(); ImGui::TextUnformatted("FIFO B");
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("volume:         {}", control.fifo_a.full_volume ? "100%" : "50%");
                ImGui::TableNextColumn(); ImGui::Text("volume:         {}", control.fifo_b.full_volume ? "100%" : "50%");
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("right enable:   {}", control.fifo_a.enables[apu::terminal::right]);
                ImGui::TableNextColumn(); ImGui::Text("right enable:   {}", control.fifo_b.enables[apu::terminal::right]);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("left enable:    {}", control.fifo_a.enables[apu::terminal::left]);
                ImGui::TableNextColumn(); ImGui::Text("left enable:    {}", control.fifo_b.enables[apu::terminal::left]);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("timer id:       {}", control.fifo_a.selected_timer_id);
                ImGui::TableNextColumn(); ImGui::Text("timer id:       {}", control.fifo_b.selected_timer_id);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Separator();
                ImGui::Text("size:           {}", access_private::size_(fifo_a));
                ImGui::TableNextColumn(); ImGui::Separator();
                ImGui::Text("size:           {}", access_private::size_(fifo_b));
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("read idx:       {}", access_private::read_idx_(fifo_a));
                ImGui::TableNextColumn(); ImGui::Text("read idx:       {}", access_private::read_idx_(fifo_b));
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("write idx:      {}", access_private::write_idx_(fifo_a));
                ImGui::TableNextColumn(); ImGui::Text("write idx:      {}", access_private::write_idx_(fifo_b));
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("latch:          {}", access_private::latch_(fifo_a));
                ImGui::TableNextColumn(); ImGui::Text("latch:          {}", access_private::latch_(fifo_b));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::BeginChild("fifoa", ImVec2{ImGui::GetContentRegionAvailWidth(), 60.f}, false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
                ram_viewer_.DrawContents(fifo_a_data.data(), fifo_a_data.size().get());
                ImGui::EndChild();

                ImGui::TableNextColumn();
                ImGui::BeginChild("fifob", ImVec2{ImGui::GetContentRegionAvailWidth(), 60.f}, false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
                ram_viewer_.DrawContents(fifo_b_data.data(), fifo_b_data.size().get());
                ImGui::EndChild();

                ImGui::EndTable();
            }

            const float fifo_draw_width = draw_width / 2.f - 2.f;
            ImPlot::SetNextPlotLimitsX(0.0, sound_buffer_capacity.get(), ImGuiCond_Always);
            ImPlot::SetNextPlotLimitsY(-1.0, 1.0, ImGuiCond_Always);
            if(ImPlot::BeginPlot("FIFO A", nullptr, nullptr, ImVec2{fifo_draw_width, 250.f},
              ImPlotFlags_NoMenus, ImPlotAxisFlags_RangeFit, ImPlotAxisFlags_Lock)) {
                draw_channel_buffer("Left", sound_buffer_fifo_a_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::left);
                draw_channel_buffer("Right", sound_buffer_fifo_a_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::right);
                ImPlot::EndPlot();
            }

            ImGui::SameLine(0.f, 2.f);
            ImPlot::SetNextPlotLimitsX(0.0, sound_buffer_capacity.get(), ImGuiCond_Always);
            ImPlot::SetNextPlotLimitsY(-1.0, 1.0, ImGuiCond_Always);
            if(ImPlot::BeginPlot("FIFO B", nullptr, nullptr, ImVec2{fifo_draw_width, 250.f},
              ImPlotFlags_NoMenus, ImPlotAxisFlags_RangeFit, ImPlotAxisFlags_Lock)) {
                draw_channel_buffer("Left", sound_buffer_fifo_b_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::left);
                draw_channel_buffer("Right", sound_buffer_fifo_b_, sound_buffer_capacity, sound_buffer_write_idx, apu::terminal::right);
                ImPlot::EndPlot();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void apu_debugger::on_sample_written(usize idx) noexcept
{
    const auto& ch1 = access_private::channel_1_(apu_engine_);
    const auto& ch2 = access_private::channel_2_(apu_engine_);
    const auto& ch3 = access_private::channel_3_(apu_engine_);
    const auto& ch4 = access_private::channel_4_(apu_engine_);
    const auto& f_a = access_private::fifo_a_(apu_engine_);
    const auto& f_b = access_private::fifo_b_(apu_engine_);
    const auto& ctrl = access_private::control_(apu_engine_);
    static constexpr array<i16, 2> dma_volume_tab = {2_i16, 4_i16};

    sound_buffer_1_[idx] = {
      ch1.output.get() / float(0x80),
      ch1.output.get() / float(0x80),
    };
    sound_buffer_2_[idx] = {
      ch2.output.get() / float(0x80),
      ch2.output.get() / float(0x80),
    };
    sound_buffer_3_[idx] = {
      ch3.output.get() / float(0x80),
      ch3.output.get() / float(0x80),
    };
    sound_buffer_4_[idx] = {
      ch4.output.get() / float(0x80),
      ch4.output.get() / float(0x80),
    };
    sound_buffer_fifo_a_[idx] = {
      (make_signed(f_a.latch()) * dma_volume_tab[bit::from_bool(ctrl.fifo_a.full_volume)]).get() / float(0x200),
      (make_signed(f_a.latch()) * dma_volume_tab[bit::from_bool(ctrl.fifo_a.full_volume)]).get() / float(0x200),
    };
    sound_buffer_fifo_b_[idx] = {
      (make_signed(f_b.latch()) * dma_volume_tab[bit::from_bool(ctrl.fifo_b.full_volume)]).get() / float(0x200),
      (make_signed(f_b.latch()) * dma_volume_tab[bit::from_bool(ctrl.fifo_b.full_volume)]).get() / float(0x200),
    };
}

} // namespace gba::debugger
