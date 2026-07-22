#include "list.hpp"

#include "../config/config_loader.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    void run(const std::string &group_filter)
    {
        const auto root = std::filesystem::current_path();
        const auto cfg = simforge::cli::ConfigLoader::load(root);

        std::cout << "Project : " << cfg.project.name << " " << cfg.project.version << "\n"
                  << "RTL     : " << cfg.paths.rtl << "\n"
                  << "Build   : " << cfg.paths.build << "\n\n";

        std::vector<std::reference_wrapper<const simforge::cli::TestbenchConfig>> shown;
        for (const auto &tb : cfg.testbenches)
            if (group_filter.empty() || tb.group == group_filter)
                shown.push_back(std::cref(tb));

        if (shown.empty())
        {
            if (!group_filter.empty())
                std::cout << "No testbenches in group '" << group_filter << "'.\n";
            else
                std::cout << "No testbenches registered yet.\n"
                          << "Add one with: simforge tb init <rtl/module.sv>\n";
            return;
        }

        std::cout << "Testbenches (" << shown.size() << "):\n";
        for (const auto &ref : shown)
        {
            const auto &tb = ref.get();
            const auto bin = root / cfg.paths.build / "tb" / tb.subdir() / (tb.name + "_tb");
            bool built = std::filesystem::exists(bin);
            std::string style = (tb.style == simforge::cli::TbStyle::UVM) ? "uvm" : "basic";
            std::string group_tag = tb.group.empty() ? "" : "[" + tb.group + "] ";

            std::cout << "  " << (built ? "OK:" : "\xE2\x97\x8B") << " " << group_tag << tb.name << "  [" << style << "]"
                      << "  top=" << tb.top << (built ? "  (built)" : "  (not built)") << "\n";
        }
        std::cout << "\n";
    }
} // anonymous namespace

void register_list_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("list", "List all testbenches registered in simforge.toml");

    auto group = std::make_shared<std::string>();
    cmd->add_option("--group", *group, "Only list testbenches in this group");

    cmd->callback([group]() { run(*group); });
}
