/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <chrono>
#include <thread>

#include <cxxopts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/spdlog.h>
#include <sdl2cpp/sdl_core.h>

#include <gba/core.h>
#include <gba/version.h>

#if WITH_DEBUGGER
  #include <gba_debugger/debugger.h>
#else
  #include <frontend/frontend.h>
#endif // WITH_DEBUGGGER

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
#if !WITH_DEBUGGER
        ("S,viewport-scale", "Scale of the viewport (not used if fullscreen is set), (240x160)*S", cxxopts::value<uint32_t>()->default_value("2"))
        ("V,initial-volume", "Initial volume of the frontend", cxxopts::value<float>()->default_value("0.7"))
        ("skip-bios", "Skips bios and starts the game directly")
#endif // WITH_DEBUGGGER
        ("bios", "BIOS binary path (looks for bios.bin if not provided)", cxxopts::value<std::string>()->default_value("bios.bin"))
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
        fmt::print("gameboiadvance v{}", gba::version);
        return EXIT_SUCCESS;
    }

    if(parsed["help"].as<bool>() || parsed["rom-path"].count() == 0) {
        fmt::print("{}", options.help());
        return EXIT_FAILURE;
    }

    const gba::fs::path bios_path = parsed["bios"].as<std::string>();
    if(!gba::fs::exists(bios_path) || gba::fs::is_empty(bios_path)) {
        fmt::print("bios file not found or empty: {}\n{}", bios_path.string(), options.help());
        return EXIT_FAILURE;
    }

    sdl::init();

    gba::core core{gba::fs::read_file(bios_path)};
    core.load_pak(parsed["rom-path"].as<std::vector<std::string>>().front());

    const auto cleanup_and_exit = []() {
        sdl::quit();
        return EXIT_SUCCESS;
    };

#if WITH_DEBUGGER
    gba::debugger::window window(&core);

    while(true) {
        if(!window.draw()) {
            return cleanup_and_exit();
        }
    }
#else
    gba::frontend::window frontend_window{
        &core,
        parsed["viewport-scale"].as<uint32_t>(),
        parsed["initial-volume"].as<float>(),
        parsed["skip-bios"].as<bool>()
    };

    while(true) {
        const gba::frontend::tick_result result = frontend_window.tick();
        switch(result) {
            case gba::frontend::tick_result::exiting:
                return cleanup_and_exit();
            case gba::frontend::tick_result::sleeping:
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(10ms);
                break;
            case gba::frontend::tick_result::ticking:
                break;
        }
    }
#endif // WITH_DEBUGGGER
}