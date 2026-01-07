cmake_minimum_required(VERSION 3.22)

project(sim_forge LANGUAGES CXX)

include(cmake/simforge_options.cmake)

add_subdirectory(libs/uvm)
add_subdirectory(backends)
add_subdirectory(tools)
add_subdirectory(gui)
