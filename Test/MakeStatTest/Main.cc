// usage: make-stat-test <input_folder>
// Reads .tpp files from input_folder according to tpp-config.json, compiles them,
// extracts the main function's parameter info, and renders a test file (to stdout).

#include <tpp/Compiler.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "make_stat_test_types.h"
#include "make_stat_test_functions.h"

static std::string readFile(const std::filesystem::path &path)
{
    std::ifstream f(path);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

// Returns true if text matches the glob pattern (supports '*' wildcard only).
static bool matchGlob(const std::string &pattern, const std::string &text)
{
    size_t pi = 0, ti = 0, starPos = std::string::npos, matchPos = 0;
    while (ti < text.size())
    {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti]))
        {
            ++pi; ++ti;
        }
        else if (pi < pattern.size() && pattern[pi] == '*')
        {
            starPos = pi++;
            matchPos = ti;
        }
        else if (starPos != std::string::npos)
        {
            pi = starPos + 1;
            ti = ++matchPos;
        }
        else
        {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*')
        ++pi;
    return pi == pattern.size();
}

// Expands a single config pattern to a sorted list of file paths.
static std::vector<std::filesystem::path> expandPattern(const std::filesystem::path &baseDir, const std::string &pattern)
{
    std::filesystem::path patPath(pattern);
    std::string filename = patPath.filename().string();
    std::filesystem::path parentDir = baseDir / patPath.parent_path();

    if (filename.find('*') == std::string::npos)
    {
        return {baseDir / pattern};
    }

    std::vector<std::filesystem::path> results;
    if (std::filesystem::is_directory(parentDir))
    {
        for (const auto &entry : std::filesystem::directory_iterator(parentDir))
        {
            if (!entry.is_regular_file())
                continue;
            auto name = entry.path().filename().string();
            if (name.size() < 4 || name.substr(name.size() - 4) != ".tpp")
                continue;
            if (matchGlob(filename, name))
                results.push_back(entry.path());
        }
        std::sort(results.begin(), results.end());
    }
    return results;
}

static std::string typeRefToCppStringNs(const tpp::TypeRef &t, const std::string &ns);

static std::string typeRefToCppStringNs(const tpp::TypeRef &t, const std::string &ns)
{
    if (std::holds_alternative<tpp::StringType>(t))
        return "std::string";
    if (std::holds_alternative<tpp::IntType>(t))
        return "int";
    if (std::holds_alternative<tpp::BoolType>(t))
        return "bool";
    if (auto *lt = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        return "std::vector<" + typeRefToCppStringNs((*lt)->elementType, ns) + ">";
    if (auto *ot = std::get_if<std::shared_ptr<tpp::OptionalType>>(&t))
        return "std::optional<" + typeRefToCppStringNs((*ot)->innerType, ns) + ">";
    if (auto *nt = std::get_if<tpp::NamedType>(&t))
        return ns.empty() ? nt->name : (ns + "::" + nt->name);
    return "";
}

std::string toGoodConstexprString(const std::string &content)
{
    std::string escapedContent = "\"";
    for (char c : content)
    {
        if (c == '\n')
        {
            escapedContent += "\\n\"\n\""; // preserve runtime newline; split source line for readability
        }
        else
        {
            if (c == '\\' || c == '"')
            {
                escapedContent += '\\';
            }
            escapedContent += c;
        }
    }
    return escapedContent + "\"";
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: make-stat-test <input_folder>" << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path testDir(argv[1]);
    std::string testName = testDir.filename().string();

    // Read tpp-config.json
    std::filesystem::path configPath = testDir / "tpp-config.json";
    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        std::cerr << testDir.string() << ": error: tpp-config.json not found" << std::endl;
        return EXIT_FAILURE;
    }
    nlohmann::json config;
    try
    {
        config = nlohmann::json::parse(std::string(
            (std::istreambuf_iterator<char>(configFile)),
            std::istreambuf_iterator<char>()));
    }
    catch (const std::exception &e)
    {
        std::cerr << configPath.string() << ": error: invalid JSON: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Collect .tpp files in config order: types first, then templates
    struct FileEntry
    {
        std::filesystem::path path;
        bool isTypes;
    };
    std::vector<FileEntry> fileEntries;
    for (const auto &pattern : config.value("types", nlohmann::json::array()))
    {
        for (const auto &p : expandPattern(testDir, pattern.get<std::string>()))
            fileEntries.push_back({p, true});
    }
    for (const auto &pattern : config.value("templates", nlohmann::json::array()))
    {
        for (const auto &p : expandPattern(testDir, pattern.get<std::string>()))
            fileEntries.push_back({p, false});
    }

    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(fileEntries.size());
    for (const auto &fe : fileEntries)
    {
        std::string content = readFile(fe.path);
        auto &msg = diagnostics.emplace_back(fe.path.string());
        if (fe.isTypes)
            compiler.add_types(content, msg.diagnostics);
        else
            compiler.add_templates(content, msg.diagnostics);
    }

    tpp::CompilerOutput compilerOutput;
    if (!compiler.compile(compilerOutput))
    {
        for (const auto &msg : diagnostics)
            for (const auto &d : msg.toGCCDiagnostics())
                std::cerr << d << std::endl;
        return EXIT_FAILURE;
    }

    tpp::FunctionSymbol mainSymbol;
    std::string error;
    if (!compilerOutput.get_function("main", mainSymbol, error))
    {
        std::cerr << testDir.string() << ": error: " << error << std::endl;
        return EXIT_FAILURE;
    }

    // Read input.json
    nlohmann::json inputJson = nlohmann::json::parse(readFile(testDir / "input.json"));

    // Read expected_output.txt (strip trailing newline)
    std::string expectedRaw = readFile(testDir / "expected_output.txt");
    if (!expectedRaw.empty() && expectedRaw.back() == '\n')
        expectedRaw.pop_back();

    // Build Defs
    Defs defs;
    defs.testName = testName;
    defs.expectedOutput = toGoodConstexprString(expectedRaw);

    const auto &params = mainSymbol.function.params;
    if (params.size() == 1)
    {
        Input inp;
        inp.type = typeRefToCppStringNs(params[0].type, testName);
        nlohmann::json value;
        bool isList = std::holds_alternative<std::shared_ptr<tpp::ListType>>(params[0].type);
        if (isList)
        {
            // list param: unwrap double-wrapped array (unary-list convention)
            if (inputJson.is_array() && inputJson.size() == 1 && inputJson[0].is_array())
                value = inputJson[0];
            else
                value = inputJson;
        }
        else
        {
            // non-list param: unwrap single-element array wrapper
            if (inputJson.is_array() && inputJson.size() == 1)
                value = inputJson[0];
            else
                value = inputJson;
        }
        inp.value = value.dump();
        defs.inputs.push_back(inp);
    }
    else
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            Input inp;
            inp.type = typeRefToCppStringNs(params[i].type, testName);
            inp.value = inputJson[i].dump();
            defs.inputs.push_back(inp);
        }
    }

    std::cout << render_test(defs);
    return EXIT_SUCCESS;
}
