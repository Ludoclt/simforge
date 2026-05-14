cmake_minimum_required(VERSION 3.22)

project(simforge LANGUAGES CXX)

include(cmake/simforge_options.cmake)
include(cmake/simforge_dependencies.cmake)

# libs
add_subdirectory(libs/uvm)

add_subdirectory(backends)
add_subdirectory(tools)
add_subdirectory(gui)
