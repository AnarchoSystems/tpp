// CodegenHelpers.h — shared helpers for tpp2java and tpp2swift code generators.
//
// Provides file utilities, test case compilation, and context-building
// functions for the template-based code generation pipeline.

#pragma once

#include <tpp/Compiler.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <algorithm>

namespace codegen {

// ═══════════════════════════════════════════════════════════════════════════════
// File / glob helpers
// ═══════════════════════════════════════════════════════════════════════════════

inline std::string readFile(const std::filesystem::path &path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open: " + path.string());
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

inline bool matchGlob(const std::string &pattern, const std::string &text)
{
    size_t pi = 0, ti = 0, starPos = std::string::npos, matchPos = 0;
    while (ti < text.size())
    {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti]))
            { ++pi; ++ti; }
        else if (pi < pattern.size() && pattern[pi] == '*')
            { starPos = pi++; matchPos = ti; }
        else if (starPos != std::string::npos)
            { pi = starPos + 1; ti = ++matchPos; }
        else
            return false;
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

inline std::vector<std::filesystem::path> expandPattern(const std::filesystem::path &baseDir,
                                                        const std::string &pattern)
{
    std::filesystem::path patPath(pattern);
    std::string filename = patPath.filename().string();
    std::filesystem::path parentDir = baseDir / patPath.parent_path();
    if (filename.find('*') == std::string::npos)
        return {baseDir / pattern};
    std::vector<std::filesystem::path> results;
    if (std::filesystem::is_directory(parentDir))
    {
        for (const auto &entry : std::filesystem::directory_iterator(parentDir))
        {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            if (name.size() < 4 || name.substr(name.size() - 4) != ".tpp") continue;
            if (matchGlob(filename, name))
                results.push_back(entry.path());
        }
        std::sort(results.begin(), results.end());
    }
    return results;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Compile a test case directory → tpp::IR + config
// ═══════════════════════════════════════════════════════════════════════════════

struct CompileResult
{
    tpp::IR ir;
    nlohmann::json config;      // parsed tpp-config.json
};

inline CompileResult compileTestCase(const std::filesystem::path &testDir)
{
    std::filesystem::path configPath = testDir / "tpp-config.json";
    nlohmann::json config = nlohmann::json::parse(readFile(configPath));

    struct FileEntry { std::filesystem::path path; bool isTypes; };
    std::vector<FileEntry> fileEntries;
    for (const auto &pattern : config.value("types", nlohmann::json::array()))
        for (const auto &p : expandPattern(testDir, pattern.get<std::string>()))
            fileEntries.push_back({p, true});
    for (const auto &pattern : config.value("templates", nlohmann::json::array()))
        for (const auto &p : expandPattern(testDir, pattern.get<std::string>()))
            fileEntries.push_back({p, false});

    tpp::Compiler compiler;
    std::vector<tpp::Diagnostic> diagnostics;
    for (const auto &fe : fileEntries)
    {
        std::string content = readFile(fe.path);
        if (fe.isTypes)
            compiler.add_types(content, diagnostics);
        else
            compiler.add_templates(content, diagnostics);
    }

    for (const auto &pe : config.value("replacement-policies", nlohmann::json::array()))
    {
        nlohmann::json pj = nlohmann::json::parse(readFile(testDir / pe.get<std::string>()));
        std::string err;
        if (!compiler.add_policy(pj, err))
            throw std::runtime_error(err);
    }

    tpp::IR ir;
    if (!compiler.compile(ir))
    {
        std::string errors;
        for (const auto &d : diagnostics)
            if (d.severity && *d.severity == tpp::DiagnosticSeverity::Error)
                errors += d.message + "\n";
        throw std::runtime_error("Compile failed:\n" + errors);
    }

    return {std::move(ir), std::move(config)};
}

// ═══════════════════════════════════════════════════════════════════════════════
// Type → template context helpers
// ═══════════════════════════════════════════════════════════════════════════════

/// Converts a tpp::TypeRef to the TypeKind JSON expected by the codegen
/// templates (enum variant encoding: {"Tag": payload}).
inline nlohmann::json typeRefToContext(const tpp::TypeRef &t)
{
    if (std::holds_alternative<tpp::StringType>(t)) return {{"Str", true}};
    if (std::holds_alternative<tpp::IntType>(t))    return {{"Int", true}};
    if (std::holds_alternative<tpp::BoolType>(t))   return {{"Bool", true}};
    if (auto *n = std::get_if<tpp::NamedType>(&t))  return {{"Named", n->name}};
    if (auto *l = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        return {{"List", typeRefToContext((*l)->elementType)}};
    if (auto *o = std::get_if<std::shared_ptr<tpp::OptionalType>>(&t))
        return {{"Optional", typeRefToContext((*o)->innerType)}};
    return {{"Str", true}};
}

/// Wraps a raw string in a language string literal with escaping.
/// Works identically for Java and Swift.
inline std::string stringLiteral(const std::string &s)
{
    std::string result = "\"";
    for (char c : s)
    {
        if (c == '"')       result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (c == '\0') result += "\\0";
        else result += c;
    }
    return result + "\"";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build the TestContext JSON consumed by the defs.tpp templates
// ═══════════════════════════════════════════════════════════════════════════════

inline nlohmann::json buildTestContext(
    const tpp::IR &ir,
    const nlohmann::json &config,
    const std::filesystem::path &testDir)
{
    std::string testName = testDir.filename().string();

    // Expected output
    std::string expected = readFile(testDir / "expected_output.txt");
    if (!expected.empty() && expected.back() == '\n')
        expected.pop_back();

    // Main function params
    tpp::FunctionSymbol mainFn;
    std::string error;
    if (!ir.get_function("main", mainFn, error))
        throw std::runtime_error(error);

    // Input from config
    nlohmann::json input;
    if (config.contains("previews") && !config["previews"].empty()
        && config["previews"][0].contains("input"))
        input = config["previews"][0]["input"];

    // ── Structs ──
    nlohmann::json structs = nlohmann::json::array();
    bool needsBox = false;
    for (const auto &s : ir.types.structs)
    {
        nlohmann::json fields = nlohmann::json::array();
        for (const auto &f : s.fields)
        {
            fields.push_back({
                {"name", f.name},
                {"type", typeRefToContext(f.type)},
                {"recursive", f.recursive}
            });
            if (f.recursive) needsBox = true;
        }
        structs.push_back({{"name", s.name}, {"fields", fields}});
    }

    // ── Enums ──
    nlohmann::json enums = nlohmann::json::array();
    for (const auto &e : ir.types.enums)
    {
        nlohmann::json variants = nlohmann::json::array();
        bool hasRecursive = false;
        for (const auto &v : e.variants)
        {
            nlohmann::json variant = {{"tag", v.tag}, {"recursive", v.recursive}};
            if (v.payload.has_value())
                variant["payload"] = typeRefToContext(*v.payload);
            variants.push_back(variant);
            if (v.recursive) hasRecursive = true;
        }
        enums.push_back({
            {"name", e.name},
            {"variants", variants},
            {"hasRecursiveVariants", hasRecursive}
        });
    }

    // ── Functions ──
    nlohmann::json functions = nlohmann::json::array();
    for (const auto &fn : ir.functions)
    {
        nlohmann::json fparams = nlohmann::json::array();
        for (const auto &p : fn.params)
        {
            fparams.push_back({
                {"name", p.name},
                {"type", typeRefToContext(p.type)},
                {"jsonValueLiteral", ""}
            });
        }
        functions.push_back({{"name", fn.name}, {"params", fparams}});
    }

    // ── Main function params with JSON value literals ──
    nlohmann::json params = nlohmann::json::array();
    for (size_t i = 0; i < mainFn.function.params.size(); ++i)
    {
        const auto &p = mainFn.function.params[i];
        nlohmann::json paramValue;
        if (input.is_array() && i < input.size())
            paramValue = input[i];
        else if (!input.is_array() && mainFn.function.params.size() == 1)
            paramValue = input;

        params.push_back({
            {"name", p.name},
            {"type", typeRefToContext(p.type)},
            {"jsonValueLiteral", stringLiteral(paramValue.dump())}
        });
    }

    return {
        {"testName", testName},
        {"structs", structs},
        {"enums", enums},
        {"functions", functions},
        {"params", params},
        {"expectedOutputLiteral", stringLiteral(expected)},
        {"needsBox", needsBox}
    };
}

} // namespace codegen
