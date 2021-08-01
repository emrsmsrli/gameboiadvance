cmake_minimum_required(VERSION 3.16)
project(gba VERSION 0.8.7 LANGUAGES CXX)

configure_file(cmake/version.h.in
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gba/version.h)

add_library(${PROJECT_NAME} STATIC
        src/core_bus.cpp
        src/apu/apu.cpp
        src/apu/apu_pulse_channel.cpp
        src/apu/apu_noise_channel.cpp
        src/apu/apu_wave_channel.cpp
        src/cpu/cpu.cpp
        src/cpu/arm7tdmi.cpp
        src/cpu/arm7tdmi_alu.cpp
        src/cpu/arm7tdmi_io.cpp
        src/cpu/dma_controller.cpp
        src/cpu/timer.cpp
        src/cpu/irq_controller_handle.cpp
        src/ppu/ppu.cpp
        src/ppu/ppu_render.cpp
        src/ppu/ppu_window.cpp
        src/cartridge/backup.cpp
        src/cartridge/gamepak.cpp
        src/cartridge/rtc.cpp
        src/cartridge/gamepak_db.cpp
        src/helper/filesystem.cpp
        $<$<BOOL:${ENABLE_GZIP_SUPPORT}>:src/helper/gzip.cpp>)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

if(ENABLE_PCH)
    target_precompile_headers(${PROJECT_NAME} PUBLIC
            include/gba/core/integer.h
            include/gba/core/container.h)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC
        fmt::fmt-header-only
        spdlog::spdlog_header_only
        $<$<BOOL:${ENABLE_GZIP_SUPPORT}>:ZLIB::ZLIB>
        project_warnings
        project_options)

target_compile_definitions(${PROJECT_NAME}
        PUBLIC WITH_GZIP=$<BOOL:${ENABLE_GZIP_SUPPORT}>)

if(MSVC)
    # allows static_for [0,2048) + some margin
    target_compile_options(${PROJECT_NAME} PRIVATE /constexpr:depth2176)
endif()
