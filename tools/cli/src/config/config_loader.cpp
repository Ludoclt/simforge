#include "config_loader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <toml++/toml.hpp>

namespace simforge::cli
{
    // helpers
    namespace
    {
        std::vector<std::string> to_string_vec(const toml::array *arr)
        {
            std::vector<std::string> out;
            if (!arr)
                return out;
            for (const auto &v : *arr)
                if (auto s = v.value<std::string>())
                    out.push_back(*s);
            return out;
        }

        // Read a [verilator] table (global or per-TB) into a VerilatorConfig
        VerilatorConfig parse_verilator(const toml::table *t)
        {
            VerilatorConfig cfg;
            if (!t)
                return cfg;

            cfg.args = to_string_vec(t->get_as<toml::array>("args"));
            cfg.include_dirs = to_string_vec(t->get_as<toml::array>("include_dirs"));
            cfg.jobs = t->at_path("jobs").value_or(0);
            cfg.trace_level = t->at_path("trace_level").value_or(0);
            return cfg;
        }
    } // anonymous namespace

    // TbStyle
    TbStyle ConfigLoader::parse_style(std::string_view s)
    {
        std::string lower(s);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "uvm")
            return TbStyle::UVM;
        if (lower == "basic")
            return TbStyle::Basic;
        throw std::runtime_error("Unknown testbench style '" + std::string(s) + "' (expected 'uvm' or 'basic')");
    }

    std::string ConfigLoader::style_to_string(TbStyle s)
    {
        switch (s)
        {
        case TbStyle::UVM:
            return "uvm";
        case TbStyle::Basic:
            return "basic";
        }
        return "uvm";
    }

    SimforgeConfig ConfigLoader::load(const std::filesystem::path &dir)
    {
        const auto path = dir / kConfigFilename;
        if (!std::filesystem::exists(path))
            throw std::runtime_error("No simforge.toml found in " + dir.string() + "\nRun 'simforge init' to create one.");

        toml::table tbl;
        try
        {
            tbl = toml::parse_file(path.string());
        }
        catch (const toml::parse_error &err)
        {
            std::ostringstream oss;
            oss << "Error parsing " << path << ":\n"
                << err.description() << "\n"
                << "  at line " << err.source().begin.line << ", column " << err.source().begin.column;
            throw std::runtime_error(oss.str());
        }

        SimforgeConfig cfg;

        // [project]
        if (const auto *p = tbl.get_as<toml::table>("project"))
        {
            cfg.project.name = p->at_path("name").value_or(cfg.project.name);
            cfg.project.version = p->at_path("version").value_or(cfg.project.version);
            cfg.project.description = p->at_path("description").value_or(cfg.project.description);
        }

        // [paths]
        if (const auto *p = tbl.get_as<toml::table>("paths"))
        {
            cfg.paths.rtl = p->at_path("rtl").value_or(cfg.paths.rtl);
            cfg.paths.tb = p->at_path("tb").value_or(cfg.paths.tb);
            cfg.paths.build = p->at_path("build").value_or(cfg.paths.build);
            cfg.paths.packages = to_string_vec(p->get_as<toml::array>("packages"));
        }

        // [verilator]
        cfg.verilator = parse_verilator(tbl.get_as<toml::table>("verilator"));

        // [artifacts]
        if (const auto *p = tbl.get_as<toml::table>("artifacts"))
        {
            cfg.artifacts.vcd = p->at_path("vcd").value_or(cfg.artifacts.vcd);
            cfg.artifacts.fst = p->at_path("fst").value_or(cfg.artifacts.fst);
            cfg.artifacts.log = p->at_path("log").value_or(cfg.artifacts.log);
        }

        // [[testbenches]]
        if (const auto *arr = tbl.get_as<toml::array>("testbenches"))
        {
            for (const auto &elem : *arr)
            {
                const auto *t = elem.as_table();
                if (!t)
                    continue;

                TestbenchConfig tb;

                if (!t->get_as<std::string>("name"))
                    throw std::runtime_error("Each [[testbenches]] entry must have a 'name' field.");
                tb.name = **t->get_as<std::string>("name");

                if (!t->get_as<std::string>("top"))
                    throw std::runtime_error("Testbench '" + tb.name + "' is missing required 'top' field.");
                tb.top = **t->get_as<std::string>("top");

                if (const auto *g = t->get_as<std::string>("group"))
                    tb.group = **g;

                if (const auto *sty = t->get_as<std::string>("style"))
                    tb.style = parse_style(**sty);

                if (const auto *s = t->get_as<std::string>("sv_top"))
                    tb.sv_top = **s;
                if (const auto *s = t->get_as<std::string>("cpp"))
                    tb.cpp = **s;

                tb.verilator = parse_verilator(t->get_as<toml::table>("verilator"));

                cfg.testbenches.push_back(std::move(tb));
            }
        }

        // guard against stale duplicate entries
        for (size_t i = 0; i < cfg.testbenches.size(); ++i)
            for (size_t j = i + 1; j < cfg.testbenches.size(); ++j)
                if (cfg.testbenches[i].subdir() == cfg.testbenches[j].subdir())
                    throw std::runtime_error(
                        "simforge.toml has duplicate [[testbenches]] entries for '" + cfg.testbenches[i].subdir() +
                        "' - remove one of them by hand (this can happen from repeated 'simforge tb init' runs "
                        "on older simforge versions)."
                    );

        return cfg;
    }

    void ConfigLoader::write_header(std::ostream &f, const SimforgeConfig &cfg)
    {
        f << "[project]\n"
          << "name        = \"" << cfg.project.name << "\"\n"
          << "version     = \"" << cfg.project.version << "\"\n"
          << "description = \"" << cfg.project.description << "\"\n"
          << "\n"
          << "[paths]\n"
          << "rtl   = \"" << cfg.paths.rtl << "\"\n"
          << "tb    = \"" << cfg.paths.tb << "\"\n"
          << "build = \"" << cfg.paths.build << "\"\n";

        if (!cfg.paths.packages.empty())
        {
            f << "packages = [";
            for (size_t i = 0; i < cfg.paths.packages.size(); ++i)
            {
                if (i)
                    f << ", ";
                f << "\"" << cfg.paths.packages[i] << "\"";
            }
            f << "]\n";
        }

        f << "\n"
          << "[verilator]\n";
        if (!cfg.verilator.args.empty())
        {
            f << "args  = [";
            for (size_t i = 0; i < cfg.verilator.args.size(); ++i)
            {
                if (i)
                    f << ", ";
                f << "\"" << cfg.verilator.args[i] << "\"";
            }
            f << "]\n";
        }
        else
        {
            f << "args  = [\"--sv\"]\n";
        }
        f << "jobs  = " << cfg.verilator.jobs << "\n"
          << "\n"
          << "[artifacts]\n"
          << "vcd = " << (cfg.artifacts.vcd ? "true" : "false") << "\n"
          << "fst = " << (cfg.artifacts.fst ? "true" : "false") << "\n"
          << "log = " << (cfg.artifacts.log ? "true" : "false") << "\n";
    }

    void ConfigLoader::write_tb_block(std::ostream &f, const TestbenchConfig &tb)
    {
        f << "\n[[testbenches]]\n"
          << "name  = \"" << tb.name << "\"\n"
          << "top   = \"" << tb.top << "\"\n";

        if (!tb.group.empty())
            f << "group = \"" << tb.group << "\"\n";

        f << "style = \"" << style_to_string(tb.style) << "\"\n";

        if (!tb.sv_top.empty())
            f << "sv_top = \"" << tb.sv_top << "\"\n";
        if (!tb.cpp.empty())
            f << "cpp    = \"" << tb.cpp << "\"\n";

        if (tb.verilator.trace_level != 0 || !tb.verilator.args.empty() || !tb.verilator.include_dirs.empty())
        {
            f << "\n  [testbenches.verilator]\n"
              << "  trace_level = " << tb.verilator.trace_level << "\n";

            if (!tb.verilator.args.empty())
            {
                f << "  args = [";
                for (size_t i = 0; i < tb.verilator.args.size(); ++i)
                {
                    if (i)
                        f << ", ";
                    f << "\"" << tb.verilator.args[i] << "\"";
                }
                f << "]\n";
            }

            if (!tb.verilator.include_dirs.empty())
            {
                f << "  include_dirs = [";
                for (size_t i = 0; i < tb.verilator.include_dirs.size(); ++i)
                {
                    if (i)
                        f << ", ";
                    f << "\"" << tb.verilator.include_dirs[i] << "\"";
                }
                f << "]\n";
            }
        }
    }

    void ConfigLoader::write_default(const SimforgeConfig &cfg, const std::filesystem::path &dir, bool force)
    {
        const auto path = dir / kConfigFilename;

        if (std::filesystem::exists(path) && !force)
            throw std::runtime_error("simforge.toml already exists in " + dir.string() + "\nUse --force to overwrite.");

        std::ofstream f(path);
        if (!f)
            throw std::runtime_error("Cannot write " + path.string());

        write_header(f, cfg);
        for (const auto &tb : cfg.testbenches)
            write_tb_block(f, tb);
    }

    void ConfigLoader::save(const SimforgeConfig &cfg, const std::filesystem::path &dir)
    {
        const auto path = dir / kConfigFilename;

        std::ofstream f(path, std::ios::trunc);
        if (!f)
            throw std::runtime_error("Cannot write " + path.string());

        write_header(f, cfg);
        for (const auto &tb : cfg.testbenches)
            write_tb_block(f, tb);
    }

    void ConfigLoader::upsert_testbench(const TestbenchConfig &tb, const std::filesystem::path &dir, bool force)
    {
        const auto path = dir / kConfigFilename;
        if (!std::filesystem::exists(path))
            throw std::runtime_error("No simforge.toml in " + dir.string() + "\nRun 'simforge init' first.");

        SimforgeConfig cfg = load(dir);

        auto it = std::find_if(cfg.testbenches.begin(), cfg.testbenches.end(), [&](const TestbenchConfig &existing) { return existing.subdir() == tb.subdir(); });

        if (it != cfg.testbenches.end())
        {
            if (!force)
                throw std::runtime_error(
                    "Testbench '" + tb.subdir() +
                    "' is already registered in simforge.toml.\n"
                    "Use --force to regenerate it (this overwrites its generated files), or pick a different --name/--group."
                );
            *it = tb;
        }
        else
        {
            cfg.testbenches.push_back(tb);
        }

        save(cfg, dir);
    }
} // namespace simforge::cli
