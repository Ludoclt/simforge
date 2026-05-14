#pragma once

#include "artifacts_config.hpp"
#include "paths_config.hpp"
#include "project_config.hpp"
#include "testbench_config.hpp"
#include "verilator_config.hpp"

#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace simforge::cli
{
    struct SimforgeConfig
    {
        ProjectConfig project;
        PathsConfig paths;
        VerilatorConfig verilator; // global defaults
        ArtifactsConfig artifacts;
        std::vector<TestbenchConfig> testbenches;

        const TestbenchConfig &get_tb(const std::string &name) const
        {
            for (const auto &tb : testbenches)
                if (tb.name == name)
                    return tb;

            throw std::runtime_error("No testbench '" + name + "' found in simforge.toml");
        }

        std::optional<std::reference_wrapper<const TestbenchConfig>> find_tb(const std::string &name) const
        {
            for (const auto &tb : testbenches)
                if (tb.name == name)
                    return std::cref(tb);
            return std::nullopt;
        }

        VerilatorConfig effective_verilator(const std::string &tb_name) const { return verilator.merged_with(get_tb(tb_name).verilator); }
    };
} // namespace simforge::cli
