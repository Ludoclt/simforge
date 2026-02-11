#pragma once

#include <toml++/toml.hpp>

namespace simforge::cli::config
{
    void init();
    void add_table(const toml::table &table);
} // namespace simforge::cli::config
