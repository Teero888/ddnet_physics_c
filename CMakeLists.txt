cmake_minimum_required(VERSION 3.15)
project(ddnet_physics_c99 LANGUAGES C)

set(CMAKE_C_COMPILER clang)
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
        set(SANITIZERS_ENABLED TRUE)
    else()
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

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /Zi)
        target_link_options(${target_name} PRIVATE /DEBUG)
        if(NOT SANITIZERS_ENABLED)
            target_compile_options(${target_name} PRIVATE /arch:AVX2)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -g -msse4.2 -mavx -mavx2 -mfma)
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(${target_name} PRIVATE m)
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/maps"
        "$<TARGET_FILE_DIR:${target_name}>/maps"
    )
endfunction()

# --- Configure Benchmark Targets ---
set(BENCHMARK_TESTS benchmark movebox) # add other benchmarks

foreach(bench_test ${BENCHMARK_TESTS})
    configure_target(${bench_test})
    target_link_libraries(${bench_test} PRIVATE OpenMP::OpenMP_C -lomp)

    if(NOT SANITIZERS_ENABLED)
        target_compile_options(${bench_test} PRIVATE
            $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros>
            $<$<C_COMPILER_ID:MSVC>:/O2>
        )

        if(DO_LTO)
            message(STATUS "LTO enabled for ${bench_test} target.")
            if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
                target_compile_options(${bench_test} PRIVATE -flto -g -march=native -mtune=native)
                target_link_options(${bench_test} PRIVATE -flto -g)
            elseif(MSVC)
                target_compile_options(${bench_test} PRIVATE /GL)
                target_link_options(${bench_test} PRIVATE /LTCG /DEBUG)
            endif()
        else()
            message(STATUS "LTO disabled for ${bench_test} target.")
        endif()
    else()
        message(WARNING "Sanitizers enabled, disabling ${bench_test}-specific optimizations and LTO.")
    endif()

    if(MSVC)
        set_property(TARGET ${bench_test} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endforeach()

# --- Configure Non-Benchmark Targets ---
configure_target(example)
configure_target(validate)

if(MSVC)
    foreach(target_name IN ITEMS example validate)
        set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
