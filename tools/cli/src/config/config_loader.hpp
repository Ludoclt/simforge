#pragma once

#include "simforge_config.hpp"

#include <filesystem>
#include <string>

namespace simforge::cli
{
    inline constexpr const char *kConfigFilename = "simforge.toml";

    class ConfigLoader
    {
      public:
        static SimforgeConfig load(const std::filesystem::path &dir = std::filesystem::current_path());

        static void write_default(const SimforgeConfig &cfg, const std::filesystem::path &dir = std::filesystem::current_path(), bool force = false);

        static void save(const SimforgeConfig &cfg, const std::filesystem::path &dir = std::filesystem::current_path());

        static void upsert_testbench(const TestbenchConfig &tb, const std::filesystem::path &dir = std::filesystem::current_path(), bool force = false);

      private:
        static TbStyle parse_style(std::string_view s);
        static std::string style_to_string(TbStyle s);
        static void write_header(std::ostream &f, const SimforgeConfig &cfg);
        static void write_tb_block(std::ostream &f, const TestbenchConfig &tb);
    };
} // namespace simforge::cli
