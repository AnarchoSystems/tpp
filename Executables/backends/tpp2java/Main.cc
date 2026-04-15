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
#include <fstream>
#include <iostream>
#include <sstream>

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR
// ═══════════════════════════════════════════════════════════════════════════════

static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::string &namespaceName)
{
    codegen::BuildFunctionsConfig bfCfg;
    bfCfg.nullLiteral = "null";
    bfCfg.staticModifier = "static ";
    bfCfg.callNeedsTry = false;
    return codegen::buildFunctionsContext(ir, functionPrefix, namespaceName, bfCfg);
}

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
    std::string inputFile;
    std::string namespaceName;
    bool runtimeMode = false;
    bool sharedRuntimeMode = false;
    bool bundleMode = false;
    bool externalRuntime = false;

    // Find the subcommand (first non-option argument)
    int cmdIndex = 0;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { printUsage(); return EXIT_SUCCESS; }
        if (arg[0] != '-') { cmdIndex = i; break; }
        if (arg == "-ns" || arg == "--input") ++i;  // skip value
    }

    if (cmdIndex == 0)
    {
        printUsage();
        return EXIT_FAILURE;
    }

    std::string cmd = argv[cmdIndex];
    if (cmd == "source") { runtimeMode = false; }
    else if (cmd == "runtime") { runtimeMode = true; }
    else if (cmd == "runtime-shared") { sharedRuntimeMode = true; }
    else if (cmd == "bundle") { bundleMode = true; }
    else { std::cerr << "Unknown command: " << cmd << "\nUse -h for usage info.\n"; return EXIT_FAILURE; }

    // Parse options (skip subcommand)
    for (int i = 1; i < argc; ++i)
    {
        if (i == cmdIndex) continue;
        std::string arg = argv[i];
        if (arg == "--input")
        {
            if (i + 1 >= argc) { std::cerr << "--input requires a file argument\n"; return EXIT_FAILURE; }
            inputFile = argv[++i];
        }
        else if (arg == "-ns")
        {
            if (i + 1 >= argc) { std::cerr << "-ns requires a name argument\n"; return EXIT_FAILURE; }
            namespaceName = argv[++i];
        }
        else if (arg == "--extern-runtime")
        {
            externalRuntime = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\nUse -h for usage info.\n";
            return EXIT_FAILURE;
        }
    }

    if (sharedRuntimeMode)
    {
        std::cout << codegen::reindent(codegen::render_java_shared_runtime(), 4) << std::endl;
        return EXIT_SUCCESS;
    }

    // Read IR JSON
    std::string inputJsonStr;
    if (!inputFile.empty())
    {
        std::ifstream f(inputFile);
        if (!f.is_open()) { std::cerr << "Could not open: " << inputFile << "\n"; return EXIT_FAILURE; }
        inputJsonStr.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    else
    {
        inputJsonStr.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    }

    tpp::IR ir;
    try { ir = nlohmann::json::parse(inputJsonStr).get<tpp::IR>(); }
    catch (const std::exception &e) { std::cerr << "Failed to parse IR JSON: " << e.what() << "\n"; return EXIT_FAILURE; }

    // Function prefix: "render_" when no namespace, "" when namespace set
    std::string functionPrefix = namespaceName.empty() ? "render_" : "";

    if (runtimeMode)
    {
        auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
        std::cout << codegen::reindent(codegen::render_java_runtime(funcCtx), 4) << std::endl;
        return EXIT_SUCCESS;
    }

    // Build codegen context and render types
    codegen::CodegenInput ctx = codegen::buildCodegenInput(ir);

    // Generate rendering functions from instruction IR
    auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
    if (externalRuntime)
        funcCtx.externalRuntime = true;

    if (bundleMode)
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
