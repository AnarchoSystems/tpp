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

static std::string formatSwiftDocComment(const std::string &doc, const std::string &indent)
{
    return codegen::formatLineDocComment(doc, indent, "///");
}

static nlohmann::json buildSwiftSourceInput(const tpp::IR &ir, bool nested)
{
    return codegen::buildSourceInput(ir, nested, formatSwiftDocComment);
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
