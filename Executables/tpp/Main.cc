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
    // Collect file paths first so we can reserve diags to the exact size needed.
    // This is important: add_types / add_templates store raw pointers into
    // diags[i].diagnostics; if diags later reallocates those pointers dangle.
    struct FileEntry
    {
        std::string path;
        bool isTypes;
    };
    std::vector<FileEntry> fileEntries;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(inputDirectory))
    {
        if (!entry.is_regular_file())
            continue;
        auto pathString = entry.path().string();
        if (pathString.size() > 10 && pathString.substr(pathString.size() - 10) == ".tpp.types")
            fileEntries.push_back({pathString, true});
        else if (pathString.size() > 4 && pathString.substr(pathString.size() - 4) == ".tpp")
            fileEntries.push_back({pathString, false});
    }

    std::vector<DiagnosticLSPMessage> diags;
    diags.reserve(fileEntries.size()); // no reallocation after this point
    Compiler compiler;

    for (const auto &fe : fileEntries)
    {
        if (fe.isTypes)
        {
            log("Found typedefs file: " + fe.path);
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
            log("Found template file: " + fe.path);
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
            if (!verbose)
            {
                std::cerr << diagnostic << std::endl;
            }
        }
    }
    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}