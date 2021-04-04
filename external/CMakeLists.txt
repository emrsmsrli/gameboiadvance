cmake_minimum_required(VERSION 3.16)
project(external_libs)

function(add_header_only_library name)
    add_library(${name} INTERFACE)
    add_library(${name}::${name} ALIAS ${name})
    target_include_directories(${name} INTERFACE ${name}/)
endfunction()

add_header_only_library(doctest)
add_header_only_library(cxxopts)
add_header_only_library(json)
add_header_only_library(access_private)

set(SPDLOG_BUILD_EXAMPLES OFF)
set(SPDLOG_BUILD_SHARED OFF)
set(SPDLOG_FMT_EXTERNAL_HO ON)

add_subdirectory(imgui_sfml)
add_subdirectory(fmt)
add_subdirectory(spdlog)
