// tpp2swift — generates Swift source from tpp intermediate representation.
//
// Usage:
//   tpp2swift <command> [options]
//
// Commands:
//   source          Generate Swift types + rendering functions (requires runtime-shared)
//   runtime         Alias for runtime-shared
//   runtime-shared  Generate the dedicated runtime helpers file
//
// Options:
//   --input <file>    Read IR JSON from file instead of stdin
//   -ns <name>        Wrap generated code in an enum namespace

#include <tpp/IR.h>
#include <CodegenHelpers.h>
#include "swift_defs_functions.h"
#include <iostream>
#include <map>
#include <sstream>

static std::string formatSwiftDocComment(const std::string &doc, const std::string &indent);

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR
// ═══════════════════════════════════════════════════════════════════════════════

static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::string &namespaceName)
{
    auto result = codegen::buildFunctionsContext(
        ir,
        functionPrefix,
        namespaceName,
        codegen::BuildFunctionsConfig::forSwift(namespaceName, !ir.policies.empty()));

    const std::string indent = namespaceName.empty() ? "" : "    ";
    for (auto &function : result.functions)
    {
        if (function.doc.has_value() && !function.doc->empty())
            function.doc = formatSwiftDocComment(*function.doc, indent);
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

static std::string formatSwiftDocComment(const std::string &doc, const std::string &indent)
{
    auto lines = splitDocLines(doc);
    if (lines.empty())
        return {};

    std::ostringstream formatted;
    for (const auto &line : lines)
    {
        formatted << indent << "///";
        if (!line.empty())
            formatted << ' ' << line;
        formatted << "\n";
    }
    return formatted.str();
}

static void setDocComment(nlohmann::json &target, const std::string &doc, const std::string &indent)
{
    const std::string formatted = formatSwiftDocComment(doc, indent);
    if (!formatted.empty())
        target["docComment"] = formatted;
}

static nlohmann::json toSwiftFieldContext(const tpp::FieldDef &field, const std::string &docIndent)
{
    nlohmann::json fieldJson;
    fieldJson["name"] = field.name;
    fieldJson["type"] = codegen::typeKindToContext(*field.type);
    fieldJson["recursive"] = field.recursive;
    setDocComment(fieldJson, field.doc, docIndent);
    return fieldJson;
}

static nlohmann::json toSwiftStructContext(
    const tpp::StructDef &structDef,
    const std::string &typeDocIndent,
    const std::string &fieldDocIndent)
{
    nlohmann::json structJson;
    structJson["name"] = structDef.name;
    structJson["fields"] = nlohmann::json::array();
    for (const auto &field : structDef.fields)
        structJson["fields"].push_back(toSwiftFieldContext(field, fieldDocIndent));
    setDocComment(structJson, structDef.doc, typeDocIndent);
    return structJson;
}

static nlohmann::json toSwiftVariantContext(const tpp::VariantDef &variant, const std::string &docIndent)
{
    nlohmann::json variantJson;
    variantJson["tag"] = variant.tag;
    if (variant.payload)
        variantJson["payload"] = codegen::typeKindToContext(*variant.payload);
    setDocComment(variantJson, variant.doc, docIndent);
    return variantJson;
}

static nlohmann::json toSwiftEnumContext(
    const tpp::EnumDef &enumDef,
    const std::string &typeDocIndent,
    const std::string &variantDocIndent)
{
    nlohmann::json enumJson;
    enumJson["name"] = enumDef.name;
    enumJson["variants"] = nlohmann::json::array();
    for (const auto &variant : enumDef.variants)
        enumJson["variants"].push_back(toSwiftVariantContext(variant, variantDocIndent));
    setDocComment(enumJson, enumDef.doc, typeDocIndent);
    return enumJson;
}

static nlohmann::json buildSwiftSourceInput(const tpp::IR &ir, bool nested)
{
    nlohmann::json inputJson;
    inputJson["enums"] = nlohmann::json::array();
    inputJson["structs"] = nlohmann::json::array();

    const std::string typeDocIndent = nested ? "    " : "";
    const std::string memberDocIndent = nested ? "        " : "    ";

    for (const auto &enumDef : ir.enums)
        inputJson["enums"].push_back(toSwiftEnumContext(enumDef, typeDocIndent, memberDocIndent));
    for (const auto &structDef : ir.structs)
        inputJson["structs"].push_back(toSwiftStructContext(structDef, typeDocIndent, memberDocIndent));

    return inputJson;
}

enum class Mode
{
    Source,
    Runtime,
    RuntimeShared
};

// ═══════════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════════

static void printUsage()
{
    std::cout << "Usage: tpp2swift <command> [options]\n"
                 "\n"
                 "Commands:\n"
                 "  source          Generate Swift types + rendering functions (requires runtime-shared at compile time)\n"
                 "  runtime         Alias for runtime-shared\n"
                 "  runtime-shared  Generate the dedicated runtime helpers file\n"
                 "\n"
                 "Options:\n"
                 "  --input <file>    Read IR JSON from file instead of stdin\n"
                 "  -ns <name>        Wrap generated code in an enum namespace\n"
                 "  -h, --help        Print this message\n";
}

int main(int argc, char *argv[])
{
    static const std::map<std::string, Mode> commands = {
        {"source", Mode::Source},
        {"runtime", Mode::Runtime},
        {"runtime-shared", Mode::RuntimeShared},
    };
    auto cli = codegen::parseBackendCommandLine(argc, argv, commands, printUsage);

    if (cli.mode == Mode::RuntimeShared || cli.mode == Mode::Runtime)
    {
        std::cout << codegen::render_swift_shared_runtime();
        return EXIT_SUCCESS;
    }

    std::string namespaceName = cli.namespaceName;
    tpp::IR ir = codegen::loadIRInputOrExit(cli.inputFile);

    // Function prefix: "render_" when no namespace, "" when namespace set
    std::string functionPrefix = namespaceName.empty() ? "render_" : "";

    auto sourceInput = buildSwiftSourceInput(ir, false);
    auto nestedSourceInput = buildSwiftSourceInput(ir, true);

    if (namespaceName.empty())
        std::cout << codegen::render_swift_source(sourceInput) << '\n';
    else
        std::cout << codegen::render_swift_namespaced_source(nestedSourceInput, namespaceName) << '\n';

    // Generate rendering functions from instruction IR
    auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);

    std::cout << codegen::render_swift_functions(funcCtx);

    return EXIT_SUCCESS;
}
