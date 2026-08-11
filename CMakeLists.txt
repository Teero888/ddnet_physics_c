cmake_minimum_required(VERSION 3.18)
# set(CMAKE_C_COMPILER /usr/share/clang) # change to your path if it can't be found automatically
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

project(ddnet_physics LANGUAGES C)

include(CTest)

option(ENABLE_AGGRESSIVE_OPTIM "Enable aggressive optimizations with LTO and native tuning" OFF)
option(BENCHMARKS "Whether to compile benchmarks" OFF)
option(EXAMPLES "Whether to compile examples" OFF)
option(SHARED_LIB "Build ddnet_physics as a shared library" OFF)

if(SHARED_LIB)
    set(DDNET_PHYSICS_LIB_TYPE SHARED)
else()
    set(DDNET_PHYSICS_LIB_TYPE STATIC)
endif()

# Define the library
add_library(ddnet_physics ${DDNET_PHYSICS_LIB_TYPE}
    include/ddnet_physics/collision.h
    include/ddnet_physics/config.h
    include/ddnet_physics/gamecore.h
    include/ddnet_physics/tuning.h
    include/ddnet_physics/vmath.h
    src/collision.c
    src/collision_tables.h
    src/gamecore.c
    src/settings.c
    src/settings.h
    src/tiles.c
    src/tiles.h
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

# include directories
target_include_directories(ddnet_physics PUBLIC include)

if(SHARED_LIB)
    target_compile_definitions(ddnet_physics PRIVATE "DDNET_PHYSICS_API=__declspec(dllexport)")
    target_compile_definitions(ddnet_physics INTERFACE "DDNET_PHYSICS_API=__declspec(dllimport)")
else()
    target_compile_definitions(ddnet_physics PUBLIC "DDNET_PHYSICS_API=")
endif()

# default compiler options
add_c_flag_if_compiler_supported(BASE_C_FLAGS -Wall)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -msse4.1)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -mavx2)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -mfma)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -ffast-math)
add_c_flag_if_compiler_supported(BASE_C_FLAGS -g)
target_compile_options(ddnet_physics PRIVATE ${BASE_C_FLAGS})

# debug and optimized configurations
add_c_flag_if_linker_supported(DEBUG_C_LINK_FLAGS -fsanitize=address,undefined)

add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -O3)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -funroll-loops)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -mfpmath=sse)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -fomit-frame-pointer)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -fno-trapping-math)
add_c_flag_if_compiler_supported(RELEASE_C_FLAGS -fno-signed-zeros)

target_link_options(ddnet_physics PRIVATE
    $<$<CONFIG:DEBUG>:${DEBUG_C_LINK_FLAGS}>
    $<$<CONFIG:RELEASE>:${RELEASE_C_LINK_FLAGS}>
)

target_compile_options(ddnet_physics PRIVATE
    $<$<CONFIG:DEBUG>:${DEBUG_C_FLAGS}>
    $<$<CONFIG:RELEASE>:${RELEASE_C_FLAGS}>
)

# Aggressive optimizations if enabled
if(ENABLE_AGGRESSIVE_OPTIM)
    target_compile_options(ddnet_physics PRIVATE
        -flto
        -mllvm -inline-threshold=500
        -march=native
        -mtune=native
    )
    target_link_options(ddnet_physics PUBLIC -flto)
	message("Aggressive optimization enabled")
endif()

set(PGO_STAGE "NONE" CACHE STRING "PGO stage: NONE, GENERATE or USE")
set_property(CACHE PGO_STAGE PROPERTY STRINGS NONE GENERATE USE)
set(PGO_PROFILE_DIR "${CMAKE_BINARY_DIR}/pgo" CACHE PATH
    "Directory raw profiles are written to during the GENERATE stage")
set(PGO_PROFILE_DATA "${CMAKE_BINARY_DIR}/pgo/merged.profdata" CACHE FILEPATH
    "Merged profile consumed by the USE stage")

if(NOT PGO_STAGE STREQUAL "NONE")
    set(PGO_WARN_FLAGS)
    if(PGO_STAGE STREQUAL "GENERATE")
        set(PGO_C_FLAGS -fprofile-generate=${PGO_PROFILE_DIR})
    elseif(PGO_STAGE STREQUAL "USE")
        if(NOT EXISTS "${PGO_PROFILE_DATA}")
            message(FATAL_ERROR
                "PGO_STAGE=USE but no merged profile at ${PGO_PROFILE_DATA}. "
                "Run the GENERATE stage and merge the raw profiles first.")
        endif()
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            set(PGO_C_FLAGS -fprofile-use=${PGO_PROFILE_DATA})
            set(PGO_WARN_FLAGS -Wno-profile-instr-out-of-date
                               -Wno-profile-instr-unprofiled)
        else()
            set(PGO_C_FLAGS -fprofile-use=${PGO_PROFILE_DIR} -fprofile-correction)
            set(PGO_WARN_FLAGS -Wno-missing-profile)
        endif()
    else()
        message(FATAL_ERROR "PGO_STAGE must be NONE, GENERATE or USE (got '${PGO_STAGE}')")
    endif()

    target_compile_options(ddnet_physics PRIVATE ${PGO_C_FLAGS} ${PGO_WARN_FLAGS})
    # GENERATE needs the profile runtime linked in; USE needs the profile
    # visible to LTO codegen. PUBLIC for the same reason as -flto above. The
    # -Wno-* flags are compile-only.
    target_link_options(ddnet_physics PUBLIC ${PGO_C_FLAGS})
    message(STATUS "ddnet_physics PGO stage: ${PGO_STAGE}")
endif()

# ddnet_map_loader fetches zlib when no system package is available. Only the
# library is needed here; zlib's example programs inherit this project's strict
# C99 mode and can fail independently of the library.
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Build zlib example programs")

# Add subdirectory for ddnet_map_loader
add_subdirectory(libs/ddnet_map_loader)
target_link_libraries(ddnet_physics PUBLIC ddnet_map_loader)
if(UNIX AND NOT APPLE)
  target_link_libraries(ddnet_physics PRIVATE m)
endif()

if(BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
if(EXAMPLES)
    add_subdirectory(examples)
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
    include/ddnet_physics/collision.h
    include/ddnet_physics/config.h
    include/ddnet_physics/gamecore.h
    include/ddnet_physics/tuning.h
    include/ddnet_physics/vmath.h
    DESTINATION include/ddnet_physics
)

# uninstall
if(NOT TARGET uninstall)
    configure_file(cmake/cmake_uninstall.cmake.in cmake_uninstall.cmake IMMEDIATE @ONLY)
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()
