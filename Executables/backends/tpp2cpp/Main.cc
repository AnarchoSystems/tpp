#include <tpp/IR.h>
#include <tpp/Runtime.h>
#include <CodegenHelpers.h>
#include "defs.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

// usage: tpp2cpp <command> [options]
// commands:
//   types     — produces a header file with type definitions
//   functions — produces a header file rendering the template functions as C++ functions
//   impl      — produces a .cc file with the implementations of the functions declared by 'functions'
// options:
//   -ns <name>       wrap generated code in a namespace
//   -i <file>        files to include at the top of the generated code (repeatable)
//   --input <file>   read IR JSON from file instead of stdin

enum Mode
{
    None,
    Types,
    Functions,
    Implementation
};

void printUsage();
static const tpp::IR &mainIR();
static nlohmann::json to_render_cpp_type_input(const tpp::IR &iRep,
                                               const std::vector<std::string> &includes,
                                               const std::string &namespaceName);
static nlohmann::json to_render_cpp_functions_input(const tpp::IR &iRep,
                                                    const std::vector<std::string> &includes,
                                                    const std::string &namespaceName);
static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::vector<std::string> &includes,
    const std::string &namespaceName);

namespace
{

struct ParsedCommandLine
{
    Mode mode = Mode::None;
    std::string inputFile;
    std::string namespaceName;
    std::vector<std::string> includes;
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
    if (command == "types")
        return Mode::Types;
    if (command == "functions")
        return Mode::Functions;
    if (command == "impl")
        return Mode::Implementation;

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
        if (arg == "-i")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "-i requires a file argument\n";
                std::exit(EXIT_FAILURE);
            }
            result.includes.push_back(argv[++index]);
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\nUse -h for usage info.\n";
        std::exit(EXIT_FAILURE);
    }

    return result;
}

} // namespace

int main(int argc, char *argv[])
{
    const auto cli = parseCommandLineOrExit(argc, argv);
    const auto input = loadIRInputOrExit(cli.inputFile);
    const auto &iRep = mainIR();

    auto renderFunction = [&](const std::string &fnName, const nlohmann::json &ctx) {
        const tpp::FunctionDef *fn = nullptr;
        std::string error, output;
        if (!(tpp::get_function(iRep, fnName, fn, error) && tpp::render_function(iRep, *fn, ctx, output, error)))
        {
            std::cerr << "defs.tpp: error: Failed to render " << fnName << ": " << error << std::endl;
            std::exit(EXIT_FAILURE);
        }
        return output;
    };

    std::string output;
    switch (cli.mode)
    {
    case Mode::None:
        printUsage();
        return EXIT_FAILURE;
    case Mode::Types:
        output = renderFunction("render_cpp_types", to_render_cpp_type_input(input, cli.includes, cli.namespaceName));
        break;
    case Mode::Functions:
        output = renderFunction("render_cpp_functions", to_render_cpp_functions_input(input, cli.includes, cli.namespaceName));
        break;
    case Mode::Implementation:
    {
        auto ctx = buildFunctionsContext(input, "", cli.includes, cli.namespaceName);
        output = renderFunction("render_cpp_native_implementation", nlohmann::json(ctx));
        break;
    }
    }

    std::cout << output << std::endl;
    return EXIT_SUCCESS;
}

void printUsage()
{
    std::cout << "Usage: tpp2cpp <command> [options]\n"
                 "\n"
                 "Commands:\n"
                 "  types      Produce a header with C++ type definitions\n"
                 "  functions  Produce a header with C++ function declarations\n"
                 "  impl       Produce a .cc with function implementations\n"
                 "\n"
                 "Options:\n"
                 "  -ns <name>        Wrap generated code in a namespace\n"
                 "  -i <file>         Include file at top of generated code (repeatable)\n"
                 "  --input <file>    Read IR JSON from file instead of stdin\n"
                 "  -h, --help        Print this message\n";
}

static std::string toCppStringLiteral(const std::string &s);
static void qualifyNamedTypeJson(nlohmann::json &typeJson);

static const tpp::IR &mainIR()
{
    static const tpp::IR result =
        nlohmann::json::parse(defs_ir_json).get<tpp::IR>();
    return result;
}

static std::string toCppStringLiteral(const std::string &s)
{
    std::string result = "\"";
    for (char c : s)
    {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else result += c;
    }
    return result + "\"";
}

static void setFormattedDocComment(nlohmann::json &target, const std::string &doc, const std::string &indent)
{
    const std::string formatted = codegen::formatBlockDocComment(doc, indent);
    if (!formatted.empty())
        target["docComment"] = formatted;
}

