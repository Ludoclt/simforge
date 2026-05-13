#include <CLI/CLI.hpp>
#include <string>
#include <stdexcept>

#include "../config.hpp"

using namespace simforge::cli;

static std::string top_module;

void run()
{
    auto paths = config::read_node("paths");

    std::string build_path = paths["build"].value_or("");
    if (build_path.empty())
        throw std::runtime_error("Unable to find build path in simforge.toml");
}

void register_verilate_command(CLI::App &app)
{
    CLI::App *cmd = app.add_subcommand("verilate", "Verilate a HDL module");

    cmd->add_option("--top", top_module, "Name of the HDL top module")->required();

    cmd->callback(&run);
}
