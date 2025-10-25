cmake_minimum_required(VERSION 3.16)

project(ddnet_physics_optimized LANGUAGES C)

find_package(OpenMP REQUIRED)

set(PGO_STAGE "NONE" CACHE STRING "Set the PGO stage (NONE, GENERATE, USE)")
set_property(CACHE PGO_STAGE PROPERTY STRINGS NONE GENERATE USE)

# Define executables
add_executable(benchmark benchmark.c)
add_executable(movebox movebox.c)

# Link against ddnet_physics, ddnet_map_loader, and OpenMP
target_link_libraries(benchmark PRIVATE ddnet_physics ddnet_map_loader m z OpenMP::OpenMP_C)
target_link_libraries(movebox PRIVATE ddnet_physics ddnet_map_loader m z OpenMP::OpenMP_C)

# Default compile options
target_compile_options(benchmark PRIVATE -O3 -ffast-math -g -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros)
target_compile_options(movebox PRIVATE -O3 -ffast-math -g -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros)

# Apply aggressive optimizations if enabled
if(ENABLE_AGGRESSIVE_OPTIM)
    target_compile_options(benchmark PRIVATE -flto -mllvm -inline-threshold=500 -march=native -mtune=native)
    target_compile_options(movebox PRIVATE -flto -mllvm -inline-threshold=500 -march=native -mtune=native)
    target_link_options(benchmark PRIVATE -flto)
    target_link_options(movebox PRIVATE -flto)
endif()

if(NOT PGO_STAGE STREQUAL "NONE")
    set(PGO_PROFILE_DIR "${CMAKE_BINARY_DIR}/pgo_profiles")
    file(MAKE_DIRECTORY ${PGO_PROFILE_DIR})

    if(PGO_STAGE STREQUAL "GENERATE")
        message(STATUS "PGO: Compiling for profile GENERATION.")
        set(PGO_FLAGS "-fprofile-generate=${PGO_PROFILE_DIR}")
    elseif(PGO_STAGE STREQUAL "USE")
        message(STATUS "PGO: Compiling with profile USE.")
        set(PGO_FLAGS "-fprofile-use=${PGO_PROFILE_DIR}/default.profdata")
        
        # add a custom target to merge the profraw files
        # first, find the llvm-profdata executable
        find_program(LLVM_PROFDATA_EXECUTABLE llvm-profdata)
        if(NOT LLVM_PROFDATA_EXECUTABLE)
            message(FATAL_ERROR "llvm-profdata not found! Please ensure it is in your PATH.")
        endif()

        # create a target that runs the merge command
        add_custom_target(pgo-merge-data
            COMMAND ${LLVM_PROFDATA_EXECUTABLE} merge -o ${PGO_PROFILE_DIR}/default.profdata ${PGO_PROFILE_DIR}
            COMMENT "Merging PGO raw profiles..."
        )
        # make the benchmark target depend on the merge step
        add_dependencies(benchmark pgo-merge-data)

    endif()

    # Apply the correct PGO flags to ALL relevant targets
    target_compile_options(ddnet_physics PRIVATE ${PGO_FLAGS})
    target_compile_options(benchmark PRIVATE ${PGO_FLAGS})
    target_link_options(benchmark PRIVATE ${PGO_FLAGS})
    target_compile_options(movebox PRIVATE ${PGO_FLAGS})
    target_link_options(movebox PRIVATE ${PGO_FLAGS})
endif()

# Include directories
target_include_directories(benchmark PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/libs/ddnet_map_loader)
target_include_directories(movebox PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/libs/ddnet_map_loader)