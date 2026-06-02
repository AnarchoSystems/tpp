#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <tpp/ProjectConfigResolver.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace tpp;

static std::string readFile(const fs::path &path)
{
    std::ifstream file(path);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

static bool hasDiagnostics(const std::vector<tpp::DiagnosticLSPMessage> &messages)
{
    for (const auto &message : messages)
        if (!message.diagnostics.empty())
            return true;
    return false;
}

// input: a single folder. if none is specified, use working directory.
// reads tpp-config.json in that folder which specifies which .tpp files are types and which are templates.
// finally, compile.
// if not successful, print diagnostics to stderr in a format that vscode's problem matcher gcc can understand it.
// if successful, serialize the IR to stdout as JSON.

struct tppApp
{
    std::string inputDirectory;
    bool includeSourceRanges = false;
    bool printInputs = false;
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
                         "Compiles .tpp and .tpp files to JSON IR on stdout.\n"
                         "The folder must contain a tpp-config.json file specifying which .tpp files\n"
                         "are type definitions and which are templates:\n"
                         "  {\"types\": [\"types.tpp\", \"types/*\"], \"templates\": [\"templates/*\"]}\n"
                         "\n"
                         "Options:\n"
                         "  --source-ranges  Include source location info in the IR output\n"
                         "  --print-inputs   Print the resolved config, source, and policy inputs\n";
            exit(0);
        }
        else if (arg == "--source-ranges")
        {
            includeSourceRanges = true;
        }
        else if (arg == "--print-inputs")
        {
            printInputs = true;
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
    ResolvedProjectInputs inputs;
    const fs::path baseDir = fs::path(inputDirectory).lexically_normal();
    {
        try
        {
            inputs = load_project_inputs(baseDir);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << "\n";
            exit(1);
        }
    }

    if (printInputs)
    {
        std::cout << inputs.configPath.string() << std::endl;
        for (const auto &source : inputs.sourceFiles)
            std::cout << source.path.string() << std::endl;
        for (const auto &policyPath : inputs.policyFiles)
            std::cout << policyPath.string() << std::endl;
        exit(EXIT_SUCCESS);
    }

    // Collect file paths first so we can produce stable per-file diagnostics
    // for missing-file errors before running the pipeline.
    struct FileEntry
    {
        std::string path;
        bool isTypes;
    };
    std::vector<FileEntry> fileEntries;
    for (const auto &source : inputs.sourceFiles)
        fileEntries.push_back({source.path.string(), source.isTypes});

    std::vector<DiagnosticLSPMessage> diags;
    diags.reserve(fileEntries.size());
    TppProject project;

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
            project.add_type_source(std::move(typedefs), fe.path);
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
            project.add_template_source(std::move(templates), fe.path);
        }
    }

    // Load replacement policies
    for (const auto &policyPath : inputs.policyFiles)
        project.add_policy_source(readFile(policyPath), policyPath.string());

    CompileOptions options;
    options.includeSourceRanges = includeSourceRanges;
    auto compileResult = tpp::compile(project, options);
    IR output = std::move(compileResult.ir);
    diags = std::move(compileResult.diagnostics);

    bool success = compileResult && !hasDiagnostics(diags);
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