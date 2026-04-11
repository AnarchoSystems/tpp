// tpp2java — generates Java source from tpp intermediate representation.
//
// Usage:
//   tpp2java [--input <ir_json>] [--namespace <name>] [-runtime] [--extern-runtime]
//       Reads IR JSON (from file or stdin), generates Java types + rendering
//       functions to stdout.
//   -runtime: generate only the standalone runtime helpers class.
//   --extern-runtime: suppress inlining runtime helpers in the functions output.

#include <tpp/Compiler.h>
#include <tpp/Instruction.h>
#include <CodegenHelpers.h>
#include "defs.h"
#include <fstream>
#include <iostream>
#include <sstream>

// ═══════════════════════════════════════════════════════════════════════════════
// Build CodegenInput context from IR (delegates to shared builder)
// ═══════════════════════════════════════════════════════════════════════════════

// Uses codegen::buildCodegenInput() from CodegenHelpers.h

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR
// ═══════════════════════════════════════════════════════════════════════════════

// ── Build the complete RenderFunctionsInput context ──────────────────────────

static nlohmann::json buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::string &namespaceName)
{
    bool hasPol = !ir.policies.all().empty();

    codegen::ConvertConfig cfg;
    cfg.functionPrefix = functionPrefix;
    cfg.callNeedsTry = false;  // Java: checked exceptions propagate without 'try'

    nlohmann::json functions = nlohmann::json::array();
    for (const auto &fn : ir.instructionFunctions)
    {
        nlohmann::json params = nlohmann::json::array();
        for (size_t i = 0; i < fn.params.size(); ++i)
        {
            params.push_back({
                {"name", fn.params[i].name},
                {"type", codegen::typeRefToContext(fn.params[i].type)}
            });
        }

        int scope = 0;
        nlohmann::json body = nlohmann::json::array();
        for (const auto &instr : fn.body)
            body.push_back(codegen::convertInstruction(instr, "_sb", fn.policy, scope, ir, cfg));

        functions.push_back({
            {"name", fn.name},
            {"params", params},
            {"body", body}
        });
    }

    nlohmann::json result = {
        {"functions", functions},
        {"hasPolicies", hasPol},
        {"policies", hasPol ? codegen::buildPolicyContext(ir, "null") : nlohmann::json::array()},
        {"functionPrefix", functionPrefix},
        {"staticModifier", "static "}  // Java always uses static methods
    };
    if (!namespaceName.empty())
        result["namespaceName"] = namespaceName;
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════════

static void printUsage()
{
    std::cout << "Usage: tpp2java [options]\n"
                 "Options:\n"
                 "  --input <file>       Read IR JSON from file instead of stdin\n"
                 "  -ns, --namespace <name>  Use as the Java class name (default: Functions)\n"
                 "  -runtime, --runtime  Generate only the standalone runtime helpers class\n"
                 "  --extern-runtime     Suppress inlining runtime helpers in functions output\n"
                 "  -h, --help           Print this message\n";
}

int main(int argc, char *argv[])
{
    std::string inputFile;
    std::string namespaceName;
    bool runtimeMode = false;
    bool externalRuntime = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { printUsage(); return EXIT_SUCCESS; }
        else if (arg == "--input")
        {
            if (i + 1 >= argc) { std::cerr << "--input requires a file argument\n"; return EXIT_FAILURE; }
            inputFile = argv[++i];
        }
        else if (arg == "-ns" || arg == "--namespace")
        {
            if (i + 1 >= argc) { std::cerr << arg << " requires a name argument\n"; return EXIT_FAILURE; }
            namespaceName = argv[++i];
        }
        else if (arg == "-runtime" || arg == "--runtime")
        {
            runtimeMode = true;
        }
        else if (arg == "--extern-runtime")
        {
            externalRuntime = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
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

    // Build codegen context
    nlohmann::json ctx = codegen::buildCodegenInput(ir);

    // Compile embedded templates
    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(2);
    compiler.add_types(types_content, diagnostics.emplace_back("defs.tpp.types").diagnostics);
    compiler.add_templates(template_content, diagnostics.emplace_back("defs.tpp").diagnostics);

    tpp::IR templateIR;
    if (!compiler.compile(templateIR))
    {
        for (const auto &msg : diagnostics)
            for (auto &d : msg.toGCCDiagnostics())
                std::cerr << d << std::endl;
        return EXIT_FAILURE;
    }

    if (runtimeMode)
    {
        // Generate only standalone runtime helpers class
        nlohmann::json funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
        tpp::FunctionSymbol rtFn;
        std::string error;
        if (!templateIR.get_function("render_java_runtime", rtFn, error))
        {
            std::cerr << "Template error: " << error << "\n";
            return EXIT_FAILURE;
        }
        std::string rtRendered;
        if (!rtFn.render(funcCtx, rtRendered, error))
        {
            std::cerr << "Render error: " << error << "\n";
            return EXIT_FAILURE;
        }
        std::cout << codegen::reindent(rtRendered, 4) << std::endl;
        return EXIT_SUCCESS;
    }

    tpp::FunctionSymbol fn;
    std::string error;
    if (!templateIR.get_function("render_java_source", fn, error))
    {
        std::cerr << "Template error: " << error << "\n";
        return EXIT_FAILURE;
    }

    std::string rendered;
    if (!fn.render(ctx, rendered, error))
    {
        std::cerr << "Render error: " << error << "\n";
        return EXIT_FAILURE;
    }

    std::cout << rendered;

    // Generate rendering functions from instruction IR
    nlohmann::json funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
    if (externalRuntime)
        funcCtx["externalRuntime"] = true;

    tpp::FunctionSymbol funcFn;
    if (!templateIR.get_function("render_java_functions", funcFn, error))
    {
        std::cerr << "Template error: " << error << "\n";
        return EXIT_FAILURE;
    }

    std::string funcRendered;
    if (!funcFn.render(funcCtx, funcRendered, error))
    {
        std::cerr << "Render error: " << error << "\n";
        return EXIT_FAILURE;
    }

    std::cout << codegen::reindent(funcRendered, 4);

    return EXIT_SUCCESS;
}
