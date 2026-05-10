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
#include <iostream>
#include <map>
#include <sstream>

static std::string formatJavaDocComment(const std::string &doc, const std::string &indent);

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

static std::vector<std::string> splitDocLines(const std::string &doc)
{
    std::vector<std::string> lines;
    if (doc.empty())
        return lines;

    size_t start = 0;
    while (start <= doc.size())
    {
        size_t end = doc.find('\n', start);
        std::string line = end == std::string::npos ? doc.substr(start) : doc.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(std::move(line));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    return lines;
}

static void setDocComment(nlohmann::json &target, const std::string &doc, const std::string &indent)
{
    const std::string formatted = formatJavaDocComment(doc, indent);
    if (!formatted.empty())
        target["docComment"] = formatted;
}

static std::string formatJavaDocComment(const std::string &doc, const std::string &indent)
{
    auto lines = splitDocLines(doc);
    if (lines.empty())
        return {};

    std::ostringstream formatted;
    formatted << indent << "/**\n";
    for (const auto &line : lines)
    {
        formatted << indent << " *";
        if (!line.empty())
            formatted << ' ' << line;
        formatted << "\n";
    }
    formatted << indent << " */\n";
    return formatted.str();
}

static nlohmann::json toJavaFieldContext(const tpp::FieldDef &field, const std::string &docIndent)
{
    nlohmann::json fieldJson;
    fieldJson["name"] = field.name;
    fieldJson["type"] = codegen::typeKindToContext(*field.type);
    fieldJson["recursive"] = field.recursive;
    setDocComment(fieldJson, field.doc, docIndent);
    return fieldJson;
}

static nlohmann::json toJavaStructContext(
    const tpp::StructDef &structDef,
    const std::string &typeDocIndent,
    const std::string &fieldDocIndent)
{
    nlohmann::json structJson;
    structJson["name"] = structDef.name;
    structJson["fields"] = nlohmann::json::array();
    for (const auto &field : structDef.fields)
        structJson["fields"].push_back(toJavaFieldContext(field, fieldDocIndent));
    setDocComment(structJson, structDef.doc, typeDocIndent);
    return structJson;
}

static nlohmann::json toJavaVariantContext(const tpp::VariantDef &variant, const std::string &docIndent)
{
    nlohmann::json variantJson;
    variantJson["tag"] = variant.tag;
    if (variant.payload)
        variantJson["payload"] = codegen::typeKindToContext(*variant.payload);
    setDocComment(variantJson, variant.doc, docIndent);
    return variantJson;
}

static nlohmann::json toJavaEnumContext(
    const tpp::EnumDef &enumDef,
    const std::string &typeDocIndent,
    const std::string &variantDocIndent)
{
    nlohmann::json enumJson;
    enumJson["name"] = enumDef.name;
    enumJson["variants"] = nlohmann::json::array();
    for (const auto &variant : enumDef.variants)
        enumJson["variants"].push_back(toJavaVariantContext(variant, variantDocIndent));
    setDocComment(enumJson, enumDef.doc, typeDocIndent);
    return enumJson;
}

static nlohmann::json buildJavaSourceInput(const tpp::IR &ir, bool nested)
{
    nlohmann::json inputJson;
    inputJson["enums"] = nlohmann::json::array();
    inputJson["structs"] = nlohmann::json::array();

    const std::string typeDocIndent = nested ? "    " : "";
    const std::string memberDocIndent = nested ? "        " : "    ";

    for (const auto &enumDef : ir.enums)
        inputJson["enums"].push_back(toJavaEnumContext(enumDef, typeDocIndent, memberDocIndent));
    for (const auto &structDef : ir.structs)
        inputJson["structs"].push_back(toJavaStructContext(structDef, typeDocIndent, memberDocIndent));

    return inputJson;
}

enum class Mode
{
    Source,
    Runtime,
    RuntimeShared,
    Bundle
};

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
    static const std::map<std::string, Mode> commands = {
        {"source", Mode::Source},
        {"runtime", Mode::Runtime},
        {"runtime-shared", Mode::RuntimeShared},
        {"bundle", Mode::Bundle},
    };
    auto cli = codegen::parseBackendCommandLine(argc, argv, commands, printUsage);

    if (cli.mode == Mode::RuntimeShared || cli.mode == Mode::Runtime)
    {
        std::cout << codegen::render_java_shared_runtime() << std::endl;
        return EXIT_SUCCESS;
    }

    std::string namespaceName = cli.namespaceName;
    tpp::IR ir = codegen::loadIRInputOrExit(cli.inputFile);

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
