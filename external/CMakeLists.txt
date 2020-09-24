cmake_minimum_required(VERSION 3.12.4)
project(external_libs)

function(add_header_only_library name)
    add_library(${name} INTERFACE)
    add_library(${name}::${name} ALIAS ${name})
    target_include_directories(${name} INTERFACE ${name}/)
endfunction()

add_header_only_library(doctest)
add_header_only_library(cxxopts)
add_header_only_library(json)

add_subdirectory(imgui_sfml)
