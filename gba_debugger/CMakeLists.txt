cmake_minimum_required(VERSION 3.12.4)
project(gba_debugger CXX)

add_library(${PROJECT_NAME} STATIC
        src/debugger.cpp
        src/disassembler.cpp
        src/memory_debugger.cpp
        src/arm_debugger.cpp
        src/gamepak_debugger.cpp
        src/ppu_debugger.cpp
        src/keypad_debugger.cpp)

add_library(gba::debugger ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PRIVATE
        gba::gba
        fmt::fmt
        imgui::sfml
        json::json
        access_private::access_private
        sfml-system
        sfml-window
        sfml-graphics
        project_warnings
        project_options)
