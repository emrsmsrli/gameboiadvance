cmake_minimum_required(VERSION 3.12.4)
project(gl3w C)

add_library(${PROJECT_NAME} src/gl3w.c)
add_library(gl3w::gl3w ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
        PUBLIC ${PROJECT_SOURCE_DIR}/include)
