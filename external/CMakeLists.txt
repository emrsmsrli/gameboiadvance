cmake_minimum_required(VERSION 3.12.4)
project(external_libs)

add_library(doctest INTERFACE)
add_library(doctest::doctest ALIAS doctest)
target_include_directories(doctest INTERFACE doctest/)

add_subdirectory(gl3w)
add_subdirectory(imgui)
