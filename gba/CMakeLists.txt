cmake_minimum_required(VERSION 3.12.4)
project(gba CXX)

add_library(${PROJECT_NAME}
        src/helper/filesystem.cpp)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PRIVATE
        project_warnings
        project_options)
