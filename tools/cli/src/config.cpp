#include "config.hpp"

#include <string>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <format>
#include <sstream>

using namespace simforge::cli;

namespace
{
    constexpr std::string filename = "simforge.toml";

    template <typename T>
    void write(T &value)
    {
        std::ofstream file(filename);
        file << value << std::endl;
        file.close();
    }

    template <typename T>
    void append(T &value)
    {
        std::ofstream file(filename, std::ios::app);
        file << value << std::endl
             << std::endl;
        file.close();
    }
}

void config::init()
{
    if (std::filesystem::exists(filename))
        throw std::runtime_error("A project already exists in this directory");

    std::ofstream file(filename);
    file.close();
}

void config::add_table(const toml::table &table)
{
    append(table);
}

toml::v3::node_view<toml::v3::node> config::read_node(std::string path)
{
    toml::table tbl;
    try
    {
        tbl = toml::parse_file("simforge.toml");
    }
    catch (const toml::parse_error &err)
    {
        std::ostringstream oss;
        oss << "Error parsing file " << err.source().path << ":\n"
            << err.description() << "\n"
            << err.source().begin;
        throw std::runtime_error(oss.str());
    }

    return tbl.at_path(path);
}
