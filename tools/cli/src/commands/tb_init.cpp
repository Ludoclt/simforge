#include "tb_init.hpp"

#include "../config/config_loader.hpp"
#include "../utils/sv_parser.hpp"
#include "../utils/template_engine.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
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

    std::string build_sv_internal_regs(const std::vector<SvPort> &inputs)
    {
        std::ostringstream oss;
        for (const auto &p : inputs)
        {
            std::string bits = p.width > 1 ? "[" + std::to_string(p.width - 1) + ":0] " : "";
            oss << "  logic " << bits << p.name << "_r;\n";
        }
        return oss.str();
    }

    std::string build_sv_port_connections(const std::vector<SvPort> &ports)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < ports.size(); ++i)
        {
            oss << "      ." << ports[i].name << "(" << ports[i].name << (ports[i].is_input() ? "_r" : "_w") << ")";
            if (i + 1 < ports.size())
                oss << ",";
            oss << "\n";
        }
        return oss.str();
    }

    std::string build_sv_reset_block(const std::vector<SvPort> &inputs)
    {
        std::ostringstream oss;
        for (const auto &p : inputs)
            oss << "      " << p.name << "_r <= '0;\n";
        return oss.str();
    }

    std::string build_sv_reg_block(const std::vector<SvPort> &inputs)
    {
        std::ostringstream oss;
        for (const auto &p : inputs)
            oss << "      " << p.name << "_r <= " << p.name << ";\n";
        return oss.str();
    }

    // TB generation

    struct TbInitOptions
    {
        std::string sv_file;
        std::string top_module;
        std::string out_dir;
        std::string style = "uvm";
        bool wrapper = false;
        bool no_parse = false; // skip verilator JSON parsing (offline mode)
        int trace_level = 3;
    };

    void generate_uvm_tb(const TbInitOptions &opts, const SvModule &mod, const std::filesystem::path &out)
    {
        const std::string M = mod.name;
        const std::string MP = to_pascal(M);
        const std::string MU = to_upper(M);
        const std::string dut = opts.top_module + (opts.wrapper ? "_dut" : "");
        const std::string dutc = "V" + dut;

        const std::string clk = mod.guess_clk_signal().empty() ? "clk" : mod.guess_clk_signal();
        const std::string rst = mod.guess_rst_signal().empty() ? "rst_n" : mod.guess_rst_signal();

        // Filter out clk/rst from VIF
        std::vector<SvPort> vif_ports;
        for (const auto &p : mod.ports)
            if (p.name != clk && p.name != rst)
                vif_ports.push_back(p);

        std::vector<SvPort> vif_inputs, vif_outputs;
        for (const auto &p : vif_ports)
            (p.is_input() ? vif_inputs : vif_outputs).push_back(p);

        TemplateContext ctx = {
            {"MODULE", M},
            {"MODULE_PASCAL", MP},
            {"MODULE_UPPER", MU},
            {"TOP_MODULE", dut},
            {"DUT_CLASS", dutc},
            {"CLK_SIGNAL", clk},
            {"RST_SIGNAL", rst},
            {"TRACE_LEVEL", std::to_string(opts.trace_level)},
            {"VIF_INPUT_FIELDS", build_vif_input_fields(vif_inputs)},
            {"VIF_OUTPUT_FIELDS", build_vif_output_fields(vif_outputs)},
            {"VIF_CTOR_INIT", build_vif_ctor_init(vif_ports, "DUT")},
        };

        auto emit = [&](std::string_view tmpl, const std::filesystem::path &rel)
        {
            TemplateEngine::render_string_to_file(tmpl, out / rel, ctx);
            std::cout << "  -> " << (out / rel).string() << "\n";
        };

        std::cout << "\nGenerating UVM testbench for '" << M << "'...\n";

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
            // Build SV wrapper context additions
            std::vector<SvPort> rtl_ports;
            for (const auto &p : mod.ports)
                if (p.name != clk && p.name != rst)
                    rtl_ports.push_back(p);

            ctx["SV_PACKAGE_IMPORTS"] = "// TODO: import packages if needed\n// import my_pkg::*;";
            ctx["SV_PORT_LIST"] = build_sv_port_list(rtl_ports);
            ctx["SV_INTERNAL_REGS"] = build_sv_internal_regs(vif_inputs);
            ctx["SV_PORT_CONNECTIONS"] = build_sv_port_connections(rtl_ports);
            ctx["SV_RESET_BLOCK"] = build_sv_reset_block(vif_inputs);
            ctx["SV_REG_BLOCK"] = build_sv_reg_block(vif_inputs);
            ctx["SV_OUT_RESET_BLOCK"] = "      // TODO";
            ctx["SV_OUT_REG_BLOCK"] = "      // TODO";

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

        const auto out = proj_root / opts.out_dir;

        // Parse SV module via verilator --json-only
        SvModule mod;
        if (!opts.no_parse && !opts.wrapper)
        {
            std::cout << "Parsing '" << opts.sv_file << "' with verilator...\n";
            try
            {
                // Load config for pkg/include dirs if simforge.toml exists
                std::vector<std::filesystem::path> pkg_dirs;
                if (std::filesystem::exists(proj_root / simforge::cli::kConfigFilename))
                {
                    auto cfg = simforge::cli::ConfigLoader::load(proj_root);
                    for (const auto &p : cfg.paths.packages)
                        pkg_dirs.push_back(proj_root / p);
                }

                mod = parse_sv_module(opts.sv_file, tb_name, pkg_dirs, {proj_root}, {"--sv"});
                std::cout << "  Found " << mod.ports.size() << " ports on module '" << mod.name << "'\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "Warning: port parsing failed: " << e.what() << "\n"
                          << "         Generating placeholder VIF. Edit manually.\n";
                mod.name = tb_name;
            }
        }
        else
        {
            mod.name = tb_name;
        }

        // Generate files
        if (opts.style == "basic")
            generate_basic_tb(opts, mod, out);
        else
            generate_uvm_tb(opts, mod, out);

        // Register TB in simforge.toml if present
        if (std::filesystem::exists(proj_root / simforge::cli::kConfigFilename))
        {
            TestbenchConfig tb_cfg;
            tb_cfg.name = tb_name;
            tb_cfg.top = tb_name + (opts.wrapper ? "_dut" : "");
            tb_cfg.style = (opts.style == "basic") ? TbStyle::Basic : TbStyle::UVM;
            tb_cfg.verilator.trace_level = opts.trace_level;

            simforge::cli::ConfigLoader::append_testbench(tb_cfg, proj_root);
            std::cout << "\nOK: Registered '" << tb_name << "' in simforge.toml\n";
        }

        std::cout << "\nDone. Next:\n"
                  << "  simforge build " << tb_name << "\n";
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

    tb_init->add_option("--out", opts->out_dir, "Output directory for generated files")->default_val("tb");

    tb_init->add_option("--style", opts->style, "Testbench style: 'uvm' (default) or 'basic'")->check(CLI::IsMember({"uvm", "basic"}));

    tb_init->add_flag("--wrapper", opts->wrapper, "Also generate a synchronous DUT wrapper .sv file");

    tb_init->add_option("--trace-level", opts->trace_level, "Verilator trace depth (default: 3)")->default_val(3);

    tb_init->add_flag("--no-parse", opts->no_parse, "Skip verilator port parsing (useful offline)");

    tb_init->callback([opts]() { run(*opts); });
}
