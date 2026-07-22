cmake_minimum_required(VERSION 3.22)

if(NOT SIMFORGE_BUILD_CLI)
    return()
endif()

# Python
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Template embedding

set(TEMPLATES_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/templates")
set(EMBED_SCRIPT    "${CMAKE_CURRENT_SOURCE_DIR}/embed_templates.py")
set(EMBED_HEADER    "${CMAKE_CURRENT_BINARY_DIR}/generated/templates_embed.hpp")

file(GLOB_RECURSE TEMPLATE_FILES "${TEMPLATES_DIR}/*.in")

add_custom_command(
    OUTPUT  "${EMBED_HEADER}"
    COMMAND ${Python3_EXECUTABLE} "${EMBED_SCRIPT}"
                "${TEMPLATES_DIR}"
                "${EMBED_HEADER}"
    DEPENDS ${TEMPLATE_FILES} "${EMBED_SCRIPT}"
    COMMENT "Embedding simforge templates..."
    VERBATIM
)

add_custom_target(simforge_embed_templates DEPENDS "${EMBED_HEADER}")

add_executable(simforge)

target_sources(simforge
    PRIVATE
        src/main.cpp

        # config
        src/config/config_loader.cpp

        # utils
        src/utils/process.cpp
        src/utils/template_engine.cpp
        src/utils/sv_parser.cpp
        src/utils/cmake_gen.cpp

        # commands
        src/commands/init.cpp
        src/commands/tb_init.cpp
        src/commands/build.cpp
        src/commands/run.cpp
        src/commands/wave.cpp
        src/commands/list.cpp
        src/commands/lint.cpp
)

target_include_directories(simforge
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Ensure templates are embedded before compilation
add_dependencies(simforge simforge_embed_templates)

target_link_libraries(simforge
    PRIVATE
        CLI11::CLI11
        tomlplusplus::tomlplusplus
        nlohmann_json::nlohmann_json
)

# Install
install(TARGETS simforge RUNTIME DESTINATION bin)
