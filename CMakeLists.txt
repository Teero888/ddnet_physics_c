cmake_minimum_required(VERSION 3.15)
project(ddnet_physics_c99 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(OpenMP REQUIRED)

# --- Sanitizer Options ---
option(USE_ASAN "Enable AddressSanitizer (Clang/GCC only)" OFF)
option(USE_UBSAN "Enable UndefinedBehaviorSanitizer (Clang/GCC only)" OFF)
option(USE_TSAN "Enable ThreadSanitizer (Clang/GCC only, incompatible with ASan)" OFF)
option(DO_LTO "Enable Link Time Optimization for benchmark target" OFF)

set(SANITIZERS_ENABLED FALSE)
list(APPEND SANITIZER_COMPILE_FLAGS)
list(APPEND SANITIZER_LINK_FLAGS)

if(USE_ASAN AND USE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled simultaneously.")
endif()

if(USE_ASAN OR USE_UBSAN OR USE_TSAN)
    if(MSVC)
        message(WARNING "Sanitizers (ASan/UBSan/TSan options) are primarily designed for GCC/Clang in this script. MSVC support might be limited or require manual setup (/fsanitize=address). Disabling optimizations anyway for debugging.")
        set(SANITIZERS_ENABLED TRUE) # Still disable optimizations on MSVC if requested
    else()
        set(SANITIZERS_ENABLED TRUE)
        # Common flags for sanitizers
        list(APPEND SANITIZER_COMPILE_FLAGS "-g" "-O0") # Need debug symbols, disable optimizations
        list(APPEND SANITIZER_LINK_FLAGS "-g") # Linker needs debug symbols too

        if(USE_ASAN)
            list(APPEND SANITIZER_COMPILE_FLAGS "-fsanitize=address")
            list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=address")
        endif()
        if(USE_UBSAN)
            list(APPEND SANITIZER_COMPILE_FLAGS "-fsanitize=undefined")
            list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=undefined")
            # Recommended UBSan additions:
            list(APPEND SANITIZER_COMPILE_FLAGS "-fno-omit-frame-pointer")
        endif()
        if(USE_TSAN)
            list(APPEND SANITIZER_COMPILE_FLAGS "-fsanitize=thread")
            list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=thread")
        endif()
    endif()
endif()

message(STATUS "Sanitizers enabled: ${SANITIZERS_ENABLED}")
if(SANITIZERS_ENABLED)
    message(STATUS "Sanitizer compile flags: ${SANITIZER_COMPILE_FLAGS}")
    message(STATUS "Sanitizer link flags: ${SANITIZER_LINK_FLAGS}")
endif()
# --- End Sanitizer Options ---


file(GLOB SRC_SOURCES CONFIGURE_DEPENDS
    src/*.c
    libs/ddnet_maploader_c/map_loader.c
)

set(COMMON_INCLUDES
    src
    libs/ddnet_maploader_c
)

function(configure_target target_name)
    add_executable(${target_name} tests/${target_name}.c ${SRC_SOURCES})
    target_include_directories(${target_name} PRIVATE ${COMMON_INCLUDES})
    target_link_libraries(${target_name} PRIVATE ZLIB::ZLIB)

    # Add sanitizer flags FIRST if enabled (includes -g -O0 on GCC/Clang)
    if(SANITIZERS_ENABLED)
        target_compile_options(${target_name} PRIVATE ${SANITIZER_COMPILE_FLAGS})
        target_link_options(${target_name} PRIVATE ${SANITIZER_LINK_FLAGS})
    endif()

    # Default options (applied *after* sanitizer flags)
    # Note: -O0 from sanitizers should override optimization flags below on GCC/Clang
    if(MSVC)
        # Add MSVC debug info regardless of sanitizers
        target_compile_options(${target_name} PRIVATE /Zi)
        target_link_options(${target_name} PRIVATE /DEBUG)
        # Add AVX2 only if sanitizers are NOT enabled (optimizations are off anyway)
        # MSVC handles intrinsics differently, /arch:AVX2 enables them *and* optimizations
        # If sanitizing, we typically don't want the optimization part of /arch:AVX2
        if(NOT SANITIZERS_ENABLED)
             target_compile_options(${target_name} PRIVATE /arch:AVX2)
        endif()
    else() # Not MSVC
        # Add base debug flag regardless of sanitizers
        target_compile_options(${target_name} PRIVATE -g) # Ensure -g is present even if not sanitizing

        # --- Add required CPU feature flags unconditionally for non-MSVC ---
        # Source code uses SSE/AVX/AVX2/FMA intrinsics, these flags MUST be enabled.
        target_compile_options(${target_name} PRIVATE -msse4.1 -msse4.2 -mavx -mavx2 -mfma)
        # --------------------------------------------------------------------

        # No other specific non-optimization flags needed here currently based on sanitizer state
    endif()

    if(UNIX AND NOT APPLE) # AppleClang usually doesn't need explicit -lm
        target_link_libraries(${target_name} PRIVATE m)
    endif()

    # Add maps
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/maps"
        "$<TARGET_FILE_DIR:${target_name}>/maps"
    )
endfunction()

# --- Target Configuration ---

# Common targets
configure_target(example)
configure_target(validate)

# Benchmark specific stuff
configure_target(benchmark)
# Link OpenMP only to benchmark
target_link_libraries(benchmark PRIVATE OpenMP::OpenMP_C)

# Apply benchmark optimizations ONLY if sanitizers are OFF
if(NOT SANITIZERS_ENABLED)
    target_compile_options(benchmark PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros>
        $<$<C_COMPILER_ID:MSVC>:/O2> # /arch:AVX2 moved to configure_target for MSVC non-sanitizer case
    )

    # Add LTO flags if enabled AND sanitizers are OFF
    if(DO_LTO)
        message(STATUS "LTO enabled for benchmark target.")
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
             # Note: Adding -g to LTO flags helps if debugging optimized builds
            target_compile_options(benchmark PRIVATE -flto -g -march=native -mtune=native)
            target_link_options(benchmark PRIVATE -flto -g)
        elseif(MSVC)
             # Note: Adding /DEBUG to LTO flags helps if debugging optimized builds
            target_compile_options(benchmark PRIVATE /GL) # /Zi already added
            target_link_options(benchmark PRIVATE /LTCG /DEBUG) # /DEBUG already added
        endif()
    else()
         message(STATUS "LTO disabled for benchmark target.")
    endif()
else()
    message(WARNING "Sanitizers enabled, disabling benchmark-specific optimizations and LTO.")
endif()


# Handle msvc runtime config
if(MSVC)
    foreach(target_name IN ITEMS benchmark example validate)
        set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
