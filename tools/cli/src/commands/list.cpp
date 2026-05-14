#include "list.hpp"

#include "../config/config_loader.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

namespace
{
    void run()
    {
        const auto root = std::filesystem::current_path();
        const auto cfg = simforge::cli::ConfigLoader::load(root);

        std::cout << "Project : " << cfg.project.name << " " << cfg.project.version << "\n"
                  << "RTL     : " << cfg.paths.rtl << "\n"
                  << "Build   : " << cfg.paths.build << "\n\n";

        if (cfg.testbenches.empty())
        {
            std::cout << "No testbenches registered yet.\n"
                      << "Add one with: simforge tb init <rtl/module.sv>\n";
            return;
        }

        std::cout << "Testbenches (" << cfg.testbenches.size() << "):\n";
        for (const auto &tb : cfg.testbenches)
        {
            const auto bin = root / cfg.paths.build / "tb" / tb.name / (tb.name + "_tb");
            bool built = std::filesystem::exists(bin);
            std::string style = (tb.style == simforge::cli::TbStyle::UVM) ? "uvm" : "basic";

            std::cout << "  " << (built ? "OK:" : "○") << " " << tb.name << "  [" << style << "]"
                      << "  top=" << tb.top << (built ? "  (built)" : "  (not built)") << "\n";
        }
        std::cout << "\n";
    }
} // anonymous namespace

void register_list_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("list", "List all testbenches registered in simforge.toml");

    cmd->callback([]() { run(); });
}
