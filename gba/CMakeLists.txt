cmake_minimum_required(VERSION 3.12.4)
project(gba VERSION 0.0.1 LANGUAGES CXX)

configure_file(cmake/version.h.in
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gba/version.h)

add_library(${PROJECT_NAME} STATIC
        src/cartridge/backup.cpp
        src/cartridge/gamepak.cpp
        src/helper/filesystem.cpp)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PUBLIC
        spdlog::spdlog
        project_warnings
        project_options)
