cmake_minimum_required(VERSION 3.16)
project(gba_debugger CXX)

add_library(${PROJECT_NAME} STATIC
        src/debugger.cpp
        src/breakpoint_database.cpp
        src/disassembler.cpp
        src/disassembly_entry.cpp
        src/memory_debugger.cpp
        src/cpu_debugger.cpp
        src/gamepak_debugger.cpp
        src/ppu_debugger.cpp
        src/apu_debugger.cpp
        src/keypad_debugger.cpp)

add_library(gba::debugger ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PRIVATE
        gba::gba
        fmt::fmt-header-only
        imgui::sfml
        SDL2::CXX
        access_private::access_private
        sfml-system
        sfml-window
        sfml-graphics
        project_warnings
        project_options)
