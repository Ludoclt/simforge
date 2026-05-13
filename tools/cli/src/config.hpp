#pragma once

#include <toml++/toml.hpp>

namespace simforge::cli::config
{
    void init();
    void add_table(const toml::table &table);
    toml::v3::node_view<toml::v3::node> read_node(std::string path);
} // namespace simforge::cli::config
