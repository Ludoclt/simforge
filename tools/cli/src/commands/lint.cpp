#include "lint.hpp"

#include "../config/config_loader.hpp"
#include "../utils/process.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <set>

namespace
{
    using namespace simforge::cli;

    struct LintOptions
    {
        std::string module;
        bool keep_going = false;
    };

    std::vector<std::filesystem::path> glob_sv(const std::filesystem::path &dir, bool recursive)
    {
        std::vector<std::filesystem::path> out;
        if (!std::filesystem::exists(dir))
            return out;

        auto consider = [&](const std::filesystem::directory_entry &e)
        {
            if (e.path().extension() == ".sv")
                out.push_back(std::filesystem::canonical(e.path()));
        };

        if (recursive)
            for (const auto &e : std::filesystem::recursive_directory_iterator(dir))
                consider(e);
        else
            for (const auto &e : std::filesystem::directory_iterator(dir))
                consider(e);

        return out;
    }

    void run(const LintOptions &opts)
    {
        const auto root = std::filesystem::current_path();

        PathsConfig paths;
        VerilatorConfig vcfg;
        if (std::filesystem::exists(root / kConfigFilename))
        {
            auto cfg = ConfigLoader::load(root);
            paths = cfg.paths;
            vcfg = cfg.verilator;
        }

        std::vector<std::string> args = {"--lint-only", "-Wall"};
        if (opts.keep_going)
            args.push_back("-Wno-fatal");
        for (const auto &a : vcfg.args)
            args.push_back(a);

        // Packages first
        std::set<std::filesystem::path> pkg_files;
        for (const auto &pkg : paths.packages)
            for (const auto &f : glob_sv(root / pkg, /*recursive=*/false))
                pkg_files.insert(f);

        std::vector<std::filesystem::path> rtl_files;
        for (const auto &f : glob_sv(root / paths.rtl, /*recursive=*/true))
            if (!pkg_files.count(f))
                rtl_files.push_back(f);

        if (pkg_files.empty() && rtl_files.empty())
            throw std::runtime_error("No .sv files found under '" + paths.rtl + "' (or its packages).");

        for (const auto &f : pkg_files)
            args.push_back(f.string());
        for (const auto &f : rtl_files)
            args.push_back(f.string());

        if (!opts.module.empty())
        {
            args.push_back("--top-module");
            args.push_back(opts.module);
            std::cout << "== Linting '" << opts.module << "' (scoped) "
                      << "==================================\n";
        }
        else
        {
            std::cout << "== Linting whole project ============================\n";
        }

        int rc = simforge::cli::utils::run_interactive("verilator", args, root);
        if (rc != 0)
            throw std::runtime_error("Lint failed (exit " + std::to_string(rc) + ")");

        std::cout << "OK: lint clean.\n";
    }
} // anonymous namespace

void register_lint_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("lint", "Fast verilator --lint-only check — no cmake, no registered testbench required");

    auto opts = std::make_shared<LintOptions>();

    cmd->add_option(
        "module",
        opts->module,
        "Optional top module to scope elaboration to (e.g. a module still in progress); "
        "omit to lint the whole project"
    );

    cmd->add_flag("--keep-going", opts->keep_going, "Don't stop at the first fatal error (still exits non-zero if any error occurred)");

    cmd->callback([opts]() { run(*opts); });
}
