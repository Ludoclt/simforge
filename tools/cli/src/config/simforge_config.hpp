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

        std::optional<std::reference_wrapper<const TestbenchConfig>> find_tb(const std::string &query) const
        {
            for (const auto &tb : testbenches)
                if (tb.subdir() == query)
                    return std::cref(tb);

            std::vector<std::reference_wrapper<const TestbenchConfig>> matches;
            for (const auto &tb : testbenches)
                if (tb.name == query)
                    matches.push_back(std::cref(tb));

            if (matches.size() == 1)
                return matches.front();

            if (matches.size() > 1)
            {
                std::string list;
                for (const auto &m : matches)
                    list += "\n  - " + m.get().subdir();
                throw std::runtime_error("Testbench name '" + query + "' is ambiguous across groups, qualify it:" + list);
            }

            return std::nullopt;
        }

        const TestbenchConfig &get_tb(const std::string &query) const
        {
            if (auto r = find_tb(query))
                return r->get();
            throw std::runtime_error("No testbench '" + query + "' found in simforge.toml");
        }

        std::vector<std::reference_wrapper<const TestbenchConfig>> in_group(const std::string &group) const
        {
            std::vector<std::reference_wrapper<const TestbenchConfig>> out;
            for (const auto &tb : testbenches)
                if (tb.group == group)
                    out.push_back(std::cref(tb));
            return out;
        }

        VerilatorConfig effective_verilator(const std::string &tb_name) const { return verilator.merged_with(get_tb(tb_name).verilator); }
    };
} // namespace simforge::cli
