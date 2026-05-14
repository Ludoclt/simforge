#pragma once

#include <string>
#include <vector>

namespace simforge::cli
{
    struct VerilatorConfig
    {
        std::vector<std::string> args;
        std::vector<std::string> include_dirs;
        int jobs = 0;
        int trace_level = 0;

        VerilatorConfig merged_with(const VerilatorConfig &local) const
        {
            VerilatorConfig out = *this;

            if (!local.args.empty())
                out.args = local.args;

            if (!local.include_dirs.empty())
                out.include_dirs = local.include_dirs;

            if (local.jobs != 0)
                out.jobs = local.jobs;

            if (local.trace_level != 0)
                out.trace_level = local.trace_level;

            return out;
        }
    };
} // namespace simforge::cli
