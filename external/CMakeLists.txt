cmake_minimum_required(VERSION 3.16)
project(external_libs)

function(add_header_only_library name)
    add_library(${name} INTERFACE)
    add_library(${name}::${name} ALIAS ${name})
    target_include_directories(${name} INTERFACE ${name}/)
    target_link_libraries(${name} INTERFACE project_options)
endfunction()

add_header_only_library(doctest)
add_header_only_library(cxxopts)
add_header_only_library(access_private)

add_subdirectory(imgui_sfml)
add_subdirectory(fmt)
add_subdirectory(spdlog)
add_subdirectory(sdl2-cpp)
add_subdirectory(portable-file-dialogs)
