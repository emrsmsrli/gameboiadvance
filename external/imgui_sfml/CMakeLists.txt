cmake_minimum_required(VERSION 3.16)
project(imgui_sfml)

add_library(${PROJECT_NAME} STATIC
        src/imgui.cpp
        src/imgui_draw.cpp
        src/imgui_widgets.cpp
        src/imgui_tables.cpp
        src/imgui_demo.cpp
        src/imgui-SFML.cpp)

add_library(imgui::sfml ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC sfml-graphics project_options)
target_include_directories(${PROJECT_NAME} PUBLIC include/)
