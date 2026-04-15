// tpp2swift — generates Swift source from tpp intermediate representation.
//
// Usage:
//   tpp2swift <command> [options]
//
// Commands:
//   source          Generate Swift types + rendering functions
//   runtime         Generate only the standalone runtime helpers file
//   runtime-shared  Generate only the shared runtime helpers/types
//
// Options:
//   --input <file>    Read IR JSON from file instead of stdin
//   -ns <name>        Wrap generated code in an enum namespace
//   --extern-runtime  Suppress inlining runtime helpers in source output

#include <tpp/IR.h>
#include <CodegenHelpers.h>
#include "swift_defs_functions.h"
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
        ir,
        functionPrefix,
        namespaceName,
        codegen::BuildFunctionsConfig::forSwift(namespaceName, !ir.policies.empty()));
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
                 "  source          Generate Swift types + rendering functions\n"
                 "  runtime         Generate only the standalone runtime helpers file\n"
                 "  runtime-shared  Generate only the shared runtime helpers/types\n"
                 "\n"
                 "Options:\n"
                 "  --input <file>    Read IR JSON from file instead of stdin\n"
                 "  -ns <name>        Wrap generated code in an enum namespace\n"
                 "  --extern-runtime  Suppress inlining runtime helpers in source output\n"
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

    if (cli.mode == Mode::RuntimeShared)
    {
        std::cout << codegen::render_swift_shared_runtime();
        return EXIT_SUCCESS;
    }

    std::string namespaceName = cli.namespaceName;
    tpp::IR ir = codegen::loadIRInputOrExit(cli.inputFile);

    // Function prefix: "render_" when no namespace, "" when namespace set
    std::string functionPrefix = namespaceName.empty() ? "render_" : "";

    if (cli.mode == Mode::Runtime)
    {
        auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
        std::cout << codegen::render_swift_runtime(funcCtx);
        return EXIT_SUCCESS;
    }

    // Build codegen context and render types
    codegen::CodegenInput ctx = codegen::buildCodegenInput(ir);
    if (namespaceName.empty())
        std::cout << codegen::render_swift_source(ctx) << '\n';
    else
        std::cout << codegen::render_swift_namespaced_source(ctx, namespaceName) << '\n';

    // Generate rendering functions from instruction IR
    auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
    if (cli.externalRuntime)
        funcCtx.externalRuntime = true;

    std::cout << codegen::render_swift_functions(funcCtx);

    return EXIT_SUCCESS;
}
