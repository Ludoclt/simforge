#pragma once

#include <string>

namespace simforge::cli
{
    struct ProjectConfig
    {
        std::string name;
        std::string version = "0.1.0";
        std::string description = "";
    };
} // namespace simforge::cli
