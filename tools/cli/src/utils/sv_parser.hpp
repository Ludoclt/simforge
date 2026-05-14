#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace simforge::cli::utils
{
    enum class PortDir
    {
        Input,
        Output,
        InOut,
        Unknown
    };

    struct SvPort
    {
        std::string name;
        PortDir dir = PortDir::Unknown;
        int width = 1;
        bool is_packed = false;
        std::string raw_type;

        bool is_input() const { return dir == PortDir::Input; }
        bool is_output() const { return dir == PortDir::Output; }

        std::string cpp_type() const;
    };

    struct SvModule
    {
        std::string name;
        std::vector<SvPort> ports;
        std::string json_path;

        std::vector<SvPort> inputs() const;
        std::vector<SvPort> outputs() const;

        std::string guess_clk_signal() const;
        std::string guess_rst_signal() const;
    };

    SvModule parse_sv_module(
        const std::filesystem::path &sv_file,
        const std::string &top_module,
        const std::vector<std::filesystem::path> &pkg_dirs = {},
        const std::vector<std::filesystem::path> &include_dirs = {},
        const std::vector<std::string> &extra_args = {},
        const std::filesystem::path &work_dir = ""
    );
} // namespace simforge::cli::utils