static nlohmann::json toCppFieldContext(const tpp::FieldDef &field)
{
    nlohmann::json fieldJson;
    fieldJson["name"] = field.name;
    fieldJson["type"] = *field.type;
    fieldJson["recursive"] = field.recursive;
    setFormattedDocComment(fieldJson, field.doc, "    ");
    return fieldJson;
}

static nlohmann::json toCppStructContext(const tpp::StructDef &structDef)
{
    nlohmann::json structJson;
    structJson["name"] = structDef.name;
    structJson["fields"] = nlohmann::json::array();
    for (const auto &field : structDef.fields)
        structJson["fields"].push_back(toCppFieldContext(field));
    structJson["rawTypedefs"] = toCppStringLiteral(structDef.rawTypedefs);
    setFormattedDocComment(structJson, structDef.doc, "");
    return structJson;
}

static nlohmann::json toCppVariantContext(const tpp::VariantDef &variant)
{
    nlohmann::json variantJson;
    variantJson["tag"] = variant.tag;
    if (variant.payload)
        variantJson["payload"] = *variant.payload;
    variantJson["recursive"] = variant.recursive;
    setFormattedDocComment(variantJson, variant.doc, "");
    return variantJson;
}

static nlohmann::json toCppEnumContext(const tpp::EnumDef &enumDef)
{
    nlohmann::json enumJson;
    enumJson["name"] = enumDef.name;
    enumJson["variants"] = nlohmann::json::array();
    for (const auto &variant : enumDef.variants)
        enumJson["variants"].push_back(toCppVariantContext(variant));
    enumJson["rawTypedefs"] = toCppStringLiteral(enumDef.rawTypedefs);
    setFormattedDocComment(enumJson, enumDef.doc, "");
    return enumJson;
}

static nlohmann::json toCppFunctionContext(const tpp::FunctionDef &function)
{
    nlohmann::json functionJson;
    functionJson["name"] = function.name;
    functionJson["params"] = nlohmann::json::array();
    for (const auto &param : function.params)
    {
        nlohmann::json paramJson = param;
        qualifyNamedTypeJson(paramJson["type"]);
        functionJson["params"].push_back(std::move(paramJson));
    }
    setFormattedDocComment(functionJson, function.doc, "");
    return functionJson;
}

static bool hasRecursiveVariant(const tpp::EnumDef &enumDef)
{
    for (const auto &variant : enumDef.variants)
        if (variant.recursive)
            return true;
    return false;
}

static void qualifyNamedTypeJson(nlohmann::json &typeJson)
{
    if (typeJson.contains("Named"))
    {
        typeJson["Named"] = codegen::qualifyPublicIRTypeName(typeJson["Named"].get<std::string>());
        return;
    }
    if (typeJson.contains("List"))
    {
        qualifyNamedTypeJson(typeJson["List"]);
        return;
    }
    if (typeJson.contains("Optional"))
        qualifyNamedTypeJson(typeJson["Optional"]);
}

static nlohmann::json to_render_cpp_type_input(const tpp::IR &iRep,
                                               const std::vector<std::string> &includes,
                                               const std::string &namespaceName)
{
    nlohmann::json structs = nlohmann::json::array();
    for (const auto &structDef : iRep.structs)
        structs.push_back(toCppStructContext(structDef));

    nlohmann::json preStructEnums = nlohmann::json::array();
    nlohmann::json postStructEnums = nlohmann::json::array();
    for (size_t enumIndex = 0; enumIndex < iRep.enums.size(); ++enumIndex)
    {
        auto enumJson = toCppEnumContext(iRep.enums[enumIndex]);
        if (hasRecursiveVariant(iRep.enums[enumIndex]))
            postStructEnums.push_back(std::move(enumJson));
        else
            preStructEnums.push_back(std::move(enumJson));
    }

    nlohmann::json ns = namespaceName.empty() ? nlohmann::json() : nlohmann::json(namespaceName);
    return nlohmann::json::array({preStructEnums, structs, postStructEnums, nlohmann::json(includes), ns});
}

static nlohmann::json to_render_cpp_functions_input(const tpp::IR &iRep,
                                                    const std::vector<std::string> &includes,
                                                    const std::string &namespaceName)
{
    nlohmann::json functions = nlohmann::json::array();
    for (const auto &function : iRep.functions)
        functions.push_back(toCppFunctionContext(function));

    nlohmann::json ns = namespaceName.empty() ? nlohmann::json() : nlohmann::json(namespaceName);
    return nlohmann::json::array({functions, nlohmann::json(includes), ns, ""});
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR (native rendering)
// ═══════════════════════════════════════════════════════════════════════════════

static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::vector<std::string> &includes,
    const std::string &namespaceName)
{
    return codegen::buildFunctionsContext(
        ir, functionPrefix, namespaceName, codegen::BuildFunctionsConfig::forCpp(includes));
}