cmake_minimum_required(VERSION 3.20)
project(buh C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(buh
  src/core/main.c
  src/core/game.c
  src/data/registry.c
  src/render/render.c
  src/systems/weapons.c
  src/systems/enemies.c
  src/systems/skill_tree.c
)
target_include_directories(buh PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(buh PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2_ttf::SDL2_ttf $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>)

add_custom_command(TARGET buh POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:buh>/data
)

add_executable(buh_tests
  tests/test_game.c
  src/core/game.c
  src/data/registry.c
  src/systems/weapons.c
  src/systems/enemies.c
  src/systems/skill_tree.c
  src/render/render.c
)
target_include_directories(buh_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party
)
target_link_libraries(buh_tests PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2_ttf::SDL2_ttf $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>)

