#include <tpp/Compiler.h>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace tpp;

// input: a single folder. if none is specified, use working directory.
// recursively find all .tpp and .tpp.types files, add either as types or as templates.
// finally, compile.
// if not successful, print diagnostics to stderr in a format that vscode's problem matcher gcc can understand it.
// if successful, serialize the CompilerOutput to stdout as JSON.
// possible flags:
// -h, --help: print usage info
// -v, --verbose: this run is not meant to be machine readable, pretty print any output and print what you're doing to stdout
// --log <file>: also log output to the specified file (for debugging tpp itself)

struct tppApp
{
    bool verbose = false;
    std::ofstream logFile;
    std::string inputDirectory;
    tppApp(int argc, char *argv[]);
    void log(const std::string &message);
    void run();
    ~tppApp();
};

int main(int argc, char *argv[])
{
    tppApp app(argc, argv);
    app.run();
}

tppApp::tppApp(int argc, char *argv[])
{
    std::string logFilePath;
    bool bHasSeenT = false;
    bool bHasSeenTp = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: tpp [options] [folder]\n"
                         "Options:\n"
                         "  -h, --help       Show this help message\n"
                         "  -v, --verbose    Print verbose output\n"
                         "  --log <file>     Log output to the specified file\n";
            exit(0);
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg == "--log")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --log option requires a file path argument\n";
                exit(1);
            }
            logFilePath = argv[++i];
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

    if (!logFilePath.empty())
    {
        logFile.open(logFilePath);
        if (!logFile.is_open())
        {
            std::cerr << "Error: Could not open log file: " << logFilePath << "\n";
            exit(1);
        }
    }

    if (inputDirectory.empty())
    {
        inputDirectory = ".";
    }
}

tppApp::~tppApp()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

void tppApp::log(const std::string &message)
{
    if (verbose)
    {
        std::cout << message << std::endl;
    }
    if (logFile.is_open())
    {
        logFile << message << std::endl;
    }
}

void tppApp::run()
{
    std::vector<DiagnosticLSPMessage> diags;
    Compiler compiler;

    for (const auto &entry : std::filesystem::recursive_directory_iterator(inputDirectory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        auto pathString = entry.path().string();
        if (pathString.size() > 10 && pathString.substr(pathString.size() - 10) == ".tpp.types")
        {
            log("Found typedefs file: " + pathString);
            diags.push_back(DiagnosticLSPMessage(pathString));
            std::ifstream file(entry.path());
            if (!file.is_open())
            {
                diags.back().diagnostics.push_back({{}, "Failed to open file", DiagnosticSeverity::Error});
                continue;
            }
            std::string typedefs((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            compiler.add_types(typedefs, diags.back().diagnostics);
            file.close();
        }
        else if (pathString.size() > 4 && pathString.substr(pathString.size() - 4) == ".tpp")
        {
            log("Found template file: " + pathString);
            diags.push_back(DiagnosticLSPMessage(pathString));
            std::ifstream file(entry.path());
            if (!file.is_open())
            {
                diags.back().diagnostics.push_back({{}, "Failed to open file", DiagnosticSeverity::Error});
                continue;
            }
            std::string templates((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            compiler.add_templates(templates, diags.back().diagnostics);
            file.close();
        }
    }
    log("Finished loading files. Compiling...");
    CompilerOutput output;
    bool success = compiler.compile(output);
    if (success)
    {
        nlohmann::json j = output;
        log("Successfully compiled. Output:");
        log(j.dump(4));
        log("Outputting JSON to stdout...");
        std::cout << j.dump() << std::endl;
    }
    else
    {
        log("Compilation failed. Diagnostics:");
    }
    for (const auto &diag : diags)
    {
        for (const auto &d : diag.diagnostics)
        {
            std::string severityStr = "error";
            if (d.severity)
            {
                switch (d.severity.value())
                {
                case DiagnosticSeverity::Error:
                    severityStr = "error";
                    break;
                case DiagnosticSeverity::Warning:
                    severityStr = "warning";
                    break;
                case DiagnosticSeverity::Information:
                    severityStr = "info";
                    break;
                case DiagnosticSeverity::Hint:
                    severityStr = "hint";
                    break;
                }
            }
            std::string diagnostic = diag.uri + ":" + std::to_string(d.range.start.line + 1) + ":" +
                                     std::to_string(d.range.start.character + 1) + ": " +
                                     severityStr + ": " + d.message;
            log(diagnostic);
            std::cerr << diagnostic << std::endl;
        }
    }
    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}