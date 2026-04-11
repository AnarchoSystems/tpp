// EmbedTemplatesMain.cc — shared build-time helper for all tpp backends.
//
// Usage:
//   tpp-embed-templates <types-file> <defs-file>     (legacy two-file mode)
//   tpp-embed-templates <directory>                   (config-based mode)
//
// In config-based mode, <directory> must contain a tpp-config.json listing
// "types" and "templates" arrays.  All type sources are concatenated into
// one embedded string; all template sources into another.
//
// Compiles the type definitions and template definitions, then renders
// the `render_templates` function to produce a C++ header with constexpr
// strings.  Used by tpp2cpp, tpp2java, tpp2swift, make-stat-test, etc.

#include <tpp/Compiler.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

static std::string readFile(const std::string &fileName)
{
    std::ifstream file(fileName);
    if (!file.is_open())
        throw std::runtime_error("Could not open file: " + fileName);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::string toGoodConstexprString(const std::string &content)
{
    std::string escaped = "\"";
    for (char c : content)
    {
        if (c == '\n')
            escaped += "\\n\"\n\"";
        else
        {
            if (c == '\\' || c == '"')
                escaped += '\\';
            escaped += c;
        }
    }
    return escaped + "\"";
}

namespace fs = std::filesystem;

// Returns true if the string matches the glob pattern (supports '*' wildcard only).
static bool matchGlob(const std::string &pattern, const std::string &text)
{
    size_t pi = 0, ti = 0, starPos = std::string::npos, matchPos = 0;
    while (ti < text.size())
    {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti]))
            { ++pi; ++ti; }
        else if (pi < pattern.size() && pattern[pi] == '*')
            { starPos = pi++; matchPos = ti; }
        else if (starPos != std::string::npos)
            { pi = starPos + 1; ti = ++matchPos; }
        else
            return false;
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

static std::vector<fs::path> expandPattern(const fs::path &baseDir, const std::string &pattern)
{
    fs::path patPath(pattern);
    std::string filename = patPath.filename().string();
    fs::path parentDir = baseDir / patPath.parent_path();
    if (filename.find('*') == std::string::npos)
        return {baseDir / pattern};
    std::vector<fs::path> results;
    if (fs::is_directory(parentDir))
    {
        for (const auto &entry : fs::directory_iterator(parentDir))
        {
            if (!entry.is_regular_file()) continue;
            if (matchGlob(filename, entry.path().filename().string()))
                results.push_back(entry.path());
        }
        std::sort(results.begin(), results.end());
    }
    return results;
}

int main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3)
    {
        std::cerr << "Usage: tpp-embed-templates <directory>\n"
                     "       tpp-embed-templates <types-file> <defs-file>\n";
        return EXIT_FAILURE;
    }

    std::string allTypes;
    std::string allTemplates;

    if (argc == 2)
    {
        // Config-based mode: read tpp-config.json from directory
        fs::path dir(argv[1]);
        fs::path configPath = dir / "tpp-config.json";
        std::ifstream configFile(configPath);
        if (!configFile.is_open())
        {
            std::cerr << "Error: " << configPath.string() << ": tpp-config.json not found\n";
            return EXIT_FAILURE;
        }
        nlohmann::json config;
        try { config = nlohmann::json::parse(std::string(
            std::istreambuf_iterator<char>(configFile),
            std::istreambuf_iterator<char>())); }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << configPath.string() << ": " << e.what() << "\n";
            return EXIT_FAILURE;
        }

        for (const auto &pat : config.value("types", nlohmann::json::array()))
            for (const auto &p : expandPattern(dir, pat.get<std::string>()))
                allTypes += readFile(p.string()) + "\n";

        for (const auto &pat : config.value("templates", nlohmann::json::array()))
            for (const auto &p : expandPattern(dir, pat.get<std::string>()))
                allTemplates += readFile(p.string()) + "\n";
    }
    else
    {
        // Legacy two-file mode
        allTypes     = readFile(argv[1]);
        allTemplates = readFile(argv[2]);
    }

    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(2);

    if (!allTypes.empty())
        compiler.add_types(allTypes, diagnostics.emplace_back("types").diagnostics);

    compiler.add_templates(allTemplates, diagnostics.emplace_back("templates").diagnostics);

    tpp::IR output;
    if (!compiler.compile(output))
    {
        for (const auto &msg : diagnostics)
            for (auto &gccDiag : msg.toGCCDiagnostics())
                std::cerr << gccDiag << std::endl;
        return EXIT_FAILURE;
    }

    tpp::FunctionSymbol fn;
    std::string error;
    if (!output.get_function("render_templates", fn, error))
    {
        std::cerr << "error: " << error << std::endl;
        return EXIT_FAILURE;
    }

    std::string rendered;
    std::vector<std::string> input = {
        toGoodConstexprString(allTypes),
        toGoodConstexprString(allTemplates)};
    if (!fn.render(nlohmann::json(input), rendered, error))
    {
        std::cerr << "error: " << error << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << rendered << std::endl;
    return EXIT_SUCCESS;
}
