#i fucking hate cmake but this is for compatibilty with msvc.fuck you tater
cmake_minimum_required(VERSION 3.15)
project(ddnet_physics_c99 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(OpenMP REQUIRED)

option(DO_LTO "Enable Link Time Optimization for benchmark target" OFF)

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

    # add debug symbols for all
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /Zi)
        target_link_options(${target_name} PRIVATE /DEBUG)
    else()
        target_compile_options(${target_name} PRIVATE -g)
    endif()

    if(UNIX)
        target_link_libraries(${target_name} PRIVATE m)
    endif()
    # add maps
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/maps"
        "$<TARGET_FILE_DIR:${target_name}>/maps"
    )
endfunction()

# benchmark specific stuff
configure_target(benchmark)
target_compile_options(benchmark PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -mavx2 -mfma -msse4.1 -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros>
    $<$<C_COMPILER_ID:MSVC>:/O2 /arch:AVX2>
)
target_link_libraries(benchmark PRIVATE OpenMP::OpenMP_C)

# Add LTO flags if enabled
if(DO_LTO)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(benchmark PRIVATE -flto -g)
        target_link_options(benchmark PRIVATE -flto)
    elseif(MSVC)
        target_compile_options(benchmark PRIVATE /GL /Zi)
        target_link_options(benchmark PRIVATE /LTCG /DEBUG)
    endif()
endif()

# other ones
configure_target(example)
configure_target(validate)

# handle msvc runtime config
if(MSVC)
    foreach(target_name IN ITEMS benchmark example validate)
        set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
