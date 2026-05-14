#include "template_engine.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace simforge::cli::utils
{
    std::string TemplateEngine::render(std::string_view tmpl, const TemplateContext &ctx)
    {
        std::string result(tmpl);

        for (const auto &[key, value] : ctx)
        {
            const std::string token = "{{" + key + "}}";
            std::string::size_type pos = 0;

            while ((pos = result.find(token, pos)) != std::string::npos)
            {
                result.replace(pos, token.size(), value);
                pos += value.size(); // skip over the inserted value
            }
        }

        auto warn_pos = result.find("{{");
        (void)warn_pos; // could log a warning here

        return result;
    }

    void TemplateEngine::render_to_file(const std::filesystem::path &tmpl_path, const std::filesystem::path &out_path, const TemplateContext &ctx)
    {
        std::ifstream f(tmpl_path);
        if (!f)
            throw std::runtime_error("Cannot open template file: " + tmpl_path.string());

        std::ostringstream oss;
        oss << f.rdbuf();

        render_string_to_file(oss.str(), out_path, ctx);
    }

    void TemplateEngine::render_string_to_file(std::string_view tmpl, const std::filesystem::path &out_path, const TemplateContext &ctx)
    {
        if (out_path.has_parent_path())
            std::filesystem::create_directories(out_path.parent_path());

        std::ofstream out(out_path);
        if (!out)
            throw std::runtime_error("Cannot write output file: " + out_path.string());

        out << render(tmpl, ctx);
    }
} // namespace simforge::cli::utils
