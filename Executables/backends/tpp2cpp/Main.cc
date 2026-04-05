#include <tpp/Compiler.h>
#include "defs.h"
#include <fstream>
#include <iostream>

// usage: tpp2cpp [options]
// options:
// -h, --help: print usage info
// -v, --verbose: print verbose output (for debugging tpp2cpp itself)
// --log <file>: also log output to the specified file (for debugging tpp2cpp itself)
// -ns --namespace <name>: wrap generated code in a namespace (default: none)
// -t, --types: produces a header file with type definitions
// -fun, --functions: produces a header file rendering the template functions as cpp functions
// -impl, --implementation: produces a cpp file with the implementations of the functions declared in the functions header (only needed if -fun/--functions is used)
// -i <file>, --include <file>: files to include at the top of the generated code (can be specified multiple times)
// --input <file>: file to read compiler output from (in json format, in the same format as tpp's output) instead of stdin
// This executable renders type definitions and template functions from JSON (in the same format as tpp's output) to C++ code, and prints it to stdout.
// compiler output is expected to be passed via stdin as json.

enum Mode
{
    None,
    Types,
    Functions,
    Implementation
};

struct tpp2cpp
{
    bool verbose = false;
    std::ofstream logFile;
    std::string namespaceName;
    Mode mode = Mode::None;
    std::vector<std::string> includes;
    tpp::CompilerOutput input;
    tpp2cpp(int argc, char *argv[]);
    ~tpp2cpp();
    void run();
    void log(const std::string &message);
};

int main(int argc, char *argv[])
{
    tpp2cpp app(argc, argv);
    app.run();
    return EXIT_SUCCESS;
}

void printUsage()
{
    std::cout << "Usage: tpp2cpp [options]\n"
                 "Options:\n"
                 "  -h, --help: print usage info\n"
                 "  -v, --verbose: print verbose output (for debugging tpp2cpp itself)\n"
                 "  --log <file>: also log output to the specified file (for debugging tpp2cpp itself)\n"
                 "  -ns --namespace <name>: wrap generated code in a namespace (default: none)\n"
                 "  -t, --types: produces a header file with type definitions\n"
                 "  -fun, --functions: produces a header file rendering the template functions as cpp functions\n"
                 "  -impl, --implementation: produces a cpp file with the implementations of the functions declared in the functions header (only needed if -fun/--functions is used)\n"
                 "  -i <file>, --include <file>: files to include at the top of the generated code (can be specified multiple times)\n";
}

