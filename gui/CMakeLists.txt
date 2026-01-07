cmake_minimum_required(VERSION 3.22)

if(NOT SIMFORGE_BUILD_GUI)
    return()
endif()

add_executable(simforge_gui
    src/main.cpp
)

target_link_libraries(simforge_gui
    PRIVATE
        simforge::uvm
        simforge::backend::verilator
        # imgui
)
