cmake_minimum_required(VERSION 3.20)
project(KronoUniverse CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compilation flags
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -fsanitize=address -Wall -Wextra")

# Find packages
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.cc"
)

# Main executable
add_executable(kronouniverse ${SOURCES})

# Link libraries
target_link_libraries(kronouniverse
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    pthread
)

# Tests
enable_testing()
add_executable(test_ecs tests/test_ecs.cpp src/engine/ecs.cpp)
target_link_libraries(test_ecs pthread)
add_test(NAME ecs_test COMMAND test_ecs)

add_executable(test_physics tests/test_physics.cpp src/physics/physics.cpp)
target_link_libraries(test_physics pthread)
add_test(NAME physics_test COMMAND test_physics)

add_executable(test_noise tests/test_noise.cpp src/procedural/noise.c)
target_link_libraries(test_noise pthread)
add_test(NAME noise_test COMMAND test_noise)

add_executable(test_movement tests/test_movement.cpp src/procedural/noise.c)
target_link_libraries(test_movement pthread)
add_test(NAME movement_test COMMAND test_movement)
