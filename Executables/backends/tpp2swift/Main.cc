// tpp2swift — generates Swift source from tpp intermediate representation.
//
// Usage:
//   tpp2swift [--input <ir_json>] [--namespace <name>] [-runtime] [--extern-runtime]
//       Reads IR JSON (from file or stdin), generates Swift types + rendering
//       functions to stdout.
//   -runtime: generate only the standalone runtime helpers file.
//   --extern-runtime: suppress inlining runtime helpers in the functions output.

#include <tpp/Compiler.h>
#include <tpp/Instruction.h>
#include <CodegenHelpers.h>
#include "defs.h"
#include <fstream>
#include <iostream>
#include <sstream>

// ═══════════════════════════════════════════════════════════════════════════════
// Build SwiftCodegenInput context from IR
// ═══════════════════════════════════════════════════════════════════════════════

static nlohmann::json buildCodegenInput(const tpp::IR &ir)
{
    nlohmann::json structs = nlohmann::json::array();
    bool needsBox = false;
    for (const auto &s : ir.types.structs)
    {
        nlohmann::json fields = nlohmann::json::array();
        for (const auto &f : s.fields)
        {
            bool isOpt = std::holds_alternative<std::shared_ptr<tpp::OptionalType>>(f.type);
            nlohmann::json innerTypeCtx = codegen::typeRefToContext(f.type);
            if (f.recursive && isOpt)
            {
                auto &opt = std::get<std::shared_ptr<tpp::OptionalType>>(f.type);
                innerTypeCtx = codegen::typeRefToContext(opt->innerType);
            }
            fields.push_back({
                {"name", f.name},
                {"type", codegen::typeRefToContext(f.type)},
                {"recursive", f.recursive},
                {"recursiveOptional", f.recursive && isOpt},
                {"innerType", innerTypeCtx}
            });
            if (f.recursive) needsBox = true;
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

    return {{"structs", structs}, {"enums", enums}, {"functions", functions}, {"needsBox", needsBox}};
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR
// ═══════════════════════════════════════════════════════════════════════════════

// ── Type name helpers (for function parameter signatures) ────────────────────

static std::string swiftType(const tpp::TypeRef &t)
{
    if (std::holds_alternative<tpp::StringType>(t)) return "String";
    if (std::holds_alternative<tpp::IntType>(t))    return "Int";
    if (std::holds_alternative<tpp::BoolType>(t))   return "Bool";
    if (auto *n = std::get_if<tpp::NamedType>(&t))  return n->name;
    if (auto *l = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        return "[" + swiftType((*l)->elementType) + "]";
    if (auto *o = std::get_if<std::shared_ptr<tpp::OptionalType>>(&t))
        return swiftType((*o)->innerType) + "?";
    return "Any";
}

// ── Field metadata for recursive/optional field path adjustments ─────────────

struct FieldMeta { bool recursive; bool optional; };

static std::map<std::string, FieldMeta> buildFieldMeta(const tpp::IR &ir)
{
    std::map<std::string, FieldMeta> result;
    for (const auto &s : ir.types.structs)
        for (const auto &f : s.fields)
            result[f.name] = {
                f.recursive,
                std::holds_alternative<std::shared_ptr<tpp::OptionalType>>(f.type)
            };
    return result;
}

static std::string swiftExprPath(
    const std::string &path,
    const tpp::TypeRef &resolvedType,
    const std::map<std::string, FieldMeta> &fieldMeta)
{
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return path;
    std::string fieldName = path.substr(dot + 1);
    auto it = fieldMeta.find(fieldName);
    if (it == fieldMeta.end() || !it->second.recursive) return path;
    if (it->second.optional)
        return path + "!.value";
    return path + ".value";
}

// ── Build the complete RenderFunctionsInput context ──────────────────────────

static nlohmann::json buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::string &namespaceName)
{
    bool hasPol = !ir.policies.all().empty();
    auto fieldMeta = buildFieldMeta(ir);

    codegen::ConvertConfig cfg;
    cfg.nullLiteral = "nil";
    cfg.functionPrefix = functionPrefix;
    cfg.callNeedsTry = hasPol;  // Only need try when functions throw (policies present)
    cfg.transformPath = [&fieldMeta](const std::string &p, const tpp::TypeRef &t) {
        return swiftExprPath(p, t, fieldMeta);
    };
    cfg.makeSpecSetupLines = [](int s, int numCols, const std::string &spec) {
        nlohmann::json lines = nlohmann::json::array();
        if (spec.empty()) return lines;
        if (spec.size() == 1)
        {
            lines.push_back(
                "_spec" + std::to_string(s)
                + " = [Character](repeating: \""
                + std::string(1, spec[0])
                + "\", count: " + std::to_string(numCols) + ")");
        }
        else
        {
            for (size_t ci = 0; ci < spec.size() && ci < (size_t)numCols; ++ci)
                lines.push_back(
                    "_spec" + std::to_string(s) + "["
                    + std::to_string(ci) + "] = \""
                    + std::string(1, spec[ci]) + "\"");
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
            paramsStr += "_ " + fn.params[i].name + ": " + swiftType(fn.params[i].type);
            argsPassStr += fn.params[i].name;
        }

        std::string paramsStrWithPolicy = paramsStr;
        if (hasPol)
        {
            if (!paramsStrWithPolicy.empty()) paramsStrWithPolicy += ", ";
            paramsStrWithPolicy += "_ _policy: String";
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
        {"policies", hasPol ? codegen::buildPolicyContext(ir, "nil") : nlohmann::json::array()},
        {"functionPrefix", functionPrefix},
        {"staticModifier", namespaceName.empty() ? "" : "static "}
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
    std::cout << "Usage: tpp2swift [options]\n"
                 "Options:\n"
                 "  --input <file>       Read IR JSON from file instead of stdin\n"
                 "  -ns, --namespace <name>  Wrap generated code in an enum namespace\n"
                 "  -runtime, --runtime  Generate only the standalone runtime helpers file\n"
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
        // Generate only standalone runtime helpers
        nlohmann::json funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
        tpp::FunctionSymbol rtFn;
        std::string error;
        if (!templateIR.get_function("render_swift_runtime", rtFn, error))
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
        std::cout << codegen::reindent(rtRendered) << std::endl;
        return EXIT_SUCCESS;
    }

    tpp::FunctionSymbol fn;
    std::string error;
    if (!templateIR.get_function("render_swift_source", fn, error))
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

    std::cout << rendered << '\n';

    // Generate rendering functions from instruction IR
    nlohmann::json funcCtx = buildFunctionsContext(ir, functionPrefix, namespaceName);
    if (externalRuntime)
        funcCtx["externalRuntime"] = true;

    tpp::FunctionSymbol funcFn;
    if (!templateIR.get_function("render_swift_functions", funcFn, error))
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

    std::cout << codegen::reindent(funcRendered);

    return EXIT_SUCCESS;
}
