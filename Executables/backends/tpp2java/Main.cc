// tpp2java — generates Java source from tpp intermediate representation.
//
// Usage:
//   tpp2java <command> [options]
//
// Commands:
//   source          Generate Java types + rendering functions
//   runtime         Generate only the standalone runtime helpers class
//   runtime-shared  Generate only the shared runtime helpers/types
//   bundle          Generate a namespaced Java wrapper with types + functions
//
// Options:
//   --input <file>    Read IR JSON from file instead of stdin
//   -ns <name>        Use as the Java class name (default: Functions)
//   --extern-runtime  Suppress inlining runtime helpers in source output

#include <tpp/IR.h>
#include <CodegenHelpers.h>
#include "java_defs_functions.h"
#include <iostream>
#include <map>

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR
// ═══════════════════════════════════════════════════════════════════════════════

static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::string &namespaceName)
{
    return codegen::buildFunctionsContext(
        ir, functionPrefix, namespaceName, codegen::BuildFunctionsConfig::forJava());
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
                 "  source          Generate Java types + rendering functions\n"
                 "  runtime         Generate only the standalone runtime helpers class\n"
                 "  runtime-shared  Generate only the shared runtime helpers/types\n"
                 "  bundle          Generate a namespaced Java wrapper with types + functions\n"
                 "\n"
                 "Options:\n"
                 "  --input <file>    Read IR JSON from file instead of stdin\n"
                 "  -ns <name>        Use as the Java class name (default: Functions)\n"
                 "  --extern-runtime  Suppress inlining runtime helpers in source output\n"
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

    if (cli.mode == Mode::RuntimeShared)
    {
        std::cout << codegen::reindent(codegen::render_java_shared_runtime(), 4) << std::endl;
        return EXIT_SUCCESS;
    }

    std::string namespaceName = cli.namespaceName;
    tpp::IR ir = codegen::loadIRInputOrExit(cli.inputFile);

    // Function prefix: "render_" when no namespace, "" when namespace set
    std::string functionPrefix = namespaceName.empty() ? "render_" : "";

    if (cli.mode == Mode::Runtime)
    {
        auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
        std::cout << codegen::reindent(codegen::render_java_runtime(funcCtx), 4) << std::endl;
        return EXIT_SUCCESS;
    }

    // Build codegen context and render types
    codegen::CodegenInput ctx = codegen::buildCodegenInput(ir);

    // Generate rendering functions from instruction IR
    auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
    if (cli.externalRuntime)
        funcCtx.externalRuntime = true;

    if (cli.mode == Mode::Bundle)
    {
        if (namespaceName.empty())
        {
            std::cerr << "bundle requires -ns <name>\n";
            return EXIT_FAILURE;
        }
        std::cout << codegen::reindent(codegen::render_java_bundle(ctx, funcCtx), 4) << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << codegen::render_java_source(ctx);

    std::cout << codegen::reindent(codegen::render_java_functions(funcCtx), 4);

    return EXIT_SUCCESS;
}
