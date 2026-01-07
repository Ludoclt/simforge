cmake_minimum_required(VERSION 3.22)

if(NOT SIMFORGE_BUILD_CLI)
    return()
endif()

add_executable(simforge)

target_sources(simforge
    PRIVATE
        src/main.cpp
)

target_link_libraries(simforge
    PRIVATE
        simforge::uvm
        simforge::backend::verilator
)
