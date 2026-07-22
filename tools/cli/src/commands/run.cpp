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
        std::string group;
        std::vector<std::string> sim_args; // forwarded verbatim to the sim binary
    };

    void run_one(const simforge::cli::TestbenchConfig &tb, const std::filesystem::path &root, const simforge::cli::SimforgeConfig &cfg, const std::vector<std::string> &sim_args)
    {
        const auto bin = root / cfg.paths.build / "tb" / tb.subdir() / (tb.name + "_tb");

        if (!std::filesystem::exists(bin))
            throw std::runtime_error("Binary not found: " + bin.string() + "\nRun 'simforge build " + tb.subdir() + "' first.");

        std::cout << "Running '" << tb.subdir() << "'...\n";

        int rc = simforge::cli::utils::run_interactive(bin.string(), sim_args, root);
        if (rc != 0)
            throw std::runtime_error("Simulation '" + tb.subdir() + "' exited with code " + std::to_string(rc));
    }

    void run(const RunOptions &opts)
    {
        const auto root = std::filesystem::current_path();
        const auto cfg = simforge::cli::ConfigLoader::load(root);

        if (!opts.tb_name.empty())
        {
            run_one(cfg.get_tb(opts.tb_name), root, cfg, opts.sim_args);
            return;
        }

        if (!opts.group.empty())
        {
            auto matches = cfg.in_group(opts.group);
            if (matches.empty())
                throw std::runtime_error("No testbench found in group '" + opts.group + "'");
            for (const auto &tb : matches)
                run_one(tb.get(), root, cfg, opts.sim_args);
            return;
        }

        throw std::runtime_error("Provide a testbench name or --group.");
    }
} // anonymous namespace

void register_run_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("run", "Execute a compiled testbench simulation");

    auto opts = std::make_shared<RunOptions>();

    cmd->add_option("tb_name", opts->tb_name, "Testbench name (bare, or '<group>/<name>' to disambiguate)");

    cmd->add_option("--group", opts->group, "Run every testbench in this group sequentially (ignored if tb_name is given)");

    cmd->add_option("--args", opts->sim_args, "Extra arguments forwarded to the simulation binary");

    cmd->callback([opts]() { run(*opts); });
}
