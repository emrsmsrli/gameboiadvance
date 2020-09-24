cmake_minimum_required(VERSION 3.12.4)
project(gameboiadvance_tests CXX)

add_executable(${PROJECT_NAME}
        src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
        doctest::doctest
        project_warnings
        project_options)

add_test(NAME gba_tests COMMAND ${PROJECT_NAME})
