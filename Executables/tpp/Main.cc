#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace tpp;

static std::string readFile(const fs::path &path)
{
    std::ifstream file(path);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

// input: a single folder. if none is specified, use working directory.
// reads tpp-config.json in that folder which specifies which .tpp files are types and which are templates.
// finally, compile.
// if not successful, print diagnostics to stderr in a format that vscode's problem matcher gcc can understand it.
// if successful, serialize the IR to stdout as JSON.

// Returns true if the string matches the glob pattern (supports '*' wildcard only).
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

// Expands a single config pattern to a sorted list of .tpp file paths.
// If the pattern contains no '*', returns the single path directly.
// If it contains '*', scans the parent directory and returns matching .tpp files.
static std::vector<fs::path> expandPattern(const fs::path &baseDir, const std::string &pattern)
{
    fs::path patPath(pattern);
    std::string filename = patPath.filename().string();
    fs::path parentDir = baseDir / patPath.parent_path();

    if (filename.find('*') == std::string::npos)
    {
        // Literal path — return as-is (file may or may not exist; errors surface later)
        return {baseDir / pattern};
    }

    std::vector<fs::path> results;
    if (fs::is_directory(parentDir))
    {
        for (const auto &entry : fs::directory_iterator(parentDir))
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

struct tppApp
{
    std::string inputDirectory;
    bool includeSourceRanges = false;
    tppApp(int argc, char *argv[]);
    void run();
};

int main(int argc, char *argv[])
{
    tppApp app(argc, argv);
    app.run();
}

tppApp::tppApp(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: tpp [options] [folder]\n"
                         "\n"
                         "Compiles .tpp and .tpp.types files to JSON IR on stdout.\n"
                         "The folder must contain a tpp-config.json file specifying which .tpp files\n"
                         "are type definitions and which are templates:\n"
                         "  {\"types\": [\"types.tpp\", \"types/*\"], \"templates\": [\"templates/*\"]}\n"
                         "\n"
                         "Options:\n"
                         "  --source-ranges  Include source location info in the IR output\n";
            exit(0);
        }
        else if (arg == "--source-ranges")
        {
            includeSourceRanges = true;
        }
        else
        {
            if (i != argc - 1)
            {
                std::cerr << "Error: Unexpected argument: " << arg << "\n";
                exit(1);
            }
            inputDirectory = arg;
        }
    }

    if (inputDirectory.empty())
    {
        inputDirectory = ".";
    }
}

void tppApp::run()
{
    // Read tpp-config.json from the input directory
    fs::path configPath = fs::path(inputDirectory) / "tpp-config.json";
    nlohmann::json config;
    {
        std::ifstream configFile(configPath);
        if (!configFile.is_open())
        {
            std::cerr << "Error: " << configPath.string() << ": tpp-config.json not found\n";
            exit(1);
        }
        try
        {
            config = nlohmann::json::parse(std::string(
                (std::istreambuf_iterator<char>(configFile)),
                std::istreambuf_iterator<char>()));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << configPath.string() << ": invalid JSON: " << e.what() << "\n";
            exit(1);
        }
    }

    // Collect file paths first so we can reserve diags to the exact size needed.
    // This is important: add_types / add_templates store raw pointers into
    // diags[i].diagnostics; if diags later reallocates those pointers dangle.
    struct FileEntry
    {
        std::string path;
        bool isTypes;
    };
    std::vector<FileEntry> fileEntries;

    fs::path baseDir(inputDirectory);
    for (const auto &pattern : config.value("types", nlohmann::json::array()))
    {
        for (const auto &p : expandPattern(baseDir, pattern.get<std::string>()))
            fileEntries.push_back({p.string(), true});
    }
    for (const auto &pattern : config.value("templates", nlohmann::json::array()))
    {
        for (const auto &p : expandPattern(baseDir, pattern.get<std::string>()))
            fileEntries.push_back({p.string(), false});
    }

    std::vector<DiagnosticLSPMessage> diags;
    diags.reserve(fileEntries.size()); // no reallocation after this point
    Compiler compiler;

    for (const auto &fe : fileEntries)
    {
        if (fe.isTypes)
        {
            diags.push_back(DiagnosticLSPMessage(fe.path));
            std::ifstream file(fe.path);
            if (!file.is_open())
            {
                diags.back().diagnostics.push_back({{}, "Failed to open file", DiagnosticSeverity::Error});
                continue;
            }
            std::string typedefs((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            compiler.add_types(typedefs, diags.back().diagnostics);
        }
        else
        {
            diags.push_back(DiagnosticLSPMessage(fe.path));
            std::ifstream file(fe.path);
            if (!file.is_open())
            {
                diags.back().diagnostics.push_back({{}, "Failed to open file", DiagnosticSeverity::Error});
                continue;
            }
            std::string templates((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            compiler.add_templates(templates, diags.back().diagnostics);
        }
    }

    // Load replacement policies
    for (const auto &policyEntry : config.value("replacement-policies", nlohmann::json::array()))
    {
        fs::path policyPath = fs::path(inputDirectory) / policyEntry.get<std::string>();
        std::string policyText = readFile(policyPath);
        std::vector<tpp::Diagnostic> policyDiagnostics;
        if (!compiler.add_policy_text(policyText, policyDiagnostics))
        {
            for (const auto &diagnostic : policyDiagnostics)
                std::cerr << "Error: " << policyPath.string() << ": " << diagnostic.message << "\n";
            exit(1);
        }
    }

    IR output;
    bool success = compiler.compile(output, includeSourceRanges);
    if (success)
    {
        nlohmann::json j = output;
        std::cout << j.dump() << std::endl;
    }
    for (const auto &diag : diags)
    {
        for (const auto &diagnostic : diag.toGCCDiagnostics())
        {
            std::cerr << diagnostic << std::endl;
        }
    }
    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}