cmake_minimum_required(VERSION 3.16)
project(gameboiadvance_tests CXX)

add_executable(${PROJECT_NAME}
        src/backup.cpp
        src/integer.cpp
        src/math.cpp
        src/rtc.cpp
        src/archive.cpp
        src/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE include/)

target_link_libraries(${PROJECT_NAME} PRIVATE
        doctest::doctest
        access_private::access_private
        gba::gba
        project_warnings
        project_options)

add_custom_command(TARGET gameboiadvance_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:gameboiadvance_tests>/res)

add_test(NAME gba_tests COMMAND ${PROJECT_NAME})
