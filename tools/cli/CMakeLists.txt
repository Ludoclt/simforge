cmake_minimum_required(VERSION 3.22)

if(NOT SIMFORGE_BUILD_CLI)
    return()
endif()

add_executable(simforge)

target_sources(simforge
    PRIVATE
        src/main.cpp
        src/commands/init.cpp
        src/config.cpp
)

target_link_libraries(simforge
    PRIVATE
        CLI11::CLI11
        simforge::uvm
        simforge::backend::verilator
)
