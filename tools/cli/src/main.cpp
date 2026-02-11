#include <CLI/CLI.hpp>

extern void register_init_command(CLI::App &app);

int main(int argc, char *argv[])
{
    CLI::App app;
    argv = app.ensure_utf8(argv);
    app.description("HDL verification tookit");

    register_init_command(app);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
