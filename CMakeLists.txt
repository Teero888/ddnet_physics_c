cmake_minimum_required(VERSION 3.16)
# set(CMAKE_C_COMPILER clang)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

project(ddnet_physics LANGUAGES C)

# Option to enable aggressive optimizations
option(ENABLE_AGGRESSIVE_OPTIM "Enable aggressive optimizations with LTO and native tuning" OFF)
option(COMPILE_TESTS "Whether to compile tests" OFF)

# Define the static library
add_library(ddnet_physics STATIC
    include/collision.h
    include/gamecore.h
    include/vmath.h
    src/collision.c
    src/collision_tables.h
    src/config.h
    src/gamecore.c
    src/tuning.h
)

# Include directories
target_include_directories(ddnet_physics PUBLIC include)

# Default compiler options
target_compile_options(ddnet_physics PRIVATE -Wall -Wextra -msse4.2 -mavx -mavx2 -mfma)

# Define debug and optimized configurations
set(CMAKE_C_FLAGS_DEBUG "-g -ffast-math -DDEBUG -fsanitize=address,undefined")
set(CMAKE_C_FLAGS_RELEASE "-g -O3 -ffast-math -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros")

# Aggressive optimizations if enabled
if(ENABLE_AGGRESSIVE_OPTIM)
    target_compile_options(ddnet_physics PRIVATE
        -flto
        -mllvm -inline-threshold=500
        -march=native
        -mtune=native
    )
    target_link_options(ddnet_physics PRIVATE -flto)
	message("Aggressive optimization enabled")
endif()

# Add subdirectory for ddnet_map_loader
add_subdirectory(libs/ddnet_map_loader)
target_link_libraries(ddnet_physics PRIVATE ddnet_map_loader)

if(COMPILE_TESTS)
    # Add subdirectory for tests
    add_subdirectory(tests)
endif()
