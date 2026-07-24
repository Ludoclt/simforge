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

    struct StructFieldInfo
    {
        std::string name;
        int width = 1;
        std::string raw_type;
    };

    struct SvPort
    {
        std::string name;
        PortDir dir = PortDir::Unknown;
        int width = 1;
        bool is_packed = false;
        std::string raw_type;
        std::vector<std::string> enum_values;
        std::vector<StructFieldInfo> struct_fields;

        bool is_input() const { return dir == PortDir::Input; }
        bool is_output() const { return dir == PortDir::Output; }

        std::string cpp_type() const;
    };

    struct SvIfaceMember
    {
        std::string name;
        PortDir dir = PortDir::Unknown;
        int width = 1;
        std::string raw_type;
        std::vector<std::string> enum_values;
        std::vector<StructFieldInfo> struct_fields;

        bool is_header_port = false;

        bool is_input() const { return dir == PortDir::Input; }
        bool is_output() const { return dir == PortDir::Output; }
    };

    struct SvIfacePort
    {
        std::string port_name;
        std::string iface_type;
        std::string modport;
        std::vector<SvIfaceMember> members;
    };

    struct SvParam
    {
        std::string name;
        std::string raw_default;
        long value = 0;
        bool resolved = false;
    };

    struct SvModule
    {
        std::string name;
        std::vector<SvPort> ports;
        std::vector<SvIfacePort> iface_ports;
        std::vector<SvParam> params;
        std::string json_path;

        std::vector<SvPort> inputs() const;
        std::vector<SvPort> outputs() const;

        std::string guess_clk_signal() const;
        std::string guess_rst_signal() const;
    };

    struct EnumInfo
    {
        std::string type_name;
        std::vector<std::string> values;
    };

    std::vector<EnumInfo> parse_package_enums(const std::vector<std::filesystem::path> &pkg_dirs);

    void annotate_enum_values(SvModule &mod, const std::vector<std::filesystem::path> &pkg_dirs);

    struct StructInfo
    {
        std::string type_name;
        std::vector<StructFieldInfo> fields;
    };

    std::vector<StructInfo> parse_package_structs(const std::vector<std::filesystem::path> &pkg_dirs);

    void annotate_struct_fields(SvModule &mod, const std::vector<std::filesystem::path> &pkg_dirs);

    SvModule parse_sv_module(
        const std::filesystem::path &sv_file,
        const std::string &top_module,
        const std::vector<std::filesystem::path> &pkg_dirs = {},
        const std::vector<std::filesystem::path> &include_dirs = {},
        const std::vector<std::string> &extra_args = {},
        const std::filesystem::path &work_dir = ""
    );

    struct SvTextScanResult
    {
        std::vector<SvPort> plain_ports;
        std::vector<SvIfacePort> iface_ports;
        std::vector<SvParam> params;
    };

    SvTextScanResult scan_module_text(
        const std::filesystem::path &sv_file,
        const std::string &top_module,
        const std::vector<std::filesystem::path> &search_dirs,
        const std::vector<std::filesystem::path> &pkg_dirs = {},
        const std::vector<std::filesystem::path> &include_dirs = {},
        const std::vector<std::string> &extra_args = {}
    );
} // namespace simforge::cli::utils
