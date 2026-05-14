#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace simforge::cli::utils
{
    struct ProcessResult
    {
        int exit_code = 0;
        std::string stdout_output;
        std::string stderr_output;

        bool ok() const { return exit_code == 0; }
    };

    ProcessResult run(const std::string &cmd, const std::vector<std::string> &args, const std::filesystem::path &cwd = std::filesystem::current_path());

    int run_interactive(const std::string &cmd, const std::vector<std::string> &args, const std::filesystem::path &cwd = std::filesystem::current_path());

    std::string which(const std::string &name);

} // namespace simforge::cli::utils
