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
#include <sstream>

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
// Re-indent generated code based on brace nesting
// ═══════════════════════════════════════════════════════════════════════════════

/// Re-indent code based on `{` / `}` nesting.
/// Handles braces inside string literals (won't count them).
inline std::string reindent(const std::string &code, int indentWidth = 4)
{
    std::string result;
    result.reserve(code.size() + code.size() / 8);
    int level = 0;
    std::istringstream iss(code);
    std::string line;
    while (std::getline(iss, line))
    {
        // Trim leading whitespace
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
        {
            result += '\n';
            continue;
        }
        std::string trimmed = line.substr(first);

        // Count leading '}' (possibly separated by spaces)
        size_t leadingCloses = 0;
        for (size_t i = 0; i < trimmed.size(); ++i)
        {
            if (trimmed[i] == '}') ++leadingCloses;
            else if (trimmed[i] == ' ' || trimmed[i] == '\t') continue;
            else break;
        }

        // Decrement level for leading closes
        level -= (int)leadingCloses;
        if (level < 0) level = 0;

        // Output the line at current level
        result.append(level * indentWidth, ' ');
        result += trimmed;
        result += '\n';

        // Count braces outside string literals
        int opens = 0, closes = 0;
        bool inStr = false;
        for (size_t i = 0; i < trimmed.size(); ++i)
        {
            char c = trimmed[i];
            if (inStr)
            {
                if (c == '\\') { ++i; continue; }
                if (c == '"') inStr = false;
            }
            else
            {
                if (c == '"') inStr = true;
                else if (c == '{') ++opens;
                else if (c == '}') ++closes;
            }
        }

        // Adjust level: we already subtracted leading closes
        level += opens - (int)(closes - leadingCloses);
        if (level < 0) level = 0;
    }
    // Strip trailing newline if input didn't end with one
    if (!result.empty() && result.back() == '\n' &&
        (code.empty() || code.back() != '\n'))
        result.pop_back();
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shared instruction IR → template context conversion
// ═══════════════════════════════════════════════════════════════════════════════

/// Extract element type from a collection's ListType.
inline tpp::TypeRef getElemType(const tpp::TypeRef &t)
{
    if (auto *l = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        return (*l)->elementType;
    return tpp::TypeRef{tpp::StringType{}};
}

/// Split instructions at AlignCellInstr boundaries into separate cell bodies.
inline std::vector<std::vector<tpp::Instruction>> splitCells(
    const std::vector<tpp::Instruction> &body)
{
    std::vector<std::vector<tpp::Instruction>> cells;
    cells.emplace_back();
    for (const auto &instr : body)
    {
        if (std::holds_alternative<tpp::AlignCellInstr>(instr))
            cells.emplace_back();
        else
            cells.back().push_back(instr);
    }
    return cells;
}

/// Sanitize a policy tag into a valid identifier (replace non-alnum with _).
inline std::string sanitizeIdentifier(const std::string &tag);

/// Check if any converted instruction body uses runtime policy dispatch.
inline bool bodyHasRuntimePolicy(const nlohmann::json &body)
{
    for (const auto &instr : body)
    {
        for (const auto &[key, val] : instr.items())
        {
            if (key == "EmitExpr" && val.value("useRuntimePolicy", false))
                return true;
            // Recurse into block bodies (For, If, Switch, etc.)
            if (val.is_object())
            {
                if (val.contains("body") && bodyHasRuntimePolicy(val["body"]))
                    return true;
                if (val.contains("thenBody") && bodyHasRuntimePolicy(val["thenBody"]))
                    return true;
                if (val.contains("elseBody") && bodyHasRuntimePolicy(val["elseBody"]))
                    return true;
                if (val.contains("cases"))
                    for (const auto &c : val["cases"])
                        if (c.contains("body") && bodyHasRuntimePolicy(c["body"]))
                            return true;
                if (val.contains("cellBodies"))
                    for (const auto &cb : val["cellBodies"])
                        if (bodyHasRuntimePolicy(cb))
                            return true;
            }
        }
    }
    return false;
}

/// Configuration for language-specific aspects of instruction conversion.
struct ConvertConfig
{
    /// Null literal for the target language ("nil" for Swift, "null" for Java).
    std::string nullLiteral = "null";

    /// Prefix for function names in generated code (e.g. "render_" or "").
    std::string functionPrefix = "render_";

    /// Whether policy-using call sites need `try` (Swift only).
    bool callNeedsTry = false;

    /// Transform an expression path for language-specific needs
    /// (e.g., Swift Box<T> unwrapping appends ".value").
    /// Default: identity.
    std::function<std::string(const std::string &path, const tpp::TypeRef &type)>
        transformPath = [](const std::string &p, const tpp::TypeRef &) { return p; };

    /// Transform a call argument path. Used for dereferencing optional/recursive
    /// fields when passing them as function arguments (e.g., C++ *node.next).
    /// Default: falls back to transformPath.
    std::function<std::string(const std::string &path, const tpp::TypeRef &type)>
        transformCallArg;

    /// Generate alignment spec setup lines (language-specific syntax).
    std::function<nlohmann::json(int scopeId, int numCols, const std::string &alignSpec)>
        makeSpecSetupLines = [](int, int, const std::string &) { return nlohmann::json::array(); };

    /// Qualifier prefix for accessing named policy static members
    /// (e.g. "TppPolicy." for Swift/Java, "TppPolicy::" for C++).
    std::string policyQualifier = "TppPolicy.";

    /// The expression used for the "pure" (identity/no-op) policy object.
    std::string purePolicy = "TppPolicy.pure";
};

/// Build the condition expression string for an IfInstr.
inline std::string condExpr(const tpp::IfInstr &n, const std::string &nullLiteral)
{
    const auto &t = n.condExpr.resolvedType;
    const std::string &p = n.condExpr.path;
    if (std::holds_alternative<tpp::BoolType>(t))
        return n.negated ? ("!" + p) : p;
    if (nullLiteral.empty())
        return n.negated ? ("!" + p) : p;
    return n.negated ? (p + " == " + nullLiteral) : (p + " != " + nullLiteral);
}

/// Recursively convert an Instruction to template context JSON.
inline nlohmann::json convertInstruction(
    const tpp::Instruction &instr, const std::string &sb,
    const std::string &activePolicy, int &scope, const tpp::IR &ir,
    const ConvertConfig &cfg)
{
    return std::visit([&](auto &&arg) -> nlohmann::json
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, tpp::EmitInstr>)
        {
            return {{"Emit", {
                {"textLit", stringLiteral(arg.text)},
                {"sb", sb}
            }}};
        }
        else if constexpr (std::is_same_v<T, tpp::EmitExprInstr>)
        {
            std::string effPol = arg.policy.empty() ? activePolicy : arg.policy;
            std::string exprPath = cfg.transformPath(arg.expr.path, arg.expr.resolvedType);
            nlohmann::json emitJson = {
                {"expr", {{"path", exprPath},
                          {"type", typeRefToContext(arg.expr.resolvedType)}}},
                {"sb", sb},
                {"useRuntimePolicy", false}
            };
            if (effPol == "none")
            {
                // explicit opt-out — no policy at all
            }
            else if (!effPol.empty())
            {
                emitJson["staticPolicyLit"] = stringLiteral(effPol);
                emitJson["staticPolicyId"] = sanitizeIdentifier(effPol);
            }
            else if (!ir.policies.all().empty())
            {
                emitJson["useRuntimePolicy"] = true;
            }
            return {{"EmitExpr", emitJson}};
        }
        else if constexpr (std::is_same_v<T, tpp::AlignCellInstr>)
        {
            return {{"AlignCell", true}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::ForInstr>>)
        {
            int s = scope++;
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;
            auto elemType = getElemType(arg->collection.resolvedType);
            bool hasAlign = arg->hasAlign;

            nlohmann::json cellsJson = nlohmann::json::array();
            int numCols = 0;
            nlohmann::json specSetupLines = nlohmann::json::array();
            nlohmann::json bodyJson = nlohmann::json::array();

            if (hasAlign)
            {
                auto cells = splitCells(arg->body);
                numCols = (int)cells.size();
                for (int ci = 0; ci < numCols; ++ci)
                {
                    std::string cellSb = "_cell" + std::to_string(s)
                                       + "_" + std::to_string(ci);
                    nlohmann::json cellBody = nlohmann::json::array();
                    for (const auto &bi : cells[ci])
                        cellBody.push_back(convertInstruction(bi, cellSb, pol, scope, ir, cfg));
                    cellsJson.push_back({{"cellIndex", ci}, {"body", cellBody}});
                }
                specSetupLines = cfg.makeSpecSetupLines(s, numCols, arg->alignSpec);
            }
            else
            {
                std::string bodySb = arg->isBlock
                    ? ("_blk" + std::to_string(s)) : sb;
                for (const auto &bi : arg->body)
                    bodyJson.push_back(convertInstruction(bi, bodySb, pol, scope, ir, cfg));
            }

            return {{"For", {
                {"scopeId", s},
                {"collPath", arg->collection.path},
                {"elemType", typeRefToContext(elemType)},
                {"varName", arg->varName},
                {"hasEnum", !arg->enumeratorName.empty()},
                {"enumeratorName", arg->enumeratorName},
                {"body", bodyJson},
                {"hasSep", !arg->sep.empty()},
                {"sepLit", !arg->sep.empty()
                    ? stringLiteral(arg->sep) : ""},
                {"hasFollowed", !arg->followedBy.empty()},
                {"followedByLit", !arg->followedBy.empty()
                    ? stringLiteral(arg->followedBy) : ""},
                {"hasPreceded", !arg->precededBy.empty()},
                {"precededByLit", !arg->precededBy.empty()
                    ? stringLiteral(arg->precededBy) : ""},
                {"isBlock", arg->isBlock},
                {"insertCol", arg->insertCol},
                {"sb", sb},
                {"hasAlign", hasAlign},
                {"alignSpec", arg->alignSpec},
                {"cells", cellsJson},
                {"numCols", numCols},
                {"specSetupLines", specSetupLines}
            }}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::IfInstr>>)
        {
            std::string condStr = condExpr(*arg, cfg.nullLiteral);
            int thenScopeId = 0, elseScopeId = 0;
            std::string thenSb = sb, elseSb = sb;
            if (arg->isBlock)
            {
                thenScopeId = scope++;
                thenSb = "_blk" + std::to_string(thenScopeId);
                if (!arg->elseBody.empty())
                {
                    elseScopeId = scope++;
                    elseSb = "_blk" + std::to_string(elseScopeId);
                }
            }
            nlohmann::json thenBody = nlohmann::json::array();
            for (const auto &ti : arg->thenBody)
                thenBody.push_back(convertInstruction(ti, thenSb, activePolicy, scope, ir, cfg));
            nlohmann::json elseBody = nlohmann::json::array();
            for (const auto &ei : arg->elseBody)
                elseBody.push_back(convertInstruction(ei, elseSb, activePolicy, scope, ir, cfg));

            return {{"If", {
                {"condExprStr", condStr},
                {"thenBody", thenBody},
                {"elseBody", elseBody},
                {"hasElse", !arg->elseBody.empty()},
                {"isBlock", arg->isBlock},
                {"insertCol", arg->insertCol},
                {"sb", sb},
                {"thenScopeId", thenScopeId},
                {"elseScopeId", elseScopeId}
            }}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::SwitchInstr>>)
        {
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;

            // Look up the enum definition for variant index/recursive info
            const tpp::EnumDef *enumDef = nullptr;
            if (auto *nt = std::get_if<tpp::NamedType>(&arg->expr.resolvedType))
                for (const auto &e : ir.types.enums)
                    if (e.name == nt->name) { enumDef = &e; break; }

            nlohmann::json casesJson = nlohmann::json::array();
            bool first = true;
            for (const auto &c : arg->cases)
            {
                int caseScopeId = 0;
                std::string caseSb = sb;
                if (arg->isBlock)
                {
                    caseScopeId = scope++;
                    caseSb = "_blk" + std::to_string(caseScopeId);
                }
                nlohmann::json caseBody = nlohmann::json::array();
                for (const auto &bi : c.body)
                    caseBody.push_back(convertInstruction(bi, caseSb, pol, scope, ir, cfg));

                // Find variant index and recursive info from enum definition
                int variantIndex = -1;
                bool isRecursivePayload = false;
                if (enumDef)
                {
                    for (int vi = 0; vi < (int)enumDef->variants.size(); ++vi)
                    {
                        if (enumDef->variants[vi].tag == c.tag)
                        {
                            variantIndex = vi;
                            isRecursivePayload = enumDef->variants[vi].recursive
                                                 && enumDef->variants[vi].payload.has_value();
                            break;
                        }
                    }
                }

                nlohmann::json caseJson = {
                    {"tag", c.tag},
                    {"tagLit", stringLiteral(c.tag)},
                    {"bindingName", c.bindingName},
                    {"hasBinding", !c.bindingName.empty()
                                   && c.payloadType.has_value()},
                    {"body", caseBody},
                    {"scopeId", caseScopeId},
                    {"isFirst", first},
                    {"variantIndex", variantIndex},
                    {"isRecursivePayload", isRecursivePayload}
                };
                if (c.payloadType.has_value())
                    caseJson["payloadType"] =
                        typeRefToContext(*c.payloadType);
                casesJson.push_back(caseJson);
                first = false;
            }

            // Extract the enum type name from the resolved type
            std::string enumTypeName;
            if (auto *nt = std::get_if<tpp::NamedType>(&arg->expr.resolvedType))
                enumTypeName = nt->name;

            return {{"Switch", {
                {"exprPath", arg->expr.path},
                {"enumTypeName", enumTypeName},
                {"cases", casesJson},
                {"isBlock", arg->isBlock},
                {"insertCol", arg->insertCol},
                {"sb", sb}
            }}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::CallInstr>>)
        {
            auto callTransform = cfg.transformCallArg ? cfg.transformCallArg : cfg.transformPath;
            std::string argsStr;
            for (size_t i = 0; i < arg->arguments.size(); ++i)
            {
                if (i > 0) argsStr += ", ";
                argsStr += callTransform(arg->arguments[i].path,
                                         arg->arguments[i].resolvedType);
            }
            if (!ir.policies.all().empty())
            {
                if (!argsStr.empty()) argsStr += ", ";
                if (!activePolicy.empty() && activePolicy != "none")
                    argsStr += cfg.policyQualifier + sanitizeIdentifier(activePolicy);
                else if (activePolicy == "none")
                    argsStr += cfg.purePolicy;
                else
                    argsStr += "_policy";
            }
            return {{"Call", {
                {"functionName", cfg.functionPrefix + arg->functionName},
                {"argsStr", argsStr},
                {"sb", sb},
                {"needsTry", cfg.callNeedsTry && !ir.policies.all().empty()}
            }}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::RenderViaInstr>>)
        {
            int s = scope++;
            auto elemType = getElemType(arg->collection.resolvedType);

            std::vector<const tpp::InstructionFunction *> overloads;
            for (const auto &fn : ir.instructionFunctions)
                if (fn.name == arg->functionName)
                    overloads.push_back(&fn);

            bool isSingleOverload = overloads.size() <= 1;
            nlohmann::json overloadsJson = nlohmann::json::array();

            if (!isSingleOverload)
            {
                const tpp::EnumDef *enumDef = nullptr;
                if (auto *nt = std::get_if<tpp::NamedType>(&elemType))
                    for (const auto &e : ir.types.enums)
                        if (e.name == nt->name) { enumDef = &e; break; }

                if (enumDef)
                {
                    bool first = true;
                    for (const auto &v : enumDef->variants)
                    {
                        const tpp::InstructionFunction *match = nullptr;
                        for (auto *ovl : overloads)
                        {
                            if (ovl->params.size() == 1
                                && v.payload.has_value()
                                && ovl->params[0].type == *v.payload)
                            {
                                match = ovl;
                                break;
                            }
                        }
                        if (match)
                        {
                            nlohmann::json ovlJson = {
                                {"tag", v.tag},
                                {"tagLit", stringLiteral(v.tag)},
                                {"isFirst", first}
                            };
                            if (v.payload.has_value())
                                ovlJson["payloadType"] = typeRefToContext(*v.payload);
                            overloadsJson.push_back(ovlJson);
                            first = false;
                        }
                    }
                }
            }

            nlohmann::json rvia = {
                {"scopeId", s},
                {"collPath", arg->collection.path},
                {"elemType", typeRefToContext(elemType)},
                {"functionName", cfg.functionPrefix + arg->functionName},
                {"hasSep", !arg->sep.empty()},
                {"sepLit", !arg->sep.empty()
                    ? stringLiteral(arg->sep) : ""},
                {"hasFollowed", !arg->followedBy.empty()},
                {"followedByLit", !arg->followedBy.empty()
                    ? stringLiteral(arg->followedBy) : ""},
                {"hasPreceded", !arg->precededBy.empty()},
                {"precededByLit", !arg->precededBy.empty()
                    ? stringLiteral(arg->precededBy) : ""},
                {"sb", sb},
                {"isSingleOverload", isSingleOverload},
                {"overloads", overloadsJson},
                {"needsTry", cfg.callNeedsTry && !ir.policies.all().empty()}
            };

            if (!ir.policies.all().empty())
            {
                std::string pol = arg->policy.empty() ? activePolicy : arg->policy;
                if (!pol.empty() && pol != "none")
                    rvia["policyArg"] = cfg.policyQualifier + sanitizeIdentifier(pol);
                else if (pol == "none")
                    rvia["policyArg"] = cfg.purePolicy;
                else
                    rvia["policyArg"] = "_policy";
            }

            return {{"RenderVia", rvia}};
        }
        else
        {
            return nlohmann::json::object();
        }
    }, instr);
}

/// Sanitize a policy tag into a valid identifier (replace non-alnum with _).
inline std::string sanitizeIdentifier(const std::string &tag)
{
    std::string id;
    for (char c : tag)
        id += (std::isalnum(c) ? c : '_');
    return id;
}

/// Build the policy context JSON array.
inline nlohmann::json buildPolicyContext(const tpp::IR &ir, const std::string &nullLiteral)
{
    nlohmann::json policies = nlohmann::json::array();
    for (const auto &[tag_, pol] : ir.policies.all())
    {
        nlohmann::json pj;
        pj["tagLit"] = stringLiteral(pol.tag);
        pj["identifier"] = sanitizeIdentifier(pol.tag);
        pj["hasLength"] = pol.length.has_value();
        pj["minVal"] = pol.length.has_value() && pol.length->min.has_value()
                       ? std::to_string(*pol.length->min) : nullLiteral;
        pj["maxVal"] = pol.length.has_value() && pol.length->max.has_value()
                       ? std::to_string(*pol.length->max) : nullLiteral;
        pj["hasRejectIf"] = pol.rejectIf.has_value();
        pj["rejectIfRegexLit"] = pol.rejectIf.has_value()
                                 ? stringLiteral(pol.rejectIf->regex) : "";
        pj["rejectMsgLit"] = pol.rejectIf.has_value()
                             ? stringLiteral(pol.rejectIf->message) : "";

        nlohmann::json reqArr = nlohmann::json::array();
        for (const auto &r : pol.require)
            reqArr.push_back({
                {"regexLit", stringLiteral(r.regex)},
                {"replaceLit", !r.replace.empty()
                    ? stringLiteral(r.compiled_replace) : ""},
                {"hasReplace", !r.replace.empty()}
            });
        pj["require"] = reqArr;
        pj["hasRequire"] = !pol.require.empty();

        nlohmann::json replArr = nlohmann::json::array();
        for (const auto &r : pol.replacements)
            replArr.push_back({
                {"findLit", stringLiteral(r.find)},
                {"replaceLit", stringLiteral(r.replace)}
            });
        pj["replacements"] = replArr;
        pj["hasReplacements"] = !pol.replacements.empty();

        nlohmann::json ofArr = nlohmann::json::array();
        for (const auto &f : pol.outputFilter)
            ofArr.push_back({{"regexLit", stringLiteral(f.regex)}});
        pj["outputFilter"] = ofArr;
        pj["hasOutputFilter"] = !pol.outputFilter.empty();

        policies.push_back(pj);
    }
    return policies;
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
