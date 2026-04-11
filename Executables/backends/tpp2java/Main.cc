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
// Build JavaCodegenInput context from IR
// ═══════════════════════════════════════════════════════════════════════════════

static nlohmann::json buildCodegenInput(const tpp::IR &ir)
{
    nlohmann::json structs = nlohmann::json::array();
    for (const auto &s : ir.types.structs)
    {
        nlohmann::json fields = nlohmann::json::array();
        for (const auto &f : s.fields)
        {
            fields.push_back({
                {"name", f.name},
                {"type", codegen::typeRefToContext(f.type)},
                {"recursive", f.recursive}
            });
        }
        structs.push_back({{"name", s.name}, {"fields", fields}});
    }

    nlohmann::json enums = nlohmann::json::array();
    for (const auto &e : ir.types.enums)
    {
        nlohmann::json variants = nlohmann::json::array();
        bool hasRecursive = false;
        for (const auto &v : e.variants)
        {
            nlohmann::json variant = {{"tag", v.tag}, {"recursive", v.recursive}};
            if (v.payload.has_value())
                variant["payload"] = codegen::typeRefToContext(*v.payload);
            variants.push_back(variant);
            if (v.recursive) hasRecursive = true;
        }
        enums.push_back({
            {"name", e.name},
            {"variants", variants},
            {"hasRecursiveVariants", hasRecursive}
        });
    }

    nlohmann::json functions = nlohmann::json::array();
    for (const auto &fn : ir.functions)
    {
        nlohmann::json params = nlohmann::json::array();
        for (const auto &p : fn.params)
        {
            params.push_back({
                {"name", p.name},
                {"type", codegen::typeRefToContext(p.type)}
            });
        }
        functions.push_back({{"name", fn.name}, {"params", params}});
    }

    return {{"structs", structs}, {"enums", enums}, {"functions", functions}};
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR
// ═══════════════════════════════════════════════════════════════════════════════

// ── Type name helpers (for function parameter signatures) ────────────────────

static std::string javaBoxedType(const tpp::TypeRef &t);

static std::string javaType(const tpp::TypeRef &t)
{
    if (std::holds_alternative<tpp::StringType>(t)) return "String";
    if (std::holds_alternative<tpp::IntType>(t))    return "int";
    if (std::holds_alternative<tpp::BoolType>(t))   return "boolean";
    if (auto *n = std::get_if<tpp::NamedType>(&t))  return n->name;
    if (auto *l = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        return "java.util.List<" + javaBoxedType((*l)->elementType) + ">";
    if (auto *o = std::get_if<std::shared_ptr<tpp::OptionalType>>(&t))
        return javaBoxedType((*o)->innerType);
    return "Object";
}

static std::string javaBoxedType(const tpp::TypeRef &t)
{
    if (std::holds_alternative<tpp::StringType>(t)) return "String";
    if (std::holds_alternative<tpp::IntType>(t))    return "Integer";
    if (std::holds_alternative<tpp::BoolType>(t))   return "Boolean";
    if (auto *n = std::get_if<tpp::NamedType>(&t))  return n->name;
    if (auto *l = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        return "java.util.List<" + javaBoxedType((*l)->elementType) + ">";
    if (auto *o = std::get_if<std::shared_ptr<tpp::OptionalType>>(&t))
        return javaBoxedType((*o)->innerType);
    return "Object";
}

// ── Build the complete RenderFunctionsInput context ──────────────────────────

static nlohmann::json buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::string &namespaceName)
{
    bool hasPol = !ir.policies.all().empty();

    codegen::ConvertConfig cfg;
    cfg.nullLiteral = "null";
    cfg.functionPrefix = functionPrefix;
    cfg.callNeedsTry = false;  // Java: checked exceptions propagate without 'try'
    cfg.makeSpecSetupLines = [](int s, int numCols, const std::string &spec) {
        nlohmann::json lines = nlohmann::json::array();
        if (spec.empty()) return lines;
        if (spec.size() == 1)
        {
            lines.push_back(
                "java.util.Arrays.fill(_spec" + std::to_string(s)
                + ", '" + std::string(1, spec[0]) + "');");
        }
        else
        {
            for (size_t ci = 0; ci < spec.size() && ci < (size_t)numCols; ++ci)
                lines.push_back(
                    "_spec" + std::to_string(s) + "["
                    + std::to_string(ci) + "] = '"
                    + std::string(1, spec[ci]) + "';");
        }
        return lines;
    };

    nlohmann::json functions = nlohmann::json::array();
    for (const auto &fn : ir.instructionFunctions)
    {
        std::string paramsStr;
        std::string argsPassStr;
        for (size_t i = 0; i < fn.params.size(); ++i)
        {
            if (i > 0) { paramsStr += ", "; argsPassStr += ", "; }
            paramsStr += javaType(fn.params[i].type) + " " + fn.params[i].name;
            argsPassStr += fn.params[i].name;
        }

        std::string paramsStrWithPolicy = paramsStr;
        if (hasPol)
        {
            if (!paramsStrWithPolicy.empty()) paramsStrWithPolicy += ", ";
            paramsStrWithPolicy += "String _policy";
            if (!argsPassStr.empty()) argsPassStr += ", ";
            argsPassStr += "\"\"";
        }

        int scope = 0;
        nlohmann::json body = nlohmann::json::array();
        for (const auto &instr : fn.body)
            body.push_back(codegen::convertInstruction(instr, "_sb", fn.policy, scope, ir, cfg));

        functions.push_back({
            {"name", fn.name},
            {"paramsStr", paramsStr},
            {"paramsStrWithPolicy", paramsStrWithPolicy},
            {"argsPassStr", argsPassStr},
            {"body", body}
        });
    }

    bool needsDispatch = false;
    if (hasPol)
        for (const auto &f : functions)
            if (codegen::bodyHasRuntimePolicy(f["body"])) { needsDispatch = true; break; }

    nlohmann::json result = {
        {"functions", functions},
        {"hasPolicies", hasPol},
        {"needsApplyDispatch", needsDispatch},
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
    nlohmann::json ctx = buildCodegenInput(ir);

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
