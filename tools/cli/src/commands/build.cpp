#include "build.hpp"

#include "../config/config_loader.hpp"
#include "../utils/cmake_gen.hpp"
#include "../utils/process.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

namespace
{
    using namespace simforge::cli;
    using namespace simforge::cli::utils;

    struct BuildOptions
    {
        std::string tb_name; // empty = build all
        std::string group;
        int jobs = 0;
    };

    void build_one(const SimforgeConfig &cfg, const TestbenchConfig &tb, const std::filesystem::path &root, int jobs_override)
    {
        std::cout << "== Building '" << tb.subdir() << "' ============================\n";

        generate_tb_cmake(cfg, tb.subdir(), root);

        const auto build_dir = root / cfg.paths.build / "tb" / tb.subdir();

        std::vector<std::string> cmake_cfg_args = {
            "-S",
            build_dir.string(),
            "-B",
            build_dir.string(),
            "-DSIMFORGE_BUILD_CLI=OFF", // we don't recurse into ourselves
        };

        if (utils::which("ninja").empty())
            cmake_cfg_args.push_back("-DCMAKE_BUILD_TYPE=Release");
        else
        {
            cmake_cfg_args.push_back("-GNinja");
            cmake_cfg_args.push_back("-DCMAKE_BUILD_TYPE=Release");
        }

        std::cout << "  cmake configure...\n";
        int rc = run_interactive("cmake", cmake_cfg_args, root);
        if (rc != 0)
            throw std::runtime_error("cmake configure failed (exit " + std::to_string(rc) + ")");

        std::vector<std::string> cmake_build_args = {
            "--build",
            build_dir.string(),
        };
        int jobs = jobs_override > 0 ? jobs_override : cfg.effective_verilator(tb.subdir()).jobs;
        if (jobs > 0)
        {
            cmake_build_args.push_back("--parallel");
            cmake_build_args.push_back(std::to_string(jobs));
        }

        std::cout << "  cmake build...\n";
        rc = run_interactive("cmake", cmake_build_args, root);
        if (rc != 0)
            throw std::runtime_error("cmake build failed (exit " + std::to_string(rc) + ")");

        std::cout << "OK: '" << tb.subdir() << "' built successfully.\n";
    }

    void run(const BuildOptions &opts)
    {
        const auto root = std::filesystem::current_path();
        const auto cfg = ConfigLoader::load(root);

        if (!opts.tb_name.empty())
        {
            build_one(cfg, cfg.get_tb(opts.tb_name), root, opts.jobs);
            return;
        }

        if (!opts.group.empty())
        {
            auto matches = cfg.in_group(opts.group);
            if (matches.empty())
                throw std::runtime_error("No testbench found in group '" + opts.group + "'");
            for (const auto &tb : matches)
                build_one(cfg, tb.get(), root, opts.jobs);
            return;
        }

        for (const auto &tb : cfg.testbenches)
            build_one(cfg, tb, root, opts.jobs);
    }
} // anonymous namespace

void register_build_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("build", "Compile a testbench (or all testbenches if no name given)");

    auto opts = std::make_shared<BuildOptions>();

    cmd->add_option(
        "tb_name",
        opts->tb_name,
        "Testbench name as declared in simforge.toml — bare name if unambiguous, or "
        "'<group>/<name>' to disambiguate (omit to build all, or all of --group)"
    );

    cmd->add_option("--group", opts->group, "Build all testbenches in this group (ignored if tb_name is given)");

    cmd->add_option("-j,--jobs", opts->jobs, "Parallel build jobs (overrides [verilator] jobs in TOML)");

    cmd->callback([opts]() { run(*opts); });
}
