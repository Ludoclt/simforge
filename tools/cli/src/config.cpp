#include "config.hpp"

#include <string>
#include <fstream>
#include <filesystem>
#include <stdexcept>

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
        file << std::endl
             << value << std::endl;
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
