cmake_minimum_required(VERSION 3.20)
project(KronoUniverse C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra")

# Find SDL2 but DON'T use SDL2main (we handle main() ourselves)
find_package(SDL2 REQUIRED)

# OpenGL: try find_package, fall back to system libs
find_package(OpenGL QUIET)
if(NOT OpenGL_FOUND AND NOT OPENGL_LIBRARIES)
    message(STATUS "OpenGL find_package failed — using system libs directly")
    set(OPENGL_LIBRARIES "GL")
    set(OPENGL_INCLUDE_DIR "")
endif()

find_package(SQLite3 REQUIRED)

# Override SDL2_LIBRARIES to use shared lib (avoid pulling in static deps)
if(EXISTS "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0")
    set(SDL2_LIBS "SDL2-2.0")
    message(STATUS "Using shared SDL2 from /usr/lib")
else()
    # Extract just SDL2 (not SDL2main) from SDL2_LIBRARIES
    set(SDL2_LIBS "")
    foreach(lib ${SDL2_LIBRARIES})
        if(NOT lib MATCHES "SDL2main")
            list(APPEND SDL2_LIBS ${lib})
        endif()
    endforeach()
endif()

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
target_compile_definitions(kronouniverse PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(kronouniverse
    ${SDL2_LIBS}
    ${OPENGL_LIBRARIES}
    ${SQLite3_LIBRARIES}
)

enable_testing()

add_executable(test_ecs tests/test_ecs.cpp)
add_test(NAME ecs_test COMMAND test_ecs)

add_executable(test_physics tests/test_physics.cpp)
add_test(NAME physics_test COMMAND test_physics)

add_executable(test_noise tests/test_noise.cpp src/procedural/noise.c)
add_test(NAME noise_test COMMAND test_noise)

add_executable(test_movement tests/test_movement.cpp src/procedural/noise.c)
add_test(NAME movement_test COMMAND test_movement)

add_executable(test_ship tests/test_ship.cpp src/procedural/noise.c)
add_test(NAME ship_test COMMAND test_ship)

add_executable(test_inventory tests/test_inventory.cpp)
add_test(NAME inventory_test COMMAND test_inventory)

add_executable(test_npc tests/test_npc.cpp)
add_test(NAME npc_test COMMAND test_npc)

add_executable(test_universe tests/test_universe.cpp src/procedural/noise.c)
add_test(NAME universe_test COMMAND test_universe)

add_executable(test_powers tests/test_powers.cpp)
add_test(NAME powers_test COMMAND test_powers)

add_executable(test_war_god tests/test_war_god.cpp src/procedural/noise.c)
add_test(NAME war_god_test COMMAND test_war_god)

add_executable(test_world tests/test_world.cpp src/procedural/noise.c)
add_test(NAME world_test COMMAND test_world)

add_executable(test_structural tests/test_structural.cpp src/procedural/noise.c)
add_test(NAME structural_test COMMAND test_structural)

# v0.3 tests
add_executable(test_combat tests/test_combat.cpp)
add_test(NAME combat_test COMMAND test_combat)

add_executable(test_mob_ai tests/test_mob_ai.cpp)
add_test(NAME mob_ai_test COMMAND test_mob_ai)

add_executable(test_weather_lighting tests/test_weather_lighting.cpp src/procedural/noise.c)
add_test(NAME weather_lighting_test COMMAND test_weather_lighting)

add_executable(test_crafting_v2 tests/test_crafting_v2.cpp)
add_test(NAME crafting_v2_test COMMAND test_crafting_v2)

add_executable(test_save_system tests/test_save_system.cpp)
target_link_libraries(test_save_system ${SQLite3_LIBRARIES})
add_test(NAME save_system_test COMMAND test_save_system)
