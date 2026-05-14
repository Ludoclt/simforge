#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace simforge::cli::utils
{
    using TemplateContext = std::unordered_map<std::string, std::string>;

    class TemplateEngine
    {
      public:
        static std::string render(std::string_view tmpl, const TemplateContext &ctx);

        static void render_to_file(const std::filesystem::path &tmpl_path, const std::filesystem::path &out_path, const TemplateContext &ctx);

        static void render_string_to_file(std::string_view tmpl, const std::filesystem::path &out_path, const TemplateContext &ctx);
    };
} // namespace simforge::cli::utils
