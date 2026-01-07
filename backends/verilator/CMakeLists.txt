cmake_minimum_required(VERSION 3.22)

find_package(Verilator REQUIRED)

add_library(simforge_backend_verilator)

add_library(simforge::backend::verilator
    ALIAS simforge_backend_verilator
)

target_include_directories(simforge_backend_verilator
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(simforge_backend_verilator
    PUBLIC
        simforge::uvm
        Verilator::verilator
)

target_compile_features(simforge_backend_verilator PUBLIC cxx_std_20)
