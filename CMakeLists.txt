cmake_minimum_required(VERSION 3.18)
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
include(CheckLinkerFlag)

function(add_c_flag_if_compiler_supported VARIABLE FLAG)
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

function(add_c_flag_if_linker_supported VARIABLE FLAG)
  if(ARGC GREATER 2)
    set(CHECKED_FLAG "${ARGV2}")
  else()
    set(CHECKED_FLAG "${FLAG}")
  endif()
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" CONFIG_VARIABLE "LINKERFLAG_SUPPORTED${CHECKED_FLAG}")
  check_linker_flag(C "${CHECKED_FLAG}" ${CONFIG_VARIABLE})
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
add_c_flag_if_compiler_supported(BASE_C_FLAGS -Wall)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -msse4.2)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -mavx)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -mavx2)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -mfma)
target_compile_options(ddnet_physics PRIVATE ${BASE_C_FLAGS})

# Define debug and optimized configurations
add_c_flag_if_linker_supported(DEBUG_C_FLAGS -fsanitize=address,undefined)
add_c_flag_if_compiler_supported(DEBUG_C_FLAGS -g)
add_c_flag_if_compiler_supported(DEBUG_C_FLAGS -ffast-math)
string (REPLACE ";" " " DEBUG_C_FLAGS "${CMAKE_C_FLAGS_DEBUG}")

add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -g)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -O3)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -ffast-math)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -funroll-loops)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -mfpmath=sse)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -fomit-frame-pointer)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -fno-trapping-math)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -fno-signed-zeros)
string (REPLACE ";" " " RELEASE_C_FLAGS "${CMAKE_C_FLAGS_RELEASE}")

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

# install
target_include_directories(ddnet_physics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
install(TARGETS ddnet_physics
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES
    include/collision.h
    include/gamecore.h
    include/vmath.h
    DESTINATION include/ddnet_physics
)

# uninstall
configure_file(cmake/cmake_uninstall.cmake.in cmake_uninstall.cmake IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
