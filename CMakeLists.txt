cmake_minimum_required(VERSION 3.16)
# set(CMAKE_C_COMPILER clang)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

project(ddnet_physics LANGUAGES C)

# Option to enable aggressive optimizations
option(ENABLE_AGGRESSIVE_OPTIM "Enable aggressive optimizations with LTO and native tuning" OFF)
option(TESTS "Whether to compile tests" OFF)

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

include(CheckCCompilerFlag)

function(add_c_compiler_flag_if_supported VARIABLE FLAG)
  if(ARGC GREATER 2)
    set(CHECKED_FLAG "${ARGV2}")
  else()
    set(CHECKED_FLAG "${FLAG}")
  endif()
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" CONFIG_VARIABLE "FLAG_SUPPORTED${CHECKED_FLAG}")
  check_c_compiler_flag("${CHECKED_FLAG}" ${CONFIG_VARIABLE})
  if(${CONFIG_VARIABLE})
    if(${VARIABLE})
      set("${VARIABLE}" "${${VARIABLE}};${FLAG}" PARENT_SCOPE)
    else()
      set("${VARIABLE}" "${FLAG}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

# Include directories
target_include_directories(ddnet_physics PUBLIC include)

# Default compiler options
add_c_compiler_flag_if_supported(BASE_C_FLAGS -Wall)
add_c_compiler_flag_if_supported(BASE_C_FLAGS -msse4.2)
add_c_compiler_flag_if_supported(BASE_C_FLAGS -mavx)
add_c_compiler_flag_if_supported(BASE_C_FLAGS -mavx2)
add_c_compiler_flag_if_supported(BASE_C_FLAGS -mfma)
target_compile_options(ddnet_physics PRIVATE ${BASE_C_FLAGS})

# Define debug and optimized configurations
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_DEBUG -g)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_DEBUG -ffast-math)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_DEBUG -DDEBUG)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_DEBUG "-fsanitize=address,undefined")
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -g)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -O3)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -ffast-math)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -funroll-loops)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -mfpmath=sse)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -fomit-frame-pointer)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -fno-trapping-math)
add_c_compiler_flag_if_supported(CMAKE_C_FLAGS_RELEASE -fno-signed-zeros)

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

if(TESTS)
    # Add subdirectory for tests
    add_subdirectory(tests)
endif()
