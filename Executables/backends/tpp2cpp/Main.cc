#include <tpp/IR.h>
#include <tpp/Runtime.h>
#include <CodegenHelpers.h>
#include "defs.h"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

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

struct tpp2cpp
{
    std::string namespaceName;
    Mode mode = Mode::None;
    bool externalRuntime = false;
    std::vector<std::string> includes;
    tpp::IR input;
    tpp2cpp(int argc, char *argv[]);
    void run();
};

int main(int argc, char *argv[])
{
    tpp2cpp app(argc, argv);
    app.run();
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

tpp2cpp::tpp2cpp(int argc, char *argv[])
{
    static const std::map<std::string, Mode> commands = {
        {"types", Mode::Types},
        {"functions", Mode::Functions},
        {"impl", Mode::Implementation}
    };
    auto cli = codegen::parseBackendCommandLine(argc, argv, commands, printUsage, true);
    namespaceName = std::move(cli.namespaceName);
    mode = cli.mode;
    externalRuntime = cli.externalRuntime;
    includes = std::move(cli.includes);
    input = codegen::loadIRInputOrExit(cli.inputFile);
}

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

static std::string toCppStringLiteral(const std::string &s);
static std::string injectDocComments(const std::string &output, const std::map<std::string, std::string> &docComments);
static std::map<std::string, std::string> buildTypeDocComments(const tpp::IR &ir);
static std::map<std::string, std::string> buildFunctionDocComments(const tpp::IR &ir);

static const tpp::IR &mainIR()
{
    static const tpp::IR result =
        nlohmann::json::parse(defs_ir_json).get<tpp::IR>();
    return result;
}

void tpp2cpp::run()
{
    const auto &iRep = mainIR();

    auto renderFunction = [&](const std::string &fnName, const nlohmann::json &ctx) {
        const tpp::FunctionDef *fn = nullptr;
        std::string error, output;
        if (!(tpp::get_function(iRep, fnName, fn, error) && tpp::render_function(iRep, *fn, ctx, output, error)))
        {
            std::cerr << "defs.tpp: error: Failed to render " << fnName << ": " << error << std::endl;
            exit(EXIT_FAILURE);
        }
        return output;
    };

    std::string output;
    switch (mode)
    {
    case Mode::None:
        printUsage();
        exit(EXIT_FAILURE);
        break;
    case Mode::Types:
        output = renderFunction("render_cpp_types", to_render_cpp_type_input(input, includes, namespaceName));
        output = injectDocComments(output, buildTypeDocComments(input));
        break;
    case Mode::Functions:
        output = renderFunction("render_cpp_functions", to_render_cpp_functions_input(input, includes, namespaceName));
        output = injectDocComments(output, buildFunctionDocComments(input));
        break;
    case Mode::Implementation:
    {
        auto ctx = buildFunctionsContext(input, "", includes, namespaceName);
        output = renderFunction("render_cpp_native_implementation", nlohmann::json(ctx));
        break;
    }
    }

    std::cout << output << std::endl;
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

static std::string makeStructDocMarker(const std::string &name)
{
    return "__TPP_DOC__STRUCT__" + name + "__";
}

static std::string makeEnumDocMarker(const std::string &name)
{
    return "__TPP_DOC__ENUM__" + name + "__";
}

static std::string makeFieldDocMarker(const std::string &structName, const std::string &fieldName)
{
    return "__TPP_DOC__FIELD__" + structName + "__" + fieldName + "__";
}

static std::string makeVariantDocMarker(const std::string &enumName, const std::string &variantTag)
{
    return "__TPP_DOC__VARIANT__" + enumName + "__" + variantTag + "__";
}

static std::string makeFunctionDocMarker(size_t index)
{
    return "__TPP_DOC__FUNCTION__" + std::to_string(index) + "__";
}

static std::string formatDocComment(const std::string &doc, const std::string &indent)
{
    if (doc.empty())
        return {};

    std::ostringstream formatted;
    formatted << indent << "/**\n";

    size_t start = 0;
    while (start <= doc.size())
    {
        size_t end = doc.find('\n', start);
        std::string line = end == std::string::npos ? doc.substr(start) : doc.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        formatted << indent << line << "\n";
        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    formatted << indent << " */\n";
    return formatted.str();
}

static std::string injectDocComments(const std::string &output, const std::map<std::string, std::string> &docComments)
{
    std::ostringstream rewritten;
    size_t lineStart = 0;

    while (lineStart < output.size())
    {
        size_t lineEnd = output.find('\n', lineStart);
        const bool hasTrailingNewline = lineEnd != std::string::npos;
        if (!hasTrailingNewline)
            lineEnd = output.size();

        const std::string line = output.substr(lineStart, lineEnd - lineStart);
        const size_t indentEnd = line.find_first_not_of(" \t");
        const std::string indent = indentEnd == std::string::npos ? line : line.substr(0, indentEnd);
        const std::string marker = indentEnd == std::string::npos ? std::string{} : line.substr(indentEnd);

        auto docIt = docComments.find(marker);
        if (docIt != docComments.end())
        {
            rewritten << formatDocComment(docIt->second, indent);
        }
        else
        {
            rewritten << line;
            if (hasTrailingNewline)
                rewritten << '\n';
        }

        if (!hasTrailingNewline)
            break;

        lineStart = lineEnd + 1;
    }

    return rewritten.str();
}

static std::map<std::string, std::string> buildTypeDocComments(const tpp::IR &ir)
{
    std::map<std::string, std::string> docComments;
    for (const auto &structDef : ir.structs)
    {
        docComments.emplace(makeStructDocMarker(structDef.name), structDef.doc);
        for (const auto &field : structDef.fields)
            docComments.emplace(makeFieldDocMarker(structDef.name, field.name), field.doc);
    }

    for (const auto &enumDef : ir.enums)
    {
        docComments.emplace(makeEnumDocMarker(enumDef.name), enumDef.doc);
        for (const auto &variant : enumDef.variants)
            docComments.emplace(makeVariantDocMarker(enumDef.name, variant.tag), variant.doc);
    }

    return docComments;
}

static std::map<std::string, std::string> buildFunctionDocComments(const tpp::IR &ir)
{
    std::map<std::string, std::string> docComments;
    for (size_t index = 0; index < ir.functions.size(); ++index)
        docComments.emplace(makeFunctionDocMarker(index), ir.functions[index].doc);
    return docComments;
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
    nlohmann::json irJson = iRep;
    nlohmann::json structs = irJson["structs"];
    for (auto &structDef : structs)
        structDef["rawTypedefs"] = toCppStringLiteral(structDef["rawTypedefs"].get<std::string>());

    nlohmann::json preStructEnums = nlohmann::json::array();
    nlohmann::json postStructEnums = nlohmann::json::array();
    const nlohmann::json &allEnums = irJson["enums"];
    for (size_t enumIndex = 0; enumIndex < iRep.enums.size(); ++enumIndex)
    {
        auto enumJson = allEnums[enumIndex];
        enumJson["rawTypedefs"] = toCppStringLiteral(enumJson["rawTypedefs"].get<std::string>());
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
    nlohmann::json irJson = iRep;
    nlohmann::json functions = irJson["functions"];
    for (auto &function : functions)
        for (auto &param : function["params"])
            qualifyNamedTypeJson(param["type"]);

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