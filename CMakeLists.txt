cmake_minimum_required(VERSION 3.12.4)
project(gameboiadvance CXX)

include(cmake/StandardProjectSettings.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Doxygen.cmake)
include(cmake/StaticAnalyzers.cmake)
if(MSVC)
    include(cmake/StaticLink.cmake)
endif()

option(ENABLE_DEBUGGER "Enable GameboiAdvance Debugger" OFF)
option(ENABLE_TESTING "Enable Tests" OFF)
option(ENABLE_PCH "Enable Precompiled Headers" OFF)
option(WITH_WARNINGS "Enable compiler warnings" ON)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)
target_compile_definitions(project_options INTERFACE
        DEBUG=$<CONFIG:Debug>
        WITH_DEBUGGER=$<BOOL:${ENABLE_DEBUGGER}>)

if(NOT MSVC)
    option(WITH_LIBCXX "Use libc++" OFF)
    if(WITH_LIBCXX)
        target_compile_options(project_options INTERFACE -stdlib=libc++)
        target_link_options(project_options INTERFACE -lc++)
    else()
        target_link_libraries(project_options INTERFACE stdc++fs)
    endif()
endif()

add_library(project_warnings INTERFACE)
if(WITH_WARNINGS)
    set_project_warnings(project_warnings)
endif()

enable_sanitizers(project_options)
enable_doxygen()

if(ENABLE_PCH)
    # This sets a global PCH parameter, each project will build its own PCH, which
    # is a good idea if any #define's change
    target_precompile_headers(project_options INTERFACE
            <array> <vector> <string> <unordered_map> <utility>)
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(SFML COMPONENTS window graphics system CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

add_subdirectory(external)
add_subdirectory(gba)

if(ENABLE_DEBUGGER)
    add_subdirectory(gba_debugger)
endif()

add_executable(gameboiadvance main.cpp)
target_link_libraries(gameboiadvance PRIVATE
        gba::gba
        $<$<BOOL:${ENABLE_DEBUGGER}>:gba::debugger>
        cxxopts::cxxopts
        $<IF:$<BOOL:${BUILDING_FOR_RPI3}>,SDL2,SDL2::SDL2>
        imgui::sfml
        project_warnings
        project_options)

target_compile_definitions(gameboiadvance PRIVATE SDL_MAIN_HANDLED)

if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
