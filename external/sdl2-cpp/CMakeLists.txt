cmake_minimum_required(VERSION 3.16)
project(sdl2-cpp)

add_library(${PROJECT_NAME} STATIC
        src/sdl_core.cpp
        src/sdl_audio.cpp)

add_library(SDL2::CXX ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
        $<IF:$<BOOL:${BUILDING_FOR_RPI3}>,SDL2,SDL2::SDL2>
        spdlog::spdlog
        project_options)
target_include_directories(${PROJECT_NAME} PUBLIC include/)
target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
