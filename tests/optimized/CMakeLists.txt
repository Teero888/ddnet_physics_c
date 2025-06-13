cmake_minimum_required(VERSION 3.16)

project(ddnet_physics_optimized LANGUAGES C)

# Define executables for optimized
add_executable(benchmark benchmark.c)
add_executable(movebox movebox.c)

# Link against ddnet_physics and ddnet_map_loader
target_link_libraries(benchmark PRIVATE ddnet_physics ddnet_map_loader m z omp)
target_link_libraries(movebox PRIVATE ddnet_physics ddnet_map_loader m z omp)

# Ensure optimized build
target_compile_options(benchmark PRIVATE -O3 -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros)
target_compile_options(movebox PRIVATE -O3 -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros)

# Apply aggressive optimizations if enabled
if(ENABLE_AGGRESSIVE_OPTIM)
    target_compile_options(benchmark PRIVATE -flto -mllvm -inline-threshold=500 -march=native -mtune=native)
    target_compile_options(movebox PRIVATE -flto -mllvm -inline-threshold=500 -march=native -mtune=native)
    target_link_options(benchmark PRIVATE -flto)
    target_link_options(movebox PRIVATE -flto)
endif()

# Include directories
target_include_directories(benchmark PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/libs/ddnet_map_loader)
target_include_directories(movebox PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/libs/ddnet_map_loader)