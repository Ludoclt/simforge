cmake_minimum_required(VERSION 3.22)

find_package(VERILATOR REQUIRED)

add_library(simforge_backend_verilator INTERFACE)

add_library(simforge::backend::verilator
    ALIAS simforge_backend_verilator
)

target_include_directories(simforge_backend_verilator
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(simforge_backend_verilator
    INTERFACE
        simforge::uvm
)

target_compile_features(simforge_backend_verilator INTERFACE cxx_std_20)
