#include "tb_init.hpp"

#include "../config/config_loader.hpp"
#include "../utils/sv_parser.hpp"
#include "../utils/template_engine.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>

// Embedded templates
#include "templates_embed.hpp"

namespace
{
    using namespace simforge::cli;
    using namespace simforge::cli::utils;

    // String helpers

    std::string to_upper(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    }

    std::string build_param_mirrors(const SvModule &mod)
    {
        if (mod.params.empty())
            return "// (no module parameters found)";

        std::ostringstream oss;
        for (const auto &p : mod.params)
        {
            if (p.resolved)
                oss << "constexpr int " << p.name << " = " << p.value << ";\n";
            else
                oss << "// " << p.name << " = " << p.raw_default << "  (not resolved automatically, set by hand if needed)\n";
        }
        return oss.str();
    }

    std::string build_enum_mirrors(const SvModule &mod)
    {
        std::vector<std::pair<std::string, std::vector<std::string>>> seen; // preserves discovery order
        auto consider = [&](const std::string &raw_type, const std::vector<std::string> &values)
        {
            if (values.empty())
                return;
            std::istringstream iss(raw_type);
            std::string type_name;
            iss >> type_name;
            for (const auto &s : seen)
                if (s.first == type_name)
                    return; // already emitted
            seen.emplace_back(type_name, values);
        };

        for (const auto &p : mod.ports)
            consider(p.raw_type, p.enum_values);
        for (const auto &ip : mod.iface_ports)
            for (const auto &m : ip.members)
                consider(m.raw_type, m.enum_values);

        if (seen.empty())
            return "// (no package enums found on this module's ports)";

        std::ostringstream oss;
        for (const auto &[type_name, values] : seen)
        {
            oss << "enum class " << type_name << " : uint32_t {\n";
            for (size_t i = 0; i < values.size(); ++i)
                oss << "    " << values[i] << (i + 1 < values.size() ? "," : "") << "\n";
            oss << "};\n\n";

            oss << "inline const char *to_string(" << type_name << " v)\n{\n"
                << "    switch (v)\n    {\n";
            for (const auto &v : values)
                oss << "    case " << type_name << "::" << v << ": return \"" << v << "\";\n";
            oss << "    default: return \"?\";\n    }\n}\n\n";
        }
        return oss.str();
    }

    std::string to_pascal(const std::string &s)
    {
        std::string out;
        bool cap = true;
        for (char c : s)
        {
            if (c == '_')
            {
                cap = true;
                continue;
            }
            out += cap ? (char)::toupper(c) : c;
            cap = false;
        }
        return out;
    }

    bool name_looks_like_clk(const std::string &n)
    {
        std::string l = n;
        std::transform(l.begin(), l.end(), l.begin(), ::tolower);
        return l.find("clk") != std::string::npos || l == "ck" || l.find("clock") != std::string::npos;
    }

    bool name_looks_like_rst(const std::string &n)
    {
        std::string l = n;
        std::transform(l.begin(), l.end(), l.begin(), ::tolower);
        return l.find("rst") != std::string::npos || l.find("reset") != std::string::npos;
    }

    // one interface member flattened
    struct IfaceFlat
    {
        SvPort port;
        std::string iface_port;
        std::string member;
        bool is_header_port = false;
        bool is_clk_alias = false;
        bool is_rst_alias = false;
    };

    std::vector<IfaceFlat> flatten_iface_members(const std::vector<SvIfacePort> &iface_ports)
    {
        std::vector<IfaceFlat> out;
        for (const auto &ip : iface_ports)
        {
            for (const auto &m : ip.members)
            {
                IfaceFlat f;
                f.iface_port = ip.port_name;
                f.member = m.name;
                f.is_header_port = m.is_header_port;
                f.is_clk_alias = m.is_header_port && name_looks_like_clk(m.name);
                f.is_rst_alias = m.is_header_port && name_looks_like_rst(m.name);

                f.port.name = ip.port_name + "_" + m.name;
                f.port.dir = m.dir;
                f.port.width = m.width;
                f.port.is_packed = m.width > 1;
                f.port.raw_type = m.raw_type;

                out.push_back(f);
            }
        }
        return out;
    }

