#pragma once

#include "verilator_config.hpp"

#include <optional>
#include <string>

namespace simforge::cli
{
    enum class TbStyle
    {
        UVM,
        Basic,
    };

    struct TestbenchConfig
    {
        std::string name;
        std::string top;
        std::string group;
        TbStyle style = TbStyle::UVM;

        // if empty, infers from [paths] + name
        std::string sv_top;
        std::string cpp;

        // Verilator overrides for this specific TB
        VerilatorConfig verilator;

        std::string subdir() const { return group.empty() ? name : group + "/" + name; }

        std::string resolved_sv_top(const std::string &tb_root) const
        {
            if (!sv_top.empty())
                return sv_top;

            return tb_root + "/" + subdir() + "/sv/" + top + ".sv";
        }

        std::string resolved_cpp(const std::string &tb_root) const
        {
            if (!cpp.empty())
                return cpp;
            return tb_root + "/" + subdir() + "/" + name + "_tb.cpp";
        }

        // Verilated class prefix
        std::string dut_class() const { return "V" + top; }
    };
} // namespace simforge::cli
