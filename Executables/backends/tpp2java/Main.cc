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