    // VIF field generation

    std::string build_vif_input_fields(const std::vector<SvPort> &inputs)
    {
        std::ostringstream oss;
        for (const auto &p : inputs)
            oss << "    " << p.cpp_type() << " &" << p.name << ";  // input\n";
        return oss.str();
    }

    std::string build_vif_output_fields(const std::vector<SvPort> &outputs)
    {
        std::ostringstream oss;
        for (const auto &p : outputs)
            oss << "    " << p.cpp_type() << " &" << p.name << ";  // output\n";
        return oss.str();
    }

    std::string build_vif_ctor_init(const std::vector<SvPort> &ports, const std::string &dut_class)
    {
        std::ostringstream oss;
        bool first = true;
        oss << "        : ";
        for (const auto &p : ports)
        {
            if (!first)
                oss << ",\n          ";
            oss << p.name << "(" << dut_class << "." << p.name << ")";
            first = false;
        }
        return oss.str();
    }

    // SV wrapper generation helpers

    std::string build_sv_port_list(const std::vector<SvPort> &ports)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < ports.size(); ++i)
        {
            const auto &p = ports[i];
            std::string dir = p.is_input() ? "input  logic" : "output logic";
            std::string bits = p.width > 1 ? "[" + std::to_string(p.width - 1) + ":0] " : "";
            oss << "    " << dir << " " << bits << p.name;
            if (i + 1 < ports.size())
                oss << ",";
            oss << "\n";
        }
        return oss.str();
    }

    std::string build_sv_internal_regs(const std::vector<SvPort> &inputs, const std::vector<SvPort> &outputs)
    {
        std::ostringstream oss;
        for (const auto &p : inputs)
        {
            std::string bits = p.width > 1 ? "[" + std::to_string(p.width - 1) + ":0] " : "";
            oss << "  logic " << bits << p.name << "_r;\n";
        }
        for (const auto &p : outputs)
        {
            std::string bits = p.width > 1 ? "[" + std::to_string(p.width - 1) + ":0] " : "";
            oss << "  logic " << bits << p.name << "_w;\n";
        }
        return oss.str();
    }

    std::string build_sv_dut_connections(
        const std::vector<SvPort> &plain_ports,
        const std::vector<SvIfacePort> &iface_ports,
        const std::string &clk_port_name,
        const std::string &rst_port_name,
        bool has_own_clk,
        bool has_own_rst
    )
    {
        std::vector<std::string> lines;
        if (has_own_clk)
            lines.push_back("." + clk_port_name + "(clk)");
        if (has_own_rst)
            lines.push_back("." + rst_port_name + "(rst_n)");
        for (const auto &p : plain_ports)
            lines.push_back("." + p.name + "(" + p.name + (p.is_input() ? "_r" : "_w") + ")");
        for (const auto &ip : iface_ports)
            lines.push_back("." + ip.port_name + "(" + ip.port_name + "_if." + (ip.modport.empty() ? "/*TODO: modport*/" : ip.modport) + ")");

        std::ostringstream oss;
        for (size_t i = 0; i < lines.size(); ++i)
            oss << "      " << lines[i] << (i + 1 < lines.size() ? "," : "") << "\n";
        return oss.str();
    }

    std::string build_sv_iface_instances(const std::vector<SvIfacePort> &iface_ports, const std::vector<IfaceFlat> &flats)
    {
        std::ostringstream oss;
        for (const auto &ip : iface_ports)
        {
            std::vector<std::string> conns;
            for (const auto &f : flats)
            {
                if (f.iface_port != ip.port_name || !f.is_header_port)
                    continue;
                std::string target;
                if (f.is_clk_alias)
                    target = "clk";
                else if (f.is_rst_alias)
                    target = "rst_n";
                else
                    target = f.port.name + (f.port.is_input() ? "_r" : "_w");
                conns.push_back("." + f.member + "(" + target + ")");
            }

            oss << "  " << ip.iface_type << " " << ip.port_name << "_if (\n";
            for (size_t i = 0; i < conns.size(); ++i)
                oss << "      " << conns[i] << (i + 1 < conns.size() ? "," : "") << "\n";
            oss << "  );\n";
        }
        return oss.str();
    }

    std::string build_sv_iface_bindings(const std::vector<IfaceFlat> &flats)
    {
        std::ostringstream oss;
        for (const auto &f : flats)
        {
            if (f.is_header_port || !f.port.is_input())
                continue;
            oss << "  assign " << f.iface_port << "_if." << f.member << " = " << f.port.name << "_r;\n";
        }
        return oss.str();
    }

    struct RegLine
    {
        std::string target;
        std::string clocked_source;
    };

    std::string build_reg_block(const std::vector<RegLine> &lines, bool reset_phase)
    {
        std::ostringstream oss;
        for (const auto &l : lines)
            oss << "      " << l.target << " <= " << (reset_phase ? "'0" : l.clocked_source) << ";\n";
        return oss.str();
    }

    std::vector<RegLine> build_in_reg_lines(const std::vector<SvPort> &inputs)
    {
        std::vector<RegLine> out;
        for (const auto &p : inputs)
            out.push_back({p.name + "_r", p.name});
        return out;
    }

    std::vector<RegLine> build_out_reg_lines(const std::vector<SvPort> &plain_outputs, const std::vector<IfaceFlat> &flats)
    {
        std::vector<RegLine> out;
        for (const auto &p : plain_outputs)
            out.push_back({p.name, p.name + "_w"});
        for (const auto &f : flats)
            if (!f.is_header_port && f.port.is_output())
                out.push_back({f.port.name, f.iface_port + "_if." + f.member});
        return out;
    }

    // TB generation

    struct TbInitOptions
    {
        std::string sv_file;
        std::string top_module;
        std::string group;
        std::string out_dir;
        std::string style = "uvm";
        bool wrapper = false;
        bool no_parse = false; // skip verilator JSON parsing (offline mode)
        bool force = false;
        int trace_level = 3;
    };

    bool dir_has_files(const std::filesystem::path &dir)
    {
        if (!std::filesystem::exists(dir))
            return false;
        for (const auto &_ : std::filesystem::recursive_directory_iterator(dir))
            return true;
        return false;
    }

    void generate_uvm_tb(const TbInitOptions &opts, const SvModule &mod, const std::filesystem::path &out)
    {
        const std::string M = mod.name;
        const std::string MP = to_pascal(M);
        const std::string MU = to_upper(M);
        const std::string dut = opts.top_module + (opts.wrapper ? "_dut" : "");
        const std::string dutc = "V" + dut;

        const std::string found_clk = mod.guess_clk_signal();
        const std::string found_rst = mod.guess_rst_signal();
        const std::string clk = found_clk.empty() ? "clk" : found_clk;
        const std::string rst = found_rst.empty() ? "rst_n" : found_rst;

        // Plain module ports, excluding clk/rst
        std::vector<SvPort> plain_ports;
        for (const auto &p : mod.ports)
            if (p.name != clk && p.name != rst)
                plain_ports.push_back(p);

        auto iface_flats = flatten_iface_members(mod.iface_ports);

        std::vector<SvPort> io_ports = plain_ports;
        for (const auto &f : iface_flats)
            if (!f.is_clk_alias && !f.is_rst_alias)
                io_ports.push_back(f.port);

        std::vector<SvPort> io_inputs, io_outputs;
        for (const auto &p : io_ports)
            (p.is_input() ? io_inputs : io_outputs).push_back(p);

        std::vector<SvPort> w_outputs;
        for (const auto &p : plain_ports)
            if (p.is_output())
                w_outputs.push_back(p);
        for (const auto &f : iface_flats)
            if (f.is_header_port && !f.is_clk_alias && !f.is_rst_alias && f.port.is_output())
                w_outputs.push_back(f.port);

        TemplateContext ctx = {
            {"MODULE", M},
            {"MODULE_PASCAL", MP},
            {"MODULE_UPPER", MU},
            {"TOP_MODULE", dut},
            {"DUT_CLASS", dutc},
            {"CLK_SIGNAL", clk},
            {"RST_SIGNAL", rst},
            {"TRACE_LEVEL", std::to_string(opts.trace_level)},
            {"VIF_INPUT_FIELDS", build_vif_input_fields(io_inputs)},
            {"VIF_OUTPUT_FIELDS", build_vif_output_fields(io_outputs)},
            {"VIF_CTOR_INIT", build_vif_ctor_init(io_ports, "DUT")},
            {"ENUM_MIRRORS", build_enum_mirrors(mod)},
            {"PARAM_MIRRORS", build_param_mirrors(mod)},
        };

        auto emit = [&](std::string_view tmpl, const std::filesystem::path &rel)
        {
            TemplateEngine::render_string_to_file(tmpl, out / rel, ctx);
            std::cout << "  -> " << (out / rel).string() << "\n";
        };

        std::cout << "\nGenerating UVM testbench for '" << M << "'...\n";
        if (!mod.iface_ports.empty())
        {
            std::cout << "  Found " << mod.iface_ports.size() << " interface port(s): ";
            for (size_t i = 0; i < mod.iface_ports.size(); ++i)
                std::cout << (i ? ", " : "") << mod.iface_ports[i].port_name << ":" << mod.iface_ports[i].iface_type << (mod.iface_ports[i].modport.empty() ? "" : ("." + mod.iface_ports[i].modport));
            std::cout << " (flattened to " << io_ports.size() - plain_ports.size() << " signal(s) — verify widths on any"
                      << " package-typed member, see warnings above)\n";
        }

        emit(tmpl::uvm::defs_hpp, M + "_defs.hpp");
        emit(tmpl::uvm::data_hpp, M + "_data.hpp");
        emit(tmpl::uvm::tb_cpp, M + "_tb.cpp");
        emit(tmpl::uvm::uvm::env_hpp, "uvm/" + M + "_env.hpp");
        emit(tmpl::uvm::uvm::env_cpp, "uvm/" + M + "_env.cpp");
        emit(tmpl::uvm::uvm::uvm_hpp, "uvm/" + M + "_uvm.hpp");
        emit(tmpl::uvm::uvm::scb_hpp, "uvm/" + M + "_scb.hpp");
        emit(tmpl::uvm::uvm::cov_hpp, "uvm/" + M + "_cov.hpp");
        emit(tmpl::uvm::uvm::agent::drv_hpp, "uvm/agent/" + M + "_drv.hpp");
        emit(tmpl::uvm::uvm::agent::mon_hpp, "uvm/agent/" + M + "_mon.hpp");
        emit(tmpl::uvm::uvm::agent::seq_hpp, "uvm/agent/" + M + "_seq.hpp");
        emit(tmpl::uvm::uvm::signals::vif_hpp, "uvm/signals/" + M + "_vif.hpp");

        if (opts.wrapper)
        {
            ctx["SV_PACKAGE_IMPORTS"] = "// TODO: import packages if needed\n// import my_pkg::*;";
            ctx["SV_PORT_LIST"] = build_sv_port_list(io_ports);
            ctx["SV_INTERNAL_REGS"] = build_sv_internal_regs(io_inputs, w_outputs);
            ctx["SV_IFACE_INSTANCES"] = build_sv_iface_instances(mod.iface_ports, iface_flats);
            ctx["SV_PORT_CONNECTIONS"] = build_sv_dut_connections(plain_ports, mod.iface_ports, clk, rst, !found_clk.empty(), !found_rst.empty());
            ctx["SV_IFACE_BINDINGS"] = build_sv_iface_bindings(iface_flats);
            ctx["SV_RESET_BLOCK"] = build_reg_block(build_in_reg_lines(io_inputs), /*reset_phase=*/true);
            ctx["SV_REG_BLOCK"] = build_reg_block(build_in_reg_lines(io_inputs), /*reset_phase=*/false);

            auto out_lines = build_out_reg_lines(w_outputs, iface_flats);
            ctx["SV_OUT_RESET_BLOCK"] = build_reg_block(out_lines, /*reset_phase=*/true);
            ctx["SV_OUT_REG_BLOCK"] = build_reg_block(out_lines, /*reset_phase=*/false);

            if (mod.iface_ports.empty())
                ctx["SV_IFACE_INSTANCES"] = "  // (no interface ports on this module)";
            if (iface_flats.empty())
                ctx["SV_IFACE_BINDINGS"] = "  // (no interface body signals to drive)";

            emit(tmpl::uvm::sv::dut_wrapper_sv, "sv/" + dut + ".sv");
        }
    }

    void generate_basic_tb(const TbInitOptions &opts, const SvModule &mod, const std::filesystem::path &out)
    {
        const std::string M = mod.name;
        const std::string clk = mod.guess_clk_signal().empty() ? "clk" : mod.guess_clk_signal();
        const std::string rst = mod.guess_rst_signal().empty() ? "rst_n" : mod.guess_rst_signal();

        TemplateContext ctx = {
            {"MODULE", M},
            {"MODULE_UPPER", to_upper(M)},
            {"MODULE_PASCAL", to_pascal(M)},
            {"DUT_CLASS", "V" + opts.top_module + (opts.wrapper ? "_dut" : "")},
            {"CLK_SIGNAL", clk},
            {"RST_SIGNAL", rst},
        };

        std::cout << "\nGenerating basic testbench for '" << M << "'...\n";

        auto emit = [&](std::string_view tmpl, const std::string &name)
        {
            TemplateEngine::render_string_to_file(tmpl, out / name, ctx);
            std::cout << "  -> " << (out / name).string() << "\n";
        };

        emit(tmpl::basic::defs_hpp, M + "_defs.hpp");
        emit(tmpl::basic::tb_cpp, M + "_tb.cpp");
    }

    // Command runner

    void run(const TbInitOptions &opts)
    {
        const auto proj_root = std::filesystem::current_path();
        const std::string tb_name = opts.top_module.empty() ? std::filesystem::path(opts.sv_file).stem().string() : opts.top_module;
        const std::string subdir = opts.group.empty() ? tb_name : opts.group + "/" + tb_name;

        const bool has_cfg = std::filesystem::exists(proj_root / simforge::cli::kConfigFilename);
        std::optional<simforge::cli::SimforgeConfig> cfg;
        if (has_cfg)
            cfg = simforge::cli::ConfigLoader::load(proj_root);

        if (cfg && cfg->find_tb(subdir) && !opts.force)
            throw std::runtime_error(
                "Testbench '" + subdir +
                "' is already registered in simforge.toml.\n"
                "Use --force to regenerate it, or a different --top/--group."
            );

        const std::string tb_root = cfg ? cfg->paths.tb : "tb";
        const auto out = opts.out_dir.empty() ? (proj_root / tb_root / subdir) : (proj_root / opts.out_dir);

        if (dir_has_files(out) && !opts.force)
            throw std::runtime_error(
                "Output directory '" + out.string() +
                "' already exists and is not empty.\n"
                "Use --force to overwrite (this will clobber any hand-edited files in it)."
            );

        // Parse SV module via verilator --json-only
        SvModule mod;
        mod.name = tb_name;

        if (!opts.no_parse)
        {
            std::vector<std::filesystem::path> pkg_dirs;
            if (cfg)
                for (const auto &p : cfg->paths.packages)
                    pkg_dirs.push_back(proj_root / p);

            bool json_ok = false;
            std::cout << "Parsing '" << opts.sv_file << "' with verilator...\n";
            try
            {
                mod = parse_sv_module(opts.sv_file, tb_name, pkg_dirs, {proj_root}, {"--sv"});
                json_ok = true;
                std::cout << "  Found " << mod.ports.size() << " plain port(s) on module '" << mod.name << "'\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "Warning: verilator port parsing failed: " << e.what() << "\n"
                          << "         Falling back to a text-based port scan (widths from named/package "
                             "types won't be as reliable - this is expected if the module has an interface "
                             "port, which verilator can't elaborate as a lone top).\n";
                mod.name = tb_name;
            }

            try
            {
                auto scan = scan_module_text(opts.sv_file, tb_name, {proj_root}, pkg_dirs, {proj_root}, {"--sv"});
                mod.iface_ports = scan.iface_ports;
                mod.params = scan.params;
                if (!json_ok)
                    mod.ports = scan.plain_ports;

                if (!mod.params.empty())
                {
                    std::cout << "  Found " << mod.params.size() << " parameter(s): ";
                    for (size_t i = 0; i < mod.params.size(); ++i)
                    {
                        const auto &p = mod.params[i];
                        std::cout << (i ? ", " : "") << p.name << "=" << (p.resolved ? std::to_string(p.value) : "?");
                    }
                    std::cout << "\n";
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Warning: interface-port text scan failed: " << e.what() << "\n"
                          << "         Generating placeholder VIF. Edit manually.\n";
            }

            annotate_enum_values(mod, pkg_dirs);
        }

        // Generate files
        if (opts.style == "basic")
            generate_basic_tb(opts, mod, out);
        else
            generate_uvm_tb(opts, mod, out);

        // Register TB in simforge.toml if present
        if (has_cfg)
        {
            TestbenchConfig tb_cfg;
            tb_cfg.name = tb_name;
            tb_cfg.group = opts.group;
            tb_cfg.top = tb_name + (opts.wrapper ? "_dut" : "");
            tb_cfg.style = (opts.style == "basic") ? TbStyle::Basic : TbStyle::UVM;
            tb_cfg.verilator.trace_level = opts.trace_level;

            simforge::cli::ConfigLoader::upsert_testbench(tb_cfg, proj_root, opts.force);
            std::cout << "\nOK: Registered '" << subdir << "' in simforge.toml\n";
        }

        std::cout << "\nDone. Next:\n"
                  << "  simforge build " << subdir << "\n";
    }

} // anonymous namespace

