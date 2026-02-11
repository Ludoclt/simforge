#include <CLI/CLI.hpp>
#include <toml++/toml.hpp>

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

#include "../config.hpp"

using namespace simforge::cli;

static std::string project_name;
static std::string pkg_path;
static std::string rtl_path;

void run()
{
    config::init();

    config::add_table(
        toml::table{
            {"project", toml::table{
                            {"name", project_name},
                        }}});

    config::add_table(
        toml::table{
            {"paths", toml::table{
                          {"rtl", toml::array{rtl_path}},
                          {"packages", toml::array{pkg_path}},
                          {"build", "build"},
                      }}});

    config::add_table(
        toml::table{
            {"artifacts", toml::table{
                              {"vcd", true},
                              {"log", true},
                          }}});

    std::cout << "done." << std::endl;
}

void register_init_command(CLI::App &app)
{
    CLI::App *cmd = app.add_subcommand("init", "Init simforge project");

    cmd->add_option("--name", project_name, "Name of the simforge project")->default_val(std::filesystem::current_path().filename().string());
    cmd->add_option("--pkg", pkg_path, "SystemVerilog packages path")->check(CLI::ExistingDirectory);
    cmd->add_option("--rtl", rtl_path, "SystemVerilog rtl modules path")->required()->check(CLI::ExistingDirectory);

    cmd->callback(&run);
}
