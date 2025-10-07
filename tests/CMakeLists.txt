cmake_minimum_required(VERSION 3.16)
project(ddnet_physics_tests LANGUAGES C)

# Copy the maps directory to the build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/maps DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/..)

# Add subdirectories for optimized and debug tests
# add_subdirectory(debug)
add_subdirectory(optimized)
