/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <cxxopts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/spdlog.h>
#include <sdl2cpp/sdl_core.h>

#include <gba/core.h>
#include <gba/version.h>
#include <gba_debugger/debugger.h>

int main(int argc, char** argv)
{
#if SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_OFF
    static constexpr const char* log_pattern = "[%H:%M:%S:%e] [%s:%#] [%^%L%$] %v";

    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern(log_pattern);
    spdlog::set_default_logger(spdlog::stdout_color_st("core"));
#endif // SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_OFF

    cxxopts::Options options("gameboiadvance", "An excellent Gameboy Advance emulator");
    options
      .show_positional_help()
      .allow_unrecognised_options()
      .add_options()
        ("v,version", "Print version and exit")
        ("h,help", "Show this help text")
#if SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_OFF
        ("enable-file-log", "Enables file logging", cxxopts::value<bool>()->default_value("true"))
#endif // SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_OFF
        ("fullscreen", "Enable fullscreen")
        ("S,viewport-scale", "Scale of the viewport (not used if fullscreen is set), (240x160)*S", cxxopts::value<uint32_t>()->default_value("2"))
        ("bios", "BIOS binary path", cxxopts::value<std::string>())
        ("rom-path", "Rom path or directory", cxxopts::value<std::vector<std::string>>());

    options.parse_positional("rom-path");

    const auto parsed = options.parse(argc, argv);

#if SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_OFF
    if(parsed["enable-file-log"].as<bool>()) {
        auto file_log_sink = std::make_shared<spdlog::sinks::daily_file_sink_st>("logs/gba.log", 0, 0, false, 5);
        file_log_sink->set_pattern(log_pattern);
        spdlog::default_logger()->sinks().push_back(std::move(file_log_sink));
    }
#endif // SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_OFF

    if(parsed["version"].as<bool>()) {
        fmt::print(stdout, "gameboiadvance v{}", gba::version);
        return 0;
    }

    if(parsed["help"].as<bool>() || parsed["rom-path"].count() == 0) {
        fmt::print(stdout, "{}", options.help());
        return 0;
    }

    sdl::init();

    gba::vector<gba::u8> bios;
    if(parsed["bios"].count() != 0) {
        bios = gba::fs::read_file(parsed["bios"].as<std::string>());
    }

    gba::core g{std::move(bios)};
    g.load_pak(parsed["rom-path"].as<std::vector<std::string>>().front());

    gba::debugger::window window(&g);

    while(true) {
        if(!window.draw()) {
            break;
        }
    }

    sdl::quit();
    return 0;
}