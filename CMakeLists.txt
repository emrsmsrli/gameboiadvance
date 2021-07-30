cmake_minimum_required(VERSION 3.16)

cmake_policy(SET CMP0091 NEW)
project(gameboiadvance CXX)

include(cmake/StandardProjectSettings.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Doxygen.cmake)
include(cmake/StaticAnalyzers.cmake)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(ENABLE_GZIP_SUPPORT "Enable gzip compressed file read-write support" ON)
option(ENABLE_DEBUGGER "Enable GameboiAdvance Debugger" OFF)
option(ENABLE_TESTING "Enable Tests" OFF)
option(ENABLE_PCH "Enable Precompiled Headers" OFF)
option(ENABLE_ASSERTIONS "Enable assertions" OFF)
option(WITH_WARNINGS "Enable compiler warnings" ON)
option(LOG_LEVEL "Log level" "OFF")

set(LOG_LEVELS "TRACE" "DEBUG" "INFO" "WARN" "ERROR" "CRITICAL" "OFF")
list(FIND LOG_LEVELS ${LOG_LEVEL} LOG_LEVEL_IDX)

message(STATUS "Log level: ${LOG_LEVEL} (${LOG_LEVEL_IDX})")
if(${LOG_LEVEL_IDX} EQUAL -1)
    message(WARNING "Unknown log level, turning off logs")
    set(LOG_LEVEL_IDX 6)
endif()

add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE $<IF:$<CXX_COMPILER_ID:MSVC>,/std:c++17,-std:c++17>)
target_compile_definitions(project_options INTERFACE
        DEBUG=$<CONFIG:Debug>
        WITH_DEBUGGER=$<BOOL:${ENABLE_DEBUGGER}>
        $<$<BOOL:${ENABLE_ASSERTIONS}>:ENABLE_ASSERTIONS>
        SPDLOG_ACTIVE_LEVEL=${LOG_LEVEL_IDX})

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

find_package(SFML COMPONENTS window graphics system CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
if(ENABLE_GZIP_SUPPORT)
    find_package(ZLIB REQUIRED)
endif()

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
        SDL2::CXX
        imgui::sfml
        project_warnings
        project_options)

if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
