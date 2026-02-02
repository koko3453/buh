cmake_minimum_required(VERSION 3.20)
project(hack_arena C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(hack_arena src/main.c)

target_link_libraries(hack_arena PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2_ttf::SDL2_ttf $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>)

add_custom_command(TARGET hack_arena POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:hack_arena>/data
)

add_executable(hack_tests tests/test_game.c)
target_include_directories(hack_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(hack_tests PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2_ttf::SDL2_ttf)
