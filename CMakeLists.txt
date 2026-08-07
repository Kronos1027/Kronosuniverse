cmake_minimum_required(VERSION 3.20)
project(KronoUniverse C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra")

# Find packages (SDL2_image and SDL2_mixer are optional)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.cc")
list(APPEND SOURCES src/procedural/noise.c)

add_executable(kronouniverse ${SOURCES})

target_link_libraries(kronouniverse
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    pthread
)

enable_testing()

add_executable(test_ecs tests/test_ecs.cpp)
target_link_libraries(test_ecs pthread)
add_test(NAME ecs_test COMMAND test_ecs)

add_executable(test_physics tests/test_physics.cpp)
target_link_libraries(test_physics pthread)
add_test(NAME physics_test COMMAND test_physics)

add_executable(test_noise tests/test_noise.cpp src/procedural/noise.c)
target_link_libraries(test_noise pthread)
add_test(NAME noise_test COMMAND test_noise)

add_executable(test_movement tests/test_movement.cpp src/procedural/noise.c)
target_link_libraries(test_movement pthread)
add_test(NAME movement_test COMMAND test_movement)

add_executable(test_ship tests/test_ship.cpp src/procedural/noise.c)
target_link_libraries(test_ship pthread)
add_test(NAME ship_test COMMAND test_ship)

add_executable(test_inventory tests/test_inventory.cpp)
target_link_libraries(test_inventory pthread)
add_test(NAME inventory_test COMMAND test_inventory)

add_executable(test_npc tests/test_npc.cpp)
target_link_libraries(test_npc pthread)
add_test(NAME npc_test COMMAND test_npc)

add_executable(test_universe tests/test_universe.cpp src/procedural/noise.c)
target_link_libraries(test_universe pthread)
add_test(NAME universe_test COMMAND test_universe)

add_executable(test_powers tests/test_powers.cpp)
target_link_libraries(test_powers pthread)
add_test(NAME powers_test COMMAND test_powers)

add_executable(test_war_god tests/test_war_god.cpp src/procedural/noise.c)
target_link_libraries(test_war_god pthread)
add_test(NAME war_god_test COMMAND test_war_god)

add_executable(test_world tests/test_world.cpp src/procedural/noise.c)
target_link_libraries(test_world pthread)
add_test(NAME world_test COMMAND test_world)

add_executable(test_structural tests/test_structural.cpp src/procedural/noise.c)
target_link_libraries(test_structural pthread)
add_test(NAME structural_test COMMAND test_structural)
