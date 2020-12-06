/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <cxxopts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <gba/gba.h>
#include <gba/version.h>

int main(int argc, char** argv) {
    cxxopts::Options options("gameboiadvance", "An excellent Gameboy Advance emulator");
    options
      .show_positional_help()
      .allow_unrecognised_options()
      .add_options()
        ("v,version", "Print version and exit")
        ("h,help", "Show this help text")
        ("L,log-level", "Logging level (trace, debug, info, warn, err, critical, off)", cxxopts::value<std::string>()->default_value("off"))
        ("fullscreen", "Enable fullscreen")
        ("S,viewport-scale", "Scale of the viewport (not used if fullscreen is set), (240x160)*S", cxxopts::value<uint32_t>()->default_value("2"))
        ("rom-path", "Rom path or directory", cxxopts::value<std::vector<std::string>>());

    options.parse_positional("rom-path");

    const auto parsed = options.parse(argc, argv);

    if(parsed["version"].as<bool>()) {
        fmt::print(stdout, "gameboiadvance v{}", gba::version);
        return 0;
    }

    if(parsed["help"].as<bool>() || parsed["rom-path"].count() == 0) {
        fmt::print(stdout, "{}", options.help());
        return 0;
    }

#if SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_OFF
    spdlog::set_default_logger(spdlog::stdout_color_st("  core  "));
    spdlog::set_pattern("[%H:%M:%S:%e] [%s:%#] [%^%L%$] %v");
    spdlog::set_level(spdlog::level::from_str(parsed["log-level"].as<std::string>()));
#endif

    gba::gba g;
    g.load_pak(parsed["rom-path"].as<std::vector<std::string>>().front());

    return 0;
}