cmake_minimum_required(VERSION 3.16)

project(ddnet_physics_debug LANGUAGES C)

# Define executables for debug
add_executable(example example.c)
add_executable(validate validate.c)

# Link against ddnet_physics and ddnet_map_loader
target_link_libraries(example PRIVATE ddnet_physics ddnet_map_loader)
target_link_libraries(validate PRIVATE ddnet_physics ddnet_map_loader)

# Ensure debug build with sanitizers
target_compile_options(example PRIVATE -g -DDEBUG -fsanitize=address,undefined)
target_compile_options(validate PRIVATE -g -DDEBUG -fsanitize=address,undefined)

target_link_options(example PRIVATE -fsanitize=address,undefined)
target_link_options(validate PRIVATE -fsanitize=address,undefined)

if(ENABLE_AGGRESSIVE_OPTIM)
    target_link_options(example PRIVATE -flto)
    target_link_options(validate PRIVATE -flto)
endif()

# Include directories
target_include_directories(example PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/libs/ddnet_map_loader)
target_include_directories(validate PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/libs/ddnet_map_loader)