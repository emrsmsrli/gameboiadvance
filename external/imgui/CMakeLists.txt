cmake_minimum_required(VERSION 3.12.4)
project(imgui_sdl)

add_library(${PROJECT_NAME}
        src/imgui.cpp
        src/imgui_draw.cpp
        src/imgui_widgets.cpp
        src/imgui_impl_sdl.cpp
        src/imgui_impl_opengl3.cpp)

add_library(imgui::sdl ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
        PRIVATE SDL2::SDL2 gl3w::gl3w)
target_include_directories(${PROJECT_NAME}
        PUBLIC ${PROJECT_SOURCE_DIR}/include)
