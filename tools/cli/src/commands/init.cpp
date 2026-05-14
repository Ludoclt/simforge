#include "init.hpp"

#include "../config/config_loader.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

namespace
{
    struct InitOptions
    {
        std::string name;
        std::string rtl_path;
        std::vector<std::string> pkg_paths;
        bool force = false;
    };

    void run(const InitOptions &opts)
    {
        const auto cwd = std::filesystem::current_path();

        simforge::cli::SimforgeConfig cfg;
        cfg.project.name = opts.name;
        cfg.paths.rtl = opts.rtl_path;
        cfg.paths.packages = opts.pkg_paths;
        cfg.paths.tb = "tb";
        cfg.paths.build = "build";
        cfg.verilator.args = {"--sv"};
        cfg.artifacts.vcd = true;
        cfg.artifacts.log = true;

        simforge::cli::ConfigLoader::write_default(cfg, cwd, opts.force);

        std::cout << "OK: Initialized simforge project '" << cfg.project.name << "'\n"
                  << "  -> " << (cwd / simforge::cli::kConfigFilename).string() << "\n\n"
                  << "Next steps:\n"
                  << "  simforge tb init " << cfg.paths.rtl << "/<module>.sv --top <module>\n"
                  << "  simforge build <module>\n"
                  << "  simforge run   <module>\n";
    }
} // anonymous namespace

void register_init_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("init", "Initialize a simforge project in the current directory");
    auto opts = std::make_shared<InitOptions>();

    cmd->add_option("--name", opts->name, "Project name (defaults to current directory name)")->default_val(std::filesystem::current_path().filename().string());

    cmd->add_option("--rtl", opts->rtl_path, "Path to SystemVerilog RTL sources")->required()->check(CLI::ExistingDirectory);

    cmd->add_option("--pkg", opts->pkg_paths, "Path(s) to SystemVerilog package directories (repeatable)")->check(CLI::ExistingDirectory);

    cmd->add_flag("--force", opts->force, "Overwrite existing simforge.toml")->default_val(false);

    cmd->callback([opts]() { run(*opts); });
}
