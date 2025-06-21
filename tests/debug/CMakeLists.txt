cmake_minimum_required(VERSION 3.16)

project(ddnet_physics_debug LANGUAGES C)

# Define common variables
set(EXECUTABLES example validate copy_world)
set(COMMON_INCLUDE_DIRS 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/libs/ddnet_map_loader
)
set(COMMON_LIBRARIES ddnet_physics ddnet_map_loader)
set(COMMON_COMPILE_OPTIONS -g -DDEBUG -fsanitize=address,undefined)
set(COMMON_LINK_OPTIONS -fsanitize=address,undefined)

# Add executables
foreach(EXEC ${EXECUTABLES})
    add_executable(${EXEC} ${EXEC}.c)
    target_link_libraries(${EXEC} PRIVATE ${COMMON_LIBRARIES})
    target_compile_options(${EXEC} PRIVATE ${COMMON_COMPILE_OPTIONS})
    target_link_options(${EXEC} PRIVATE ${COMMON_LINK_OPTIONS})
    target_include_directories(${EXEC} PRIVATE ${COMMON_INCLUDE_DIRS})

    if(ENABLE_AGGRESSIVE_OPTIM)
        target_link_options(${EXEC} PRIVATE -flto)
    endif()
endforeach()
