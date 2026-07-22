#include "wave.hpp"

#include "../config/config_loader.hpp"
#include "../utils/process.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

namespace
{
    void run(const std::string &tb_name)
    {
        const auto root = std::filesystem::current_path();
        const auto cfg = simforge::cli::ConfigLoader::load(root);

        const auto &tb = cfg.get_tb(tb_name);

        const auto build_dir = root / cfg.paths.build / "tb" / tb.subdir();
        std::filesystem::path wave_file;

        if (std::filesystem::exists(build_dir))
            for (const auto &entry : std::filesystem::directory_iterator(build_dir))
            {
                const auto ext = entry.path().extension().string();
                if (ext == ".vcd" || ext == ".fst")
                {
                    wave_file = entry.path();
                    break;
                }
            }

        if (wave_file.empty())
            throw std::runtime_error("No waveform file found in " + build_dir.string() + "\nRun 'simforge run " + tb.subdir() + "' first.");

        const std::string viewer = simforge::cli::utils::which("gtkwave").empty() ? "gtkwave" : simforge::cli::utils::which("gtkwave");

        if (viewer.empty())
            throw std::runtime_error("gtkwave not found on PATH - install it first.");

        std::cout << "Opening " << wave_file.filename().string() << " in gtkwave...\n";

        simforge::cli::utils::run_interactive("gtkwave", {wave_file.string()}, root);
    }
} // anonymous namespace

void register_wave_command(CLI::App &app)
{
    auto *cmd = app.add_subcommand("wave", "Open the latest waveform dump in GTKWave");

    auto tb_name = std::make_shared<std::string>();
    cmd->add_option("tb_name", *tb_name, "Testbench name (bare, or '<group>/<name>' to disambiguate)")->required();

    cmd->callback([tb_name]() { run(*tb_name); });
}
