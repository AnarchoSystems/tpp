// tpp2java — generates Java source from tpp intermediate representation.
//
// Usage:
//   tpp2java <command> [options]
//
// Commands:
//   source          Generate Java types + rendering functions (requires runtime-shared)
//   runtime         Alias for runtime-shared
//   runtime-shared  Generate the dedicated runtime helpers file
//   bundle          Generate a namespaced Java wrapper with types + functions
//
// Options:
//   --input <file>    Read IR JSON from file instead of stdin
//   -ns <name>        Use as the Java class name (default: Functions)

#include <tpp/IR.h>
#include <CodegenHelpers.h>
#include "java_defs_functions.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

static std::string formatJavaDocComment(const std::string &doc, const std::string &indent);
static void printUsage();

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR
// ═══════════════════════════════════════════════════════════════════════════════

static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::string &namespaceName)
{
    auto result = codegen::buildFunctionsContext(
        ir, functionPrefix, namespaceName, codegen::BuildFunctionsConfig::forJava());

    for (auto &function : result.functions)
    {
        if (function.doc.has_value() && !function.doc->empty())
            function.doc = formatJavaDocComment(*function.doc, "    ");
    }

    return result;
}

static std::string formatJavaDocComment(const std::string &doc, const std::string &indent)
{
    return codegen::formatBlockDocComment(doc, indent, " *");
}

static nlohmann::json buildJavaSourceInput(const tpp::IR &ir, bool nested)
{
    return codegen::buildSourceInput(ir, nested, formatJavaDocComment);
}

enum class Mode
{
    Source,
    Runtime,
    RuntimeShared,
    Bundle
};

namespace
{

struct ParsedCommandLine
{
    Mode mode = Mode::Source;
    std::string inputFile;
    std::string namespaceName;
};

static std::string readTextInputOrExit(const std::string &inputFile)
{
    if (!inputFile.empty())
    {
        std::ifstream inputStream(inputFile);
        if (!inputStream.is_open())
        {
            std::cerr << "Could not open input file: " << inputFile << std::endl;
            std::exit(EXIT_FAILURE);
        }
        return std::string(std::istreambuf_iterator<char>(inputStream), std::istreambuf_iterator<char>());
    }

    return std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
}

static nlohmann::json parseJsonTextOrExit(const std::string &inputJson,
                                          const std::string &description)
{
    try
    {
        return nlohmann::json::parse(inputJson);
    }
    catch (const std::exception &error)
    {
        std::cerr << "Failed to parse " << description << ": " << error.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

static tpp::IR loadIRInputOrExit(const std::string &inputFile)
{
    try
    {
        return parseJsonTextOrExit(readTextInputOrExit(inputFile), "input JSON").get<tpp::IR>();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Failed to decode input JSON as tpp IR: " << error.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

static Mode parseModeOrExit(const std::string &command)
{
    if (command == "source")
        return Mode::Source;
    if (command == "runtime")
        return Mode::Runtime;
    if (command == "runtime-shared")
        return Mode::RuntimeShared;
    if (command == "bundle")
        return Mode::Bundle;

    std::cerr << "Unknown command: " << command << "\n\n";
    printUsage();
    std::exit(EXIT_FAILURE);
}

static ParsedCommandLine parseCommandLineOrExit(int argc, char *argv[])
{
    ParsedCommandLine result;

    if (argc < 2)
    {
        printUsage();
        std::exit(EXIT_FAILURE);
    }

    const std::string command = argv[1];
    if (command == "-h" || command == "--help")
    {
        printUsage();
        std::exit(EXIT_SUCCESS);
    }
    result.mode = parseModeOrExit(command);

    for (int index = 2; index < argc; ++index)
    {
        const std::string arg = argv[index];
        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            std::exit(EXIT_SUCCESS);
        }
        if (arg == "-ns")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "-ns requires a namespace argument\n";
                std::exit(EXIT_FAILURE);
            }
            result.namespaceName = argv[++index];
            continue;
        }
        if (arg == "--input")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "--input requires a file argument\n";
                std::exit(EXIT_FAILURE);
            }
            result.inputFile = argv[++index];
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\nUse -h for usage info.\n";
        std::exit(EXIT_FAILURE);
    }

    return result;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════════

static void printUsage()
{
    std::cout << "Usage: tpp2java <command> [options]\n"
                 "\n"
                 "Commands:\n"
                 "  source          Generate Java types + rendering functions (requires runtime-shared at compile time)\n"
                 "  runtime         Alias for runtime-shared\n"
                 "  runtime-shared  Generate the dedicated runtime helpers file\n"
                 "  bundle          Generate a namespaced Java wrapper with types + functions (requires runtime-shared at compile time)\n"
                 "\n"
                 "Options:\n"
                 "  --input <file>    Read IR JSON from file instead of stdin\n"
                 "  -ns <name>        Use as the Java class name (default: Functions)\n"
                 "  -h, --help        Print this message\n";
}

int main(int argc, char *argv[])
{
    const auto cli = parseCommandLineOrExit(argc, argv);
    if (cli.mode == Mode::RuntimeShared || cli.mode == Mode::Runtime)
    {
        std::cout << codegen::render_java_shared_runtime() << std::endl;
        return EXIT_SUCCESS;
    }

    const auto ir = loadIRInputOrExit(cli.inputFile);
    std::string namespaceName = cli.namespaceName;

    // Function prefix: "render_" when no namespace, "" when namespace set
    std::string functionPrefix = namespaceName.empty() ? "render_" : "";

    auto sourceInput = buildJavaSourceInput(ir, false);
    auto nestedSourceInput = buildJavaSourceInput(ir, true);
    auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);

    if (cli.mode == Mode::Bundle)
    {
        if (namespaceName.empty())
        {
            std::cerr << "bundle requires -ns <name>\n";
            return EXIT_FAILURE;
        }
        std::cout << codegen::render_java_bundle(nestedSourceInput, funcCtx) << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << codegen::render_java_source(sourceInput);
    std::cout << codegen::render_java_functions(funcCtx);

    return EXIT_SUCCESS;
}
