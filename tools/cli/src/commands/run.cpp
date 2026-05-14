#include "run.hpp"

#include "../config/config_loader.hpp"
#include "../utils/process.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

namespace
{
    struct RunOptions
    {
        std::string tb_name;
        std::vector<std::string> sim_args; // forwarded verbatim to the sim binary
    };

    void run(const RunOptions &opts)
    {
        const auto root = std::filesystem::current_path();
        const auto cfg = simforge::cli::ConfigLoader::load(root);

        const auto &tb = cfg.get_tb(opts.tb_name);
        const auto bin = root / cfg.paths.build / "tb" / opts.tb_name / (opts.tb_name + "_tb");

        if (!std::filesystem::exists(bin))
            throw std::runtime_error("Binary not found: " + bin.string() + "\nRun 'simforge build " + opts.tb_name + "' first.");

        std::cout << "Running '" << opts.tb_name << "'...\n";

        int rc = simforge::cli::utils::run_interactive(bin.string(), opts.sim_args, root);
        if (rc != 0)
            throw std::runtime_error("Simulation exited with code " + std::to_string(rc));
    }
} // anonymous namespace

void register_run_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("run", "Execute a compiled testbench simulation");

    auto opts = std::make_shared<RunOptions>();

    cmd->add_option("tb_name", opts->tb_name, "Testbench name")->required();

    cmd->add_option("--args", opts->sim_args, "Extra arguments forwarded to the simulation binary");

    cmd->callback([opts]() { run(*opts); });
}
