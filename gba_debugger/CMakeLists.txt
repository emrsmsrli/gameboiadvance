cmake_minimum_required(VERSION 3.12.4)
project(gba_debugger CXX)

add_library(${PROJECT_NAME} STATIC
        src/disassembler.cpp)

add_library(gba::debugger ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PRIVATE
        gba::gba
        fmt::fmt
        imgui::sfml
        json::json
        sfml-system
        sfml-window
        sfml-graphics
        project_warnings
        project_options)
