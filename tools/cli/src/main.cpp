#include "commands/build.hpp"
#include "commands/init.hpp"
#include "commands/lint.hpp"
#include "commands/list.hpp"
#include "commands/run.hpp"
#include "commands/tb_init.hpp"
#include "commands/wave.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[])
{
    CLI::App app{"simforge - HDL verification toolkit"};
    app.set_version_flag("--version,-V", "0.1.0");
    argv = app.ensure_utf8(argv);

    app.require_subcommand(1);

    // Register all commands
    register_init_command(app);
    register_tb_commands(app);
    register_build_command(app);
    register_run_command(app);
    register_wave_command(app);
    register_list_command(app);
    register_lint_command(app);

    // Parse
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return app.exit(e);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
