#include "sv_parser.hpp"

#include "nlohmann/json.hpp"
#include "process.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace simforge::cli::utils
{
    std::string SvPort::cpp_type() const
    {
        if (width <= 8)
            return "vluint8_t";
        if (width <= 16)
            return "vluint16_t";
        if (width <= 32)
            return "vluint32_t";
        if (width <= 64)
            return "vluint64_t";
        int words = (width + 31) / 32;
        return "VlWide<" + std::to_string(words) + ">";
    }

    std::vector<SvPort> SvModule::inputs() const
    {
        std::vector<SvPort> out;
        for (const auto &p : ports)
            if (p.is_input())
                out.push_back(p);
        return out;
    }

    std::vector<SvPort> SvModule::outputs() const
    {
        std::vector<SvPort> out;
        for (const auto &p : ports)
            if (p.is_output())
                out.push_back(p);
        return out;
    }

    static bool matches_any(const std::string &name, std::initializer_list<const char *> patterns)
    {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        for (const char *p : patterns)
            if (lower.find(p) != std::string::npos)
                return true;
        return false;
    }

    std::string SvModule::guess_clk_signal() const
    {
        for (const auto &p : ports)
            if (p.is_input() && p.width == 1 && matches_any(p.name, {"clk", "clock", "ck"}))
                return p.name;
        return {};
    }

    std::string SvModule::guess_rst_signal() const
    {
        for (const auto &p : ports)
            if (p.is_input() && p.width == 1 && matches_any(p.name, {"rst", "reset", "rstn", "rst_n", "resetn"}))
                return p.name;
        return {};
    }

    namespace
    {
        using json = nlohmann::json;

        void buildAddrMap(const json &node, std::unordered_map<std::string, json> &map)
        {
            if (!node.is_object())
                return;
            if (node.contains("addr") && node["addr"].is_string())
            {
                map[node["addr"].get<std::string>()] = node;
            }
            for (auto &[key, val] : node.items())
            {
                if (val.is_array())
                {
                    for (auto &child : val)
                        buildAddrMap(child, map);
                }
                else if (val.is_object())
                {
                    buildAddrMap(val, map);
                }
            }
        }

        int parseRangeWidth(const std::string &range)
        {
            auto colon = range.find(':');
            if (colon == std::string::npos)
                return 1;
            int hi = std::stoi(range.substr(0, colon));
            int lo = std::stoi(range.substr(colon + 1));
            return std::abs(hi - lo) + 1;
        }

        int resolveWidth(const std::string &addr, const std::unordered_map<std::string, json> &addrMap)
        {
            auto it = addrMap.find(addr);
            if (it == addrMap.end())
                return 1;
            const json &dtype = it->second;
            const std::string type = dtype.value("type", "");

            if (type == "BASICDTYPE")
            {
                if (dtype.contains("range") && dtype["range"].is_string())
                    return parseRangeWidth(dtype["range"].get<std::string>());
                return 1;
            }
            if (type == "ENUMDTYPE" || type == "REFDTYPE")
            {
                if (dtype.contains("refDTypep") && dtype["refDTypep"].is_string())
                    return resolveWidth(dtype["refDTypep"].get<std::string>(), addrMap);
                return 1;
            }
            if (type == "STRUCTDTYPE")
            {
                int total = 0;
                for (auto &member : dtype.value("membersp", json::array()))
                    total += resolveWidth(member.value("refDTypep", ""), addrMap);
                return total > 0 ? total : 1;
            }
            return 1;
        }

        SvModule parse_json(const std::filesystem::path &json_path, const std::string &top_name)
        {
            SvModule mod;
            mod.name = top_name;
            mod.json_path = json_path.string();

            std::ifstream file(json_path);

            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open file: " + json_path.string());
            }

            json netlist = json::parse(file);

            std::unordered_map<std::string, json> addrMap;
            buildAddrMap(netlist, addrMap);

            for (const json &j_mod : netlist["modulesp"])
            {
                if (j_mod.value("type", "") != "MODULE" || j_mod.value("name", "") != top_name)
                    continue;

                for (const json &stmt : j_mod["stmtsp"])
                {
                    if (stmt.value("type", "") != "VAR" || !stmt.value("isPrimaryIO", false))
                        continue;

                    SvPort port;
                    port.name = stmt["name"].get<std::string>();
                    port.raw_type = stmt.value("dtypeName", "");
                    port.width = resolveWidth(stmt.value("dtypep", ""), addrMap);
                    port.is_packed = (port.width > 1);

                    std::string dir = stmt.value("direction", "");
                    if (dir == "INPUT")
                        port.dir = PortDir::Input;
                    else if (dir == "OUTPUT")
                        port.dir = PortDir::Output;
                    else if (dir == "INOUT")
                        port.dir = PortDir::InOut;
                    else
                        port.dir = PortDir::Unknown;

                    mod.ports.push_back(port);
                }
            }

            return mod;
        }
    } // anonymous namespace

    namespace
    {
        // Strips // and /* */ comments
        std::string strip_comments(const std::string &src)
        {
            std::string out;
            out.reserve(src.size());
            for (size_t i = 0; i < src.size(); ++i)
            {
                if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/')
                {
                    while (i < src.size() && src[i] != '\n')
                        ++i;
                    out += '\n';
                    continue;
                }
                if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*')
                {
                    i += 2;
                    while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/'))
                        ++i;
                    ++i;
                    out += ' ';
                    continue;
                }
                out += src[i];
            }
            return out;
        }

        std::string read_file(const std::filesystem::path &path)
        {
            std::ifstream f(path);
            if (!f)
                throw std::runtime_error("Cannot open " + path.string());
            std::ostringstream ss;
            ss << f.rdbuf();
            return strip_comments(ss.str());
        }

        std::string trim(const std::string &s)
        {
            size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos)
                return "";
            size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        std::string extract_parens(const std::string &text, size_t from, size_t *end_pos = nullptr)
        {
            size_t open = text.find('(', from);
            if (open == std::string::npos)
                return {};
            int depth = 0;
            size_t i = open;
            for (; i < text.size(); ++i)
            {
                if (text[i] == '(')
                    ++depth;
                else if (text[i] == ')')
                {
                    --depth;
                    if (depth == 0)
                        break;
                }
            }
            if (depth != 0)
                return {};
            if (end_pos)
                *end_pos = i;
            return text.substr(open + 1, i - open - 1);
        }

        std::vector<std::string> split_top_level(const std::string &s, char sep = ',')
        {
            std::vector<std::string> out;
            int depth = 0;
            std::string cur;
            for (char c : s)
            {
                if (c == '[' || c == '(')
                    ++depth;
                else if (c == ']' || c == ')')
                    --depth;

                if (c == sep && depth == 0)
                {
                    out.push_back(trim(cur));
                    cur.clear();
                }
                else
                {
                    cur += c;
                }
            }
            if (!trim(cur).empty())
                out.push_back(trim(cur));
            return out;
        }

        PortDir dir_from_keyword(const std::string &kw)
        {
            if (kw == "input")
                return PortDir::Input;
            if (kw == "output")
                return PortDir::Output;
            if (kw == "inout")
                return PortDir::InOut;
            return PortDir::Unknown;
        }

        bool is_basic_type_keyword(const std::string &tok)
        {
            static const std::vector<std::string> basics = {"logic", "wire", "reg", "bit", "integer", "byte", "signed", "unsigned"};
            return std::find(basics.begin(), basics.end(), tok) != basics.end();
        }

        // Minimal recursive-descent evaluator for the arithmetic SystemVerilog uses in parameter
        class ExprEval
        {
          public:
            ExprEval(const std::string &s, const std::unordered_map<std::string, long> &params) : s_(s), params_(params) {}

            long run()
            {
                long v = parse_expr();
                skip_ws();
                if (pos_ != s_.size())
                    throw std::runtime_error("trailing characters in expression: " + s_);
                return v;
            }

          private:
            const std::string &s_;
            const std::unordered_map<std::string, long> &params_;
            size_t pos_ = 0;

            void skip_ws()
            {
                while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_])))
                    ++pos_;
            }

            char peek()
            {
                skip_ws();
                return pos_ < s_.size() ? s_[pos_] : '\0';
            }

            long parse_expr() // + -
            {
                long v = parse_term();
                for (;;)
                {
                    char c = peek();
                    if (c == '+')
                    {
                        ++pos_;
                        v += parse_term();
                    }
                    else if (c == '-')
                    {
                        ++pos_;
                        v -= parse_term();
                    }
                    else
                        break;
                }
                return v;
            }

            long parse_term() // * /
            {
                long v = parse_unary();
                for (;;)
                {
                    char c = peek();
                    if (c == '*')
                    {
                        ++pos_;
                        v *= parse_unary();
                    }
                    else if (c == '/')
                    {
                        ++pos_;
                        long d = parse_unary();
                        if (d == 0)
                            throw std::runtime_error("division by zero in expression: " + s_);
                        v /= d;
                    }
                    else
                        break;
                }
                return v;
            }

            long parse_unary()
            {
                if (peek() == '-')
                {
                    ++pos_;
                    return -parse_unary();
                }
                if (peek() == '+')
                {
                    ++pos_;
                    return parse_unary();
                }
                return parse_primary();
            }

            long parse_primary()
            {
                skip_ws();
                if (peek() == '(')
                {
                    ++pos_;
                    long v = parse_expr();
                    if (peek() != ')')
                        throw std::runtime_error("unbalanced parens in expression: " + s_);
                    ++pos_;
                    return v;
                }

                size_t start = pos_;
                if (std::isdigit(static_cast<unsigned char>(peek())))
                {
                    while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_])))
                        ++pos_;

                    // Based literal: <size>'<base><digits>, e.g. 8'd10, 4'hF, 1'b1.
                    if (pos_ < s_.size() && s_[pos_] == '\'')
                    {
                        ++pos_; // consume '
                        if (pos_ >= s_.size())
                            throw std::runtime_error("truncated based literal: " + s_);
                        char base = static_cast<char>(std::tolower(static_cast<unsigned char>(s_[pos_])));
                        ++pos_;
                        size_t digits_start = pos_;
                        while (pos_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '_'))
                            ++pos_;
                        std::string digits = s_.substr(digits_start, pos_ - digits_start);
                        digits.erase(std::remove(digits.begin(), digits.end(), '_'), digits.end());
                        int radix = (base == 'b') ? 2 : (base == 'o') ? 8 : (base == 'h') ? 16 : 10;
                        return digits.empty() ? 0 : std::stol(digits, nullptr, radix);
                    }

                    return std::stol(s_.substr(start, pos_ - start));
                }

                if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_')
                {
                    while (pos_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '_'))
                        ++pos_;
                    std::string ident = s_.substr(start, pos_ - start);
                    auto it = params_.find(ident);
                    if (it == params_.end())
                        throw std::runtime_error("unresolved identifier '" + ident + "' in expression: " + s_);
                    return it->second;
                }

                throw std::runtime_error("unexpected character in expression: " + s_);
            }
        };

        long eval_expr(const std::string &expr, const std::unordered_map<std::string, long> &params)
        {
            return ExprEval(expr, params).run();
        }

        int width_from_range_tokens(const std::vector<std::string> &tokens, const std::unordered_map<std::string, long> &params = {})
        {
            static const std::regex range_re(R"(\[\s*([^\]:]+)\s*:\s*([^\]]+)\s*\])");
            for (const auto &t : tokens)
            {
                std::smatch m;
                if (std::regex_search(t, m, range_re))
                {
                    try
                    {
                        long hi = eval_expr(trim(m[1].str()), params);
                        long lo = eval_expr(trim(m[2].str()), params);
                        return static_cast<int>(std::abs(hi - lo) + 1);
                    }
                    catch (const std::exception &)
                    {
                        return 0; // unresolved - let the caller decide the default
                    }
                }
            }
            return 0;
        }

        std::vector<SvParam> parse_module_params(const std::string &param_body)
        {
            std::vector<SvParam> out;
            std::unordered_map<std::string, long> known;

            for (const auto &entry : split_top_level(param_body))
            {
                auto eq = entry.find('=');
                std::string lhs = trim(eq == std::string::npos ? entry : entry.substr(0, eq));
                std::string rhs = eq == std::string::npos ? "" : trim(entry.substr(eq + 1));

                std::istringstream iss(lhs);
                std::string tok, name;
                while (iss >> tok)
                    name = tok;
                if (name.empty())
                    continue;

                SvParam p;
                p.name = name;
                p.raw_default = rhs;
                if (!rhs.empty())
                {
                    try
                    {
                        p.value = eval_expr(rhs, known);
                        p.resolved = true;
                        known[name] = p.value;
                    }
                    catch (const std::exception &)
                    {
                        // leave unresolved
                    }
                }
                out.push_back(p);
            }
            return out;
        }

        int probe_type_width(
            const std::string &type_name,
            const std::vector<std::filesystem::path> &pkg_dirs,
            const std::vector<std::filesystem::path> &include_dirs,
            const std::vector<std::string> &extra_args
        )
        {
            try
            {
                const std::string probe_top = "sf_wprobe_" + type_name + "_probe";
                auto work_dir = std::filesystem::temp_directory_path() / ("simforge_wprobe_" + type_name);
                std::filesystem::create_directories(work_dir);

                // imports
                std::vector<std::string> pkg_names;
                std::regex pkg_re(R"(\bpackage\s+(\w+)\s*;)");
                for (const auto &dir : pkg_dirs)
                {
                    if (!std::filesystem::exists(dir))
                        continue;
                    for (const auto &entry : std::filesystem::directory_iterator(dir))
                    {
                        if (!entry.is_regular_file() || entry.path().extension() != ".sv")
                            continue;
                        std::string body = read_file(entry.path());
                        for (auto it = std::sregex_iterator(body.begin(), body.end(), pkg_re); it != std::sregex_iterator(); ++it)
                            pkg_names.push_back((*it)[1].str());
                    }
                }

                auto probe_file = work_dir / "probe.sv";
                {
                    std::ofstream f(probe_file);
                    for (const auto &pn : pkg_names)
                        f << "import " << pn << "::*;\n";
                    f << "module " << probe_top << " (output " << type_name << " sf_probe_sig);\nendmodule\n";
                }

                SvModule probe_mod = parse_sv_module(probe_file, probe_top, pkg_dirs, include_dirs, extra_args, work_dir);
                if (!probe_mod.ports.empty())
                    return probe_mod.ports.front().width;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Warning: could not resolve width of type '" << type_name << "': " << e.what() << " (defaulting to 1 bit, fix manually if wrong)\n";
            }
            return 1;
        }

        std::filesystem::path find_interface_file(const std::string &iface_type, const std::vector<std::filesystem::path> &search_dirs)
        {
            std::regex decl_re(R"(\binterface\s+)" + iface_type + R"(\b)");
            for (const auto &dir : search_dirs)
            {
                if (!std::filesystem::exists(dir))
                    continue;
                for (const auto &entry : std::filesystem::recursive_directory_iterator(dir))
                {
                    if (!entry.is_regular_file() || entry.path().extension() != ".sv")
                        continue;
                    std::ifstream f(entry.path());
                    std::ostringstream ss;
                    ss << f.rdbuf();
                    if (std::regex_search(ss.str(), decl_re))
                        return entry.path();
                }
            }
            return {};
        }

        std::vector<SvIfaceMember> parse_modport_members(
            const std::string &iface_body,
            const std::string &modport_name,
            const std::vector<std::filesystem::path> &pkg_dirs,
            const std::vector<std::filesystem::path> &include_dirs,
            const std::vector<std::string> &extra_args
        )
        {
            std::vector<SvIfaceMember> members;

            std::regex modport_kw_re(R"(\bmodport\s+)" + modport_name + R"(\s*)");
            std::smatch m;
            if (!std::regex_search(iface_body, m, modport_kw_re))
                return members; // modport not found, caller warns

            std::string list_body = extract_parens(iface_body, static_cast<size_t>(m.position(0)) + m.length(0));

            std::string cur_dir = "input"; // SV requires an explicit direction before first use anyway
            for (const auto &entry : split_top_level(list_body))
            {
                std::istringstream iss(entry);
                std::string first;
                iss >> first;
                if (first.empty())
                    continue;

                std::string name;
                if (first == "input" || first == "output" || first == "inout")
                {
                    cur_dir = first;
                    if (!(iss >> name))
                        continue; // malformed, skip
                }
                else
                {
                    name = first; // bare name continuing the previous direction
                }

                SvIfaceMember mem;
                mem.name = name;
                mem.dir = dir_from_keyword(cur_dir);

                std::regex decl_re(R"((\w+)\s*(\[\s*\d+\s*:\s*\d+\s*\])?\s*)" + name + R"(\s*;)");
                std::smatch dm;
                if (std::regex_search(iface_body, dm, decl_re))
                {
                    std::string type_tok = dm[1].str();
                    mem.raw_type = type_tok + (dm[2].matched ? " " + dm[2].str() : "");
                    if (is_basic_type_keyword(type_tok))
                        mem.width = dm[2].matched ? width_from_range_tokens({dm[2].str()}) : 1;
                    else
                        mem.width = probe_type_width(type_tok, pkg_dirs, include_dirs, extra_args);
                }
                else
                {
                    mem.width = 1;
                    mem.is_header_port = true;
                }

                members.push_back(mem);
            }

            return members;
        }

        std::string extract_braces(const std::string &text, size_t from, size_t *end_pos = nullptr)
        {
            size_t open = text.find('{', from);
            if (open == std::string::npos)
                return {};
            int depth = 0;
            size_t i = open;
            for (; i < text.size(); ++i)
            {
                if (text[i] == '{')
                    ++depth;
                else if (text[i] == '}')
                {
                    --depth;
                    if (depth == 0)
                        break;
                }
            }
            if (depth != 0)
                return {};
            if (end_pos)
                *end_pos = i;
            return text.substr(open + 1, i - open - 1);
        }
    } // anonymous namespace

    std::vector<EnumInfo> parse_package_enums(const std::vector<std::filesystem::path> &pkg_dirs)
    {
        std::vector<EnumInfo> out;
        std::regex enum_re(R"(\btypedef\s+enum\b)");

        for (const auto &dir : pkg_dirs)
        {
            if (!std::filesystem::exists(dir))
                continue;

            for (const auto &entry : std::filesystem::directory_iterator(dir))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".sv")
                    continue;

                std::string body = read_file(entry.path());

                for (auto it = std::sregex_iterator(body.begin(), body.end(), enum_re); it != std::sregex_iterator(); ++it)
                {
                    size_t after_kw = static_cast<size_t>(it->position(0)) + it->length(0);
                    size_t brace_end = 0;
                    std::string list_body = extract_braces(body, after_kw, &brace_end);
                    if (list_body.empty())
                        continue;

                    size_t semi = body.find(';', brace_end);
                    if (semi == std::string::npos)
                        continue;

                    std::string type_name = trim(body.substr(brace_end + 1, semi - brace_end - 1));
                    if (type_name.empty() || type_name.find_first_of(" \t\n") != std::string::npos)
                        continue; // not a simple "} name;" tail, skip rather than guess

                    EnumInfo info;
                    info.type_name = type_name;
                    for (const auto &item : split_top_level(list_body))
                    {
                        auto eq = item.find('=');
                        std::string name = trim(eq == std::string::npos ? item : item.substr(0, eq));
                        if (!name.empty())
                            info.values.push_back(name);
                    }
                    if (!info.values.empty())
                        out.push_back(std::move(info));
                }
            }
        }
        return out;
    }

    void annotate_enum_values(SvModule &mod, const std::vector<std::filesystem::path> &pkg_dirs)
    {
        auto enums = parse_package_enums(pkg_dirs);
        if (enums.empty())
            return;

        auto find_values = [&](const std::string &raw_type) -> const std::vector<std::string> *
        {
            std::istringstream iss(raw_type);
            std::string first_tok;
            iss >> first_tok;
            for (const auto &e : enums)
                if (e.type_name == first_tok)
                    return &e.values;
            return nullptr;
        };

        for (auto &p : mod.ports)
            if (const auto *vals = find_values(p.raw_type))
                p.enum_values = *vals;

        for (auto &ip : mod.iface_ports)
            for (auto &m : ip.members)
                if (const auto *vals = find_values(m.raw_type))
                    m.enum_values = *vals;
    }

    std::vector<StructFieldInfo> parse_struct_fields(const std::string &brace_body)
    {
        std::vector<StructFieldInfo> fields;
        static const std::regex field_re(R"(^(\w+)\s*(\[\s*\d+\s*:\s*\d+\s*\])?\s*(.+)$)");
        static const std::regex range_re(R"(\[\s*(\d+)\s*:\s*(\d+)\s*\])");

        for (const auto &decl : split_top_level(brace_body, ';'))
        {
            std::string d = trim(decl);
            if (d.empty())
                continue;

            std::smatch m;
            if (!std::regex_match(d, m, field_re))
                continue;

            std::string type_tok = m[1].str();
            std::string range = m[2].str();
            std::string names_csv = trim(m[3].str());

            int width = 1;
            std::smatch rm;
            if (!range.empty() && std::regex_search(range, rm, range_re))
                width = std::abs(std::stoi(rm[1].str()) - std::stoi(rm[2].str())) + 1;

            for (const auto &raw_name : split_top_level(names_csv, ','))
            {
                std::string n = trim(raw_name);
                if (!n.empty())
                    fields.push_back({n, width, type_tok});
            }
        }
        return fields;
    }

    std::vector<StructInfo> parse_package_structs(const std::vector<std::filesystem::path> &pkg_dirs)
    {
        std::vector<StructInfo> out;
        std::regex struct_re(R"(\btypedef\s+struct\s+(?:packed\s+)?)");

        for (const auto &dir : pkg_dirs)
        {
            if (!std::filesystem::exists(dir))
                continue;

            for (const auto &entry : std::filesystem::directory_iterator(dir))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".sv")
                    continue;

                std::string body = read_file(entry.path());

                for (auto it = std::sregex_iterator(body.begin(), body.end(), struct_re); it != std::sregex_iterator(); ++it)
                {
                    size_t after_kw = static_cast<size_t>(it->position(0)) + it->length(0);
                    size_t brace_end = 0;
                    std::string brace_body = extract_braces(body, after_kw, &brace_end);
                    if (brace_body.empty())
                        continue;

                    size_t semi = body.find(';', brace_end);
                    if (semi == std::string::npos)
                        continue;

                    std::string type_name = trim(body.substr(brace_end + 1, semi - brace_end - 1));
                    if (type_name.empty() || type_name.find_first_of(" \t\n") != std::string::npos)
                        continue; // skip rather than guess

                    StructInfo info;
                    info.type_name = type_name;
                    info.fields = parse_struct_fields(brace_body);
                    if (!info.fields.empty())
                        out.push_back(std::move(info));
                }
            }
        }
        return out;
    }

    void annotate_struct_fields(SvModule &mod, const std::vector<std::filesystem::path> &pkg_dirs)
    {
        auto structs = parse_package_structs(pkg_dirs);
        if (structs.empty())
            return;

        auto find_fields = [&](const std::string &raw_type) -> const std::vector<StructFieldInfo> *
        {
            std::istringstream iss(raw_type);
            std::string first_tok;
            iss >> first_tok;
            for (const auto &s : structs)
                if (s.type_name == first_tok)
                    return &s.fields;
            return nullptr;
        };

        for (auto &p : mod.ports)
            if (const auto *fields = find_fields(p.raw_type))
                p.struct_fields = *fields;

        for (auto &ip : mod.iface_ports)
            for (auto &m : ip.members)
                if (const auto *fields = find_fields(m.raw_type))
                    m.struct_fields = *fields;
    }

    SvTextScanResult scan_module_text(
        const std::filesystem::path &sv_file,
        const std::string &top_module,
        const std::vector<std::filesystem::path> &search_dirs,
        const std::vector<std::filesystem::path> &pkg_dirs,
        const std::vector<std::filesystem::path> &include_dirs,
        const std::vector<std::string> &extra_args
    )
    {
        SvTextScanResult result;

        std::string src = read_file(sv_file);

        std::regex mod_re(R"(\bmodule\s+)" + top_module + R"(\b)");
        std::smatch mm;
        if (!std::regex_search(src, mm, mod_re))
            throw std::runtime_error("Could not find 'module " + top_module + "' in " + sv_file.string());

        size_t after_name = static_cast<size_t>(mm.position(0)) + mm.length(0);
        size_t scan_pos = after_name;
        while (scan_pos < src.size() && std::isspace(static_cast<unsigned char>(src[scan_pos])))
            ++scan_pos;

        std::unordered_map<std::string, long> param_values;
        if (scan_pos < src.size() && src[scan_pos] == '#')
        {
            size_t param_end = 0;
            std::string param_body = extract_parens(src, scan_pos, &param_end);
            result.params = parse_module_params(param_body);
            for (const auto &p : result.params)
                if (p.resolved)
                    param_values[p.name] = p.value;
            after_name = param_end + 1;
        }

        std::string portlist = extract_parens(src, after_name);

        for (const auto &entry : split_top_level(portlist))
        {
            std::istringstream iss(entry);
            std::string first;
            iss >> first;
            if (first.empty())
                continue;

            if (first == "input" || first == "output" || first == "inout")
            {
                std::vector<std::string> tokens;
                std::string tok;
                while (iss >> tok)
                    tokens.push_back(tok);
                if (tokens.empty())
                    continue; // malformed

                SvPort port;
                port.name = tokens.back();
                port.dir = dir_from_keyword(first);
                tokens.pop_back();

                int range_width = width_from_range_tokens(tokens, param_values);
                if (range_width > 0)
                {
                    port.width = range_width;
                    port.raw_type = tokens.empty() ? "logic" : tokens.front();
                }
                else if (tokens.empty() || is_basic_type_keyword(tokens.front()))
                {
                    port.width = 1;
                    port.raw_type = tokens.empty() ? "logic" : tokens.front();
                }
                else
                {
                    // named custom type on a plain port
                    port.raw_type = tokens.front();
                    port.width = probe_type_width(tokens.front(), pkg_dirs, include_dirs, extra_args);
                }
                port.is_packed = port.width > 1;

                result.plain_ports.push_back(port);
            }
            else
            {
                std::istringstream iss2(entry);
                std::string iface_and_modport, port_name;
                iss2 >> iface_and_modport >> port_name;
                if (port_name.empty())
                    continue; // not actually a port entry

                SvIfacePort ifp;
                auto dot = iface_and_modport.find('.');
                if (dot != std::string::npos)
                {
                    ifp.iface_type = iface_and_modport.substr(0, dot);
                    ifp.modport = iface_and_modport.substr(dot + 1);
                }
                else
                {
                    ifp.iface_type = iface_and_modport;
                }
                ifp.port_name = port_name;

                auto iface_file = find_interface_file(ifp.iface_type, search_dirs);
                if (iface_file.empty())
                {
                    std::cerr << "Warning: could not locate interface '" << ifp.iface_type << "' (port '" << ifp.port_name << "') under any search dir — leaving its members empty, wire it by hand.\n";
                }
                else if (ifp.modport.empty())
                {
                    std::cerr << "Warning: port '" << ifp.port_name << "' of interface '" << ifp.iface_type << "' has no modport — cannot determine per-signal direction, leaving members empty.\n";
                }
                else
                {
                    std::string iface_body = read_file(iface_file);
                    ifp.members = parse_modport_members(iface_body, ifp.modport, pkg_dirs, include_dirs, extra_args);
                    if (ifp.members.empty())
                        std::cerr << "Warning: modport '" << ifp.modport << "' not found in " << iface_file.string() << "\n";
                }

                result.iface_ports.push_back(ifp);
            }
        }

        return result;
    }

    SvModule parse_sv_module(
        const std::filesystem::path &sv_file,
        const std::string &top_module,
        const std::vector<std::filesystem::path> &pkg_dirs,
        const std::vector<std::filesystem::path> &include_dirs,
        const std::vector<std::string> &extra_args,
        const std::filesystem::path &work_dir_in
    )
    {
        if (which("verilator").empty())
            throw std::runtime_error(
                "verilator not found on PATH.\n"
                "Install it via your package manager or from https://verilator.org"
            );

        // Determine working directory for JSON output
        std::filesystem::path work_dir = work_dir_in.empty() ? std::filesystem::temp_directory_path() / ("simforge_" + top_module) : work_dir_in;

        std::filesystem::create_directories(work_dir);

        // Build verilator args
        std::vector<std::string> args = {"--json-only", "--top-module", top_module, "--Mdir", work_dir.string()};

        std::vector<std::filesystem::path> pkg_files;

        for (const auto &dir : pkg_dirs)
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(dir))
            {
                if (!entry.is_regular_file())
                    continue;

                auto ext = entry.path().extension().string();

                if (ext == ".sv" || ext == ".v")
                {
                    pkg_files.push_back(entry.path());
                }
            }
        }

        for (const auto &a : extra_args)
            args.push_back(a);

        for (const auto &f : pkg_files)
            args.push_back(f.string());

        for (const auto &d : include_dirs)
            args.push_back("+incdir+" + d.string());

        args.push_back(sv_file.string());

        auto result = run("verilator", args, work_dir);

        if (!result.ok())
        {
            throw std::runtime_error("verilator --json-only failed (exit " + std::to_string(result.exit_code) + "):\n" + result.stderr_output);
        }

        std::filesystem::path json_path = work_dir / ("V" + top_module + ".tree.json");

        if (!std::filesystem::exists(json_path))
        {
            std::string found;
            if (std::filesystem::exists(work_dir))
                for (const auto &e : std::filesystem::directory_iterator(work_dir))
                    found += "\n  - " + e.path().filename().string();
            throw std::runtime_error(
                "Could not find JSON output from verilator at " + json_path.string() +
                (found.empty() ? "\n(and " + work_dir.string() + " is empty)" : "\nFiles actually present in " + work_dir.string() + ":" + found)
            );
        }

        return parse_json(json_path, top_module);
    }
} // namespace simforge::cli::utils
