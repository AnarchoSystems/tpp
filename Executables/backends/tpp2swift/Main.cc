// tpp2swift — generates Swift source from tpp intermediate representation.
//
// Usage:
//   tpp2swift <command> [options]
//
// Commands:
//   source   Generate Swift types + rendering functions
//   runtime  Generate only the standalone runtime helpers file
//
// Options:
//   --input <file>    Read IR JSON from file instead of stdin
//   -ns <name>        Wrap generated code in an enum namespace
//   --extern-runtime  Suppress inlining runtime helpers in source output

#include <tpp/IR.h>
#include <CodegenHelpers.h>
#include "swift_defs_functions.h"
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
    bfCfg.nullLiteral = "nil";
    bfCfg.staticModifier = namespaceName.empty() ? "" : "static ";
    bfCfg.callNeedsTry = !ir.policies.empty();
    return codegen::buildFunctionsContext(ir, functionPrefix, namespaceName, bfCfg);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════════

static void printUsage()
{
    std::cout << "Usage: tpp2swift <command> [options]\n"
                 "\n"
                 "Commands:\n"
                 "  source   Generate Swift types + rendering functions\n"
                 "  runtime  Generate only the standalone runtime helpers file\n"
                 "\n"
                 "Options:\n"
                 "  --input <file>    Read IR JSON from file instead of stdin\n"
                 "  -ns <name>        Wrap generated code in an enum namespace\n"
                 "  --extern-runtime  Suppress inlining runtime helpers in source output\n"
                 "  -h, --help        Print this message\n";
}

int main(int argc, char *argv[])
{
    std::string inputFile;
    std::string namespaceName;
    bool runtimeMode = false;
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
        std::cout << codegen::render_swift_runtime(funcCtx);
        return EXIT_SUCCESS;
    }

    // Build codegen context and render types
    codegen::CodegenInput ctx = codegen::buildCodegenInput(ir);
    std::cout << codegen::render_swift_source(ctx) << '\n';

    // Generate rendering functions from instruction IR
    auto funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
    if (externalRuntime)
        funcCtx.externalRuntime = true;

    std::cout << codegen::render_swift_functions(funcCtx);

    return EXIT_SUCCESS;
}
