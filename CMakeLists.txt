cmake_minimum_required(VERSION 4.0)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

project(ddnet_physics_c99 LANGUAGES C)

find_package(ZLIB REQUIRED)
find_package(OpenMP REQUIRED)

#-- - Sanitizer Options -- -
option(USE_ASAN "Enable AddressSanitizer" OFF)
option(USE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(USE_TSAN "Enable ThreadSanitizer (incompatible with ASan)" OFF)
option(DO_LTO "Enable Link Time Optimization for benchmark target" OFF)

set(SANITIZERS_ENABLED FALSE)
list(APPEND SANITIZER_COMPILE_FLAGS)
list(APPEND SANITIZER_LINK_FLAGS)

if(USE_ASAN AND USE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled simultaneously.")
endif()

if(USE_ASAN OR USE_UBSAN OR USE_TSAN)
    set(SANITIZERS_ENABLED TRUE)
    list(APPEND SANITIZER_COMPILE_FLAGS "-g" "-O0")
    list(APPEND SANITIZER_LINK_FLAGS "-g")
    if(USE_ASAN)
        list(APPEND SANITIZER_COMPILE_FLAGS "-fsanitize=address")
        list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=address")
    endif()
    if(USE_UBSAN)
        list(APPEND SANITIZER_COMPILE_FLAGS "-fsanitize=undefined" "-fno-omit-frame-pointer")
        list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=undefined")
    endif()
    if(USE_TSAN)
        list(APPEND SANITIZER_COMPILE_FLAGS "-fsanitize=thread")
        list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=thread")
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

    if(SANITIZERS_ENABLED)
        target_compile_options(${target_name} PRIVATE ${SANITIZER_COMPILE_FLAGS})
        target_link_options(${target_name} PRIVATE ${SANITIZER_LINK_FLAGS})
    endif()

    target_compile_options(${target_name} PRIVATE -g -msse4.2 -mavx -mavx2 -mfma)
    target_link_libraries(${target_name} PRIVATE m)

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/maps"
        "$<TARGET_FILE_DIR:${target_name}>/maps"
    )
endfunction()

# --- Configure Benchmark Targets ---
set(BENCHMARK_TESTS benchmark movebox)

foreach(bench_test ${BENCHMARK_TESTS})
    configure_target(${bench_test})
    target_link_libraries(${bench_test} PRIVATE OpenMP::OpenMP_C)

    if(NOT SANITIZERS_ENABLED)
        target_compile_options(${bench_test} PRIVATE -O3 -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros)
        if(DO_LTO)
            message(STATUS "LTO enabled for ${bench_test} target.")
            target_compile_options(${bench_test} PRIVATE -flto -g -march=native -mtune=native)
            target_link_options(${bench_test} PRIVATE -flto -g)
        else()
            message(STATUS "LTO disabled for ${bench_test} target.")
        endif()
    else()
        message(WARNING "Sanitizers enabled, disabling ${bench_test}-specific optimizations and LTO.")
    endif()
endforeach()

# --- Configure Non-Benchmark Targets ---
configure_target(example)
configure_target(validate)
