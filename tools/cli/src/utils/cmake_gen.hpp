#pragma once

#include "../config/simforge_config.hpp"

#include <filesystem>

namespace simforge::cli::utils
{
    void generate_tb_cmake(const simforge::cli::SimforgeConfig &cfg, const std::string &tb_name, const std::filesystem::path &project_root);
} // namespace simforge::cli::utils