void register_tb_commands(CLI::App &app)
{
    auto *tb = app.add_subcommand("tb", "Testbench management");
    tb->require_subcommand(1);

    // tb init
    auto *tb_init = tb->add_subcommand("init", "Generate a testbench scaffold from a SystemVerilog top module");

    auto opts = std::make_shared<TbInitOptions>();

    tb_init->add_option("sv_file", opts->sv_file, "Path to the SystemVerilog source file")->required()->check(CLI::ExistingFile);

    tb_init->add_option("--top", opts->top_module, "Top module name (inferred from filename if omitted)");

    tb_init->add_option("--group", opts->group, "Namespace/category folder for this tb, e.g. 'mos6502' (mirrors rtl/<group>/)");

    tb_init->add_option("--out", opts->out_dir, "Output directory (default: <tb_root>/<group>/<name>, computed from simforge.toml)");

    tb_init->add_option("--style", opts->style, "Testbench style: 'uvm' (default) or 'basic'")->check(CLI::IsMember({"uvm", "basic"}));

    tb_init->add_flag("--wrapper", opts->wrapper, "Also generate a synchronous DUT wrapper .sv file");

    tb_init->add_option("--trace-level", opts->trace_level, "Verilator trace depth (default: 3)")->default_val(3);

    tb_init->add_flag("--no-parse", opts->no_parse, "Skip verilator port parsing (useful offline)");

    tb_init->add_flag("--force", opts->force, "Overwrite an already-registered testbench / existing generated files");

    tb_init->callback([opts]() { run(*opts); });
}