tpp2cpp::tpp2cpp(int argc, char *argv[])
{
    std::string inputFileName;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            exit(EXIT_SUCCESS);
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg == "--log")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--log option requires a file argument\n";
                exit(EXIT_FAILURE);
            }
            logFile.open(argv[++i]);
            if (!logFile.is_open())
            {
                std::cerr << "Could not open log file: " << argv[i] << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (arg == "-ns" || arg == "--namespace")
        {
            if (i + 1 >= argc)
            {
                std::cerr << arg << " option requires a name argument\n";
                exit(EXIT_FAILURE);
            }
            namespaceName = argv[++i];
        }
        else if (arg == "-t" || arg == "--types")
        {
            if (mode != Mode::None)
            {
                std::cerr << "Only one of -t/--types, -fun/--functions and -impl/--implementation can be specified\n";
                exit(EXIT_FAILURE);
            }
            mode = Mode::Types;
        }
        else if (arg == "-fun" || arg == "--functions")
        {
            if (mode != Mode::None)
            {
                std::cerr << "Only one of -t/--types, -fun/--functions and -impl/--implementation can be specified\n";
                exit(EXIT_FAILURE);
            }
            mode = Mode::Functions;
        }
        else if (arg == "-impl" || arg == "--implementation")
        {
            if (mode != Mode::None)
            {
                std::cerr << "Only one of -t/--types, -fun/--functions and -impl/--implementation can be specified\n";
                exit(EXIT_FAILURE);
            }
            mode = Mode::Implementation;
        }
        else if (arg == "-i" || arg == "--include")
        {
            if (i + 1 >= argc)
            {
                std::cerr << arg << " option requires a file argument\n";
                exit(EXIT_FAILURE);
            }
            includes.push_back(argv[++i]);
        }
        else if (arg == "--input")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--input option requires a file argument\n";
                exit(EXIT_FAILURE);
            }
            inputFileName = argv[++i];
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\nUse -h or --help for usage info.\n";
            exit(EXIT_FAILURE);
        }
    }
    // read input from file or stdin
    std::string inputJson;
    if (!inputFileName.empty())
    {
        std::ifstream inputFile(inputFileName);
        if (!inputFile.is_open())
        {
            std::cerr << "Could not open input file: " << inputFileName << std::endl;
            exit(EXIT_FAILURE);
        }
        inputJson.assign((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
    }
    else
    {
        inputJson.assign((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    }
    try
    {
        input = nlohmann::json::parse(inputJson).get<tpp::CompilerOutput>();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse input JSON: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}

tpp2cpp::~tpp2cpp()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

void tpp2cpp::log(const std::string &message)
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

nlohmann::json to_render_cpp_type_input(const tpp::CompilerOutput &compilerOutput,
                                        const std::vector<std::string> &includes);

nlohmann::json to_render_cpp_functions_input(const tpp::CompilerOutput &compilerOutput,
                                             const std::vector<std::string> &includes);

nlohmann::json to_render_cpp_implementation_input(const tpp::CompilerOutput &compilerOutput,
                                                  const std::vector<std::string> &includes);

void tpp2cpp::run()
{
    tpp::Compiler compiler;
    tpp::CompilerOutput compilerOutput;

    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(2);

    compiler.add_types(types_content, diagnostics.emplace_back("defs.tpp.types").diagnostics);
    compiler.add_templates(template_content, diagnostics.emplace_back("defs.tpp").diagnostics);

    log("Compiling templates...");

    if (!compiler.compile(compilerOutput))
    {
        for (const auto &msg : diagnostics)
        {
            for (auto &gccDiagnostic : msg.toGCCDiagnostics())
            {
                log(gccDiagnostic);
                if (!verbose)
                {
                    std::cerr << gccDiagnostic << std::endl;
                }
            }
        }
        exit(EXIT_FAILURE);
    }

    tpp::FunctionSymbol functionSymbol;
    std::string functionName;
    std::function<nlohmann::json(const tpp::CompilerOutput &, const std::vector<std::string> &)> toInputFunction;

    switch (mode)
    {
    case Mode::None:
    {
        printUsage();
        exit(EXIT_FAILURE);
        break;
    }
    case Mode::Types:
    {
        functionName = "render_cpp_types";
        toInputFunction = to_render_cpp_type_input;
        break;
    }
    case Mode::Functions:
    {
        functionName = "render_cpp_functions";
        toInputFunction = to_render_cpp_functions_input;
        break;
    }
    case Mode::Implementation:
    {
        functionName = "render_cpp_implementation";
        toInputFunction = to_render_cpp_implementation_input;
        break;
    }
    }

    std::string renderedOutput;
    std::string error;

    if (!(compilerOutput.get_function(functionName, functionSymbol, error) && functionSymbol.render(toInputFunction(input, includes), renderedOutput, error)))
    {
        std::string errorMessage = "defs.tpp: error: Failed to render output: " + error;
        log(errorMessage);
        if (!verbose)
        {
            std::cerr << errorMessage << std::endl;
        }
        exit(EXIT_FAILURE);
    }

    std::cout << renderedOutput << std::endl;
}

nlohmann::json to_render_cpp_type_input(const tpp::CompilerOutput &compilerOutput,
                                        const std::vector<std::string> &includes)
{
    // TODO
    return {};
}

nlohmann::json to_render_cpp_functions_input(const tpp::CompilerOutput &compilerOutput,
                                             const std::vector<std::string> &includes)
{
    // TODO
    return {};
}

nlohmann::json to_render_cpp_implementation_input(const tpp::CompilerOutput &compilerOutput,
                                                  const std::vector<std::string> &includes)
{
    // TODO
    return {};
}