cmake_minimum_required(VERSION 3.22)

add_library(simforge_uvm)

file(GLOB_RECURSE UVM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

target_sources(simforge_uvm PRIVATE ${UVM_SOURCES})

target_include_directories(simforge_uvm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(simforge_uvm
    PUBLIC
        spdlog::spdlog
)

target_compile_features(simforge_uvm PUBLIC cxx_std_20)
