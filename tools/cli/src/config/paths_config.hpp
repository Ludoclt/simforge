#pragma once

#include <string>
#include <vector>

namespace simforge::cli
{
    struct PathsConfig
    {
        std::string rtl = "rtl";
        std::vector<std::string> packages; // SV packages dirs (loaded first)
        std::string tb = "tb";
        std::string build = "build";
    };
} // namespace simforge::cli
