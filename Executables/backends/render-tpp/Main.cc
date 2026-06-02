#include <tpp/IR.h>
#include <tpp/Runtime.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

// usage: render-tpp [--input <file>] <template> <input> [signature...]
// intermediate representation is expected on stdin unless --input is given.

using namespace tpp;

namespace
{

struct ParsedRenderCommandLine
{
    std::string inputFile;
    std::string templateName;
    nlohmann::json inputData;
    std::vector<std::string> signature;
};

static void printUsage()
{
    std::cerr << "Usage: render-tpp [--input <file>] <template> <input> [signature...]" << std::endl;
}

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

static IR loadIRInputOrExit(const std::string &inputFile)
{
    try
    {
        return parseJsonTextOrExit(readTextInputOrExit(inputFile), "input JSON").get<IR>();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Failed to decode input JSON as tpp IR: " << error.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

static ParsedRenderCommandLine parseCommandLineOrExit(int argc, char *argv[])
{
    ParsedRenderCommandLine result;

    int argumentIndex = 1;
    if (argc > argumentIndex && std::string(argv[argumentIndex]) == "--input")
    {
        if (argc <= argumentIndex + 1)
        {
            printUsage();
            std::exit(EXIT_FAILURE);
        }
        result.inputFile = argv[argumentIndex + 1];
        argumentIndex += 2;
    }

    if (argc - argumentIndex < 2)
    {
        printUsage();
        std::exit(EXIT_FAILURE);
    }

    result.templateName = argv[argumentIndex++];
    result.inputData = parseJsonTextOrExit(argv[argumentIndex++], "render input JSON");
    for (int index = argumentIndex; index < argc; ++index)
        result.signature.emplace_back(argv[index]);

    return result;
}

} // namespace

int main(int argc, char *argv[])
{
    const auto cli = parseCommandLineOrExit(argc, argv);

    try
    {
        IR iRep = loadIRInputOrExit(cli.inputFile);
        const FunctionDef *function = nullptr;
        std::string error;
        bool found = cli.signature.empty()
            ? get_function(iRep, cli.templateName, function, error)
            : get_function(iRep, cli.templateName, cli.signature, function, error);
        if (!found)
        {
            std::cerr << error << std::endl;
            return EXIT_FAILURE;
        }
        std::string output;
        if (!render_function(iRep, *function, cli.inputData, output, error))
        {
            std::cerr << error << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << output;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}