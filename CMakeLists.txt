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

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

add_library(project_warnings INTERFACE)
option(WITH_WARNINGS "Enable compiler warnings" OFF)
if(WITH_WARNINGS)
    set_project_warnings(project_warnings)
endif()

enable_sanitizers(project_options)
enable_doxygen()

option(ENABLE_PCH "Enable Precompiled Headers" OFF)
if(ENABLE_PCH)
    # This sets a global PCH parameter, each project will build its own PCH, which
    # is a good idea if any #define's change
    target_precompile_headers(project_options INTERFACE
            <array> <vector> <string> <unordered_map> <utility>)
endif()

find_package(SDL2 CONFIG REQUIRED)

add_subdirectory(external)

add_executable(gameboiadvance main.cpp)
target_link_libraries(gameboiadvance PRIVATE SDL2::SDL2 imgui::sdl gl3w::gl3w)
target_compile_definitions(gameboiadvance PRIVATE SDL_MAIN_HANDLED)

option(ENABLE_TESTING "Enable Tests" OFF)
if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
