// tpp2java — generates Java source from tpp intermediate representation.
//
// Usage:
//   tpp2java [--input <ir_json>]
//       Reads IR JSON (from file or stdin), generates Java types + function
//       stubs to stdout.

#include <tpp/Compiler.h>
#include <CodegenHelpers.h>
#include "defs.h"
#include <fstream>
#include <iostream>

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
// Main
// ═══════════════════════════════════════════════════════════════════════════════

static void printUsage()
{
    std::cout << "Usage: tpp2java [options]\n"
                 "Options:\n"
                 "  --input <file>  Read IR JSON from file instead of stdin\n"
                 "  -h, --help      Print this message\n";
}

int main(int argc, char *argv[])
{
    std::string inputFile;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { printUsage(); return EXIT_SUCCESS; }
        else if (arg == "--input")
        {
            if (i + 1 >= argc) { std::cerr << "--input requires a file argument\n"; return EXIT_FAILURE; }
            inputFile = argv[++i];
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
    return EXIT_SUCCESS;
}
