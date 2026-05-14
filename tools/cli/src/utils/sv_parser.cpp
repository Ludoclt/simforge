#include "sv_parser.hpp"

#include "nlohmann/json.hpp"
#include "process.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace simforge::cli::utils
{
    std::string SvPort::cpp_type() const
    {
        if (width <= 8)
            return "vluint8_t";
        if (width <= 16)
            return "vluint16_t";
        if (width <= 32)
            return "vluint32_t";
        if (width <= 64)
            return "vluint64_t";
        int words = (width + 31) / 32;
        return "VlWide<" + std::to_string(words) + ">";
    }

    std::vector<SvPort> SvModule::inputs() const
    {
        std::vector<SvPort> out;
        for (const auto &p : ports)
            if (p.is_input())
                out.push_back(p);
        return out;
    }

    std::vector<SvPort> SvModule::outputs() const
    {
        std::vector<SvPort> out;
        for (const auto &p : ports)
            if (p.is_output())
                out.push_back(p);
        return out;
    }

    static bool matches_any(const std::string &name, std::initializer_list<const char *> patterns)
    {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        for (const char *p : patterns)
            if (lower.find(p) != std::string::npos)
                return true;
        return false;
    }

    std::string SvModule::guess_clk_signal() const
    {
        for (const auto &p : ports)
            if (p.is_input() && p.width == 1 && matches_any(p.name, {"clk", "clock", "ck"}))
                return p.name;
        return {};
    }

    std::string SvModule::guess_rst_signal() const
    {
        for (const auto &p : ports)
            if (p.is_input() && p.width == 1 && matches_any(p.name, {"rst", "reset", "rstn", "rst_n", "resetn"}))
                return p.name;
        return {};
    }

    namespace
    {
        using json = nlohmann::json;

        void buildAddrMap(const json &node, std::unordered_map<std::string, json> &map)
        {
            if (!node.is_object())
                return;
            if (node.contains("addr") && node["addr"].is_string())
            {
                map[node["addr"].get<std::string>()] = node;
            }
            for (auto &[key, val] : node.items())
            {
                if (val.is_array())
                {
                    for (auto &child : val)
                        buildAddrMap(child, map);
                }
                else if (val.is_object())
                {
                    buildAddrMap(val, map);
                }
            }
        }

        int parseRangeWidth(const std::string &range)
        {
            auto colon = range.find(':');
            if (colon == std::string::npos)
                return 1;
            int hi = std::stoi(range.substr(0, colon));
            int lo = std::stoi(range.substr(colon + 1));
            return std::abs(hi - lo) + 1;
        }

        int resolveWidth(const std::string &addr, const std::unordered_map<std::string, json> &addrMap)
        {
            auto it = addrMap.find(addr);
            if (it == addrMap.end())
                return 1;
            const json &dtype = it->second;
            const std::string type = dtype.value("type", "");

            if (type == "BASICDTYPE")
            {
                if (dtype.contains("range") && dtype["range"].is_string())
                    return parseRangeWidth(dtype["range"].get<std::string>());
                return 1;
            }
            if (type == "ENUMDTYPE" || type == "REFDTYPE")
            {
                if (dtype.contains("refDTypep") && dtype["refDTypep"].is_string())
                    return resolveWidth(dtype["refDTypep"].get<std::string>(), addrMap);
                return 1;
            }
            if (type == "STRUCTDTYPE")
            {
                int total = 0;
                for (auto &member : dtype.value("membersp", json::array()))
                    total += resolveWidth(member.value("refDTypep", ""), addrMap);
                return total > 0 ? total : 1;
            }
            return 1;
        }

        SvModule parse_json(const std::filesystem::path &json_path, const std::string &top_name)
        {
            SvModule mod;
            mod.name = top_name;
            mod.json_path = json_path.string();

            std::ifstream file(json_path);

            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open file: " + json_path.string());
            }

            json netlist = json::parse(file);

            std::unordered_map<std::string, json> addrMap;
            buildAddrMap(netlist, addrMap);

            for (const json &j_mod : netlist["modulesp"])
            {
                if (j_mod.value("type", "") != "MODULE" || j_mod.value("name", "") != top_name)
                    continue;

                for (const json &stmt : j_mod["stmtsp"])
                {
                    if (stmt.value("type", "") != "VAR" || !stmt.value("isPrimaryIO", false))
                        continue;

                    SvPort port;
                    port.name = stmt["name"].get<std::string>();
                    port.raw_type = stmt.value("dtypeName", "");
                    port.width = resolveWidth(stmt.value("dtypep", ""), addrMap);
                    port.is_packed = (port.width > 1);

                    std::string dir = stmt.value("direction", "");
                    if (dir == "INPUT")
                        port.dir = PortDir::Input;
                    else if (dir == "OUTPUT")
                        port.dir = PortDir::Output;
                    else if (dir == "INOUT")
                        port.dir = PortDir::InOut;
                    else
                        port.dir = PortDir::Unknown;

                    mod.ports.push_back(port);
                }
            }

            return mod;
        }
    } // anonymous namespace

    SvModule parse_sv_module(
        const std::filesystem::path &sv_file,
        const std::string &top_module,
        const std::vector<std::filesystem::path> &pkg_dirs,
        const std::vector<std::filesystem::path> &include_dirs,
        const std::vector<std::string> &extra_args,
        const std::filesystem::path &work_dir_in
    )
    {
        if (which("verilator").empty())
            throw std::runtime_error(
                "verilator not found on PATH.\n"
                "Install it via your package manager or from https://verilator.org"
            );

        // Determine working directory for JSON output
        std::filesystem::path work_dir = work_dir_in.empty() ? std::filesystem::temp_directory_path() / ("simforge_" + top_module) : work_dir_in;

        std::filesystem::create_directories(work_dir);

        // Build verilator args
        std::vector<std::string> args = {"--json-only", "--top-module", top_module};

        std::vector<std::filesystem::path> pkg_files;

        for (const auto &dir : pkg_dirs)
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(dir))
            {
                if (!entry.is_regular_file())
                    continue;

                auto ext = entry.path().extension().string();

                if (ext == ".sv" || ext == ".v")
                {
                    pkg_files.push_back(entry.path());
                }
            }
        }

        for (const auto &a : extra_args)
            args.push_back(a);

        for (const auto &f : pkg_files)
            args.push_back(f.string());

        for (const auto &d : include_dirs)
            args.push_back("+incdir+" + d.string());

        args.push_back(sv_file.string());

        auto result = run("verilator", args, work_dir);

        if (!result.ok())
        {
            throw std::runtime_error("verilator --json-only failed (exit " + std::to_string(result.exit_code) + "):\n" + result.stderr_output);
        }

        std::filesystem::path json_path = work_dir / ("V" + top_module + ".tree.json");

        if (!std::filesystem::exists(json_path))
        {
            throw std::runtime_error("Could not find JSON output from verilator in " + work_dir.string());
        }

        return parse_json(json_path, top_module);
    }
} // namespace simforge::cli::utils
