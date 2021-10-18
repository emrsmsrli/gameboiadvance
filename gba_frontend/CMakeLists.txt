cmake_minimum_required(VERSION 3.16)
project(gba_frontend CXX)

add_library(${PROJECT_NAME} STATIC
        src/frontend.cpp)

add_library(gba::frontend ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PUBLIC
        gba::gba
        imgui::sfml
        SDL2::CXX
        sfml-system
        sfml-window
        sfml-graphics
        portable_file_dialogs
        project_warnings
        project_options)
