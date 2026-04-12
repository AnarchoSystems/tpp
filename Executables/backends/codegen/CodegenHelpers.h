// CodegenHelpers.h — shared helpers for tpp2java and tpp2swift code generators.
//
// Provides file utilities, test case compilation, and context-building
// functions for the template-based code generation pipeline.

#pragma once

#include <tpp/Compiler.h>
#include "CodegenTypes.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
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

/// Converts a tpp::TypeRef to the codegen::TypeKind struct.
inline TypeKind typeRefToContext(const tpp::TypeRef &t)
{
    TypeKind result;
    if (std::holds_alternative<tpp::StringType>(t))
        result.value = TypeKind_Str{};
    else if (std::holds_alternative<tpp::IntType>(t))
        result.value = TypeKind_Int{};
    else if (std::holds_alternative<tpp::BoolType>(t))
        result.value = TypeKind_Bool{};
    else if (auto *n = std::get_if<tpp::NamedType>(&t))
        result.value.emplace<3>(n->name);
    else if (auto *l = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        result.value.emplace<4>(std::make_unique<TypeKind>(typeRefToContext((*l)->elementType)));
    else if (auto *o = std::get_if<std::shared_ptr<tpp::OptionalType>>(&t))
        result.value.emplace<5>(std::make_unique<TypeKind>(typeRefToContext((*o)->innerType)));
    else
        result.value = TypeKind_Str{};
    return result;
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

/// Build a PolicyRef from the active policy state.
inline std::optional<PolicyRef> makePolicyRef(
    const std::string &activePolicy, const tpp::IR &ir)
{
    if (ir.policies.all().empty()) return std::nullopt;
    if (!activePolicy.empty() && activePolicy != "none")
    {
        PolicyRef ref;
        ref.value = sanitizeIdentifier(activePolicy);
        return ref;
    }
    if (activePolicy == "none")
    {
        PolicyRef ref;
        ref.value = PolicyRef_Pure{};
        return ref;
    }
    PolicyRef ref;
    ref.value = PolicyRef_Runtime{};
    return ref;
}

/// Configuration for language-specific aspects of instruction conversion.
struct ConvertConfig
{
    /// Prefix for function names in generated code (e.g. "render_" or "").
    std::string functionPrefix = "render_";

    /// Whether policy-using call sites need `try` (Swift only).
    bool callNeedsTry = false;
};

/// Look up whether a field (by last dot-segment of path) is recursive/optional.
struct FieldLookup { bool recursive = false; bool optional = false; };
inline FieldLookup lookupField(const std::string &path, const tpp::IR &ir)
{
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return {};
    std::string fieldName = path.substr(dot + 1);
    for (const auto &s : ir.types.structs)
        for (const auto &f : s.fields)
            if (f.name == fieldName)
                return {f.recursive,
                        std::holds_alternative<std::shared_ptr<tpp::OptionalType>>(f.type)};
    return {};
}

/// Wrap a C++ path in (*...) when it traverses a unique_ptr field (recursive type).
inline std::string fixCppPath(const std::string &path, const tpp::IR &ir)
{
    if (lookupField(path, ir).recursive) return "(*" + path + ")";
    return path;
}

/// Recursively convert an Instruction to a codegen::Instruction struct.
inline Instruction convertInstruction(
    const tpp::Instruction &instr, const std::string &sb,
    const std::string &activePolicy, int &scope, const tpp::IR &ir,
    const ConvertConfig &cfg)
{
    return std::visit([&](auto &&arg) -> Instruction
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, tpp::EmitInstr>)
        {
            return {EmitData{stringLiteral(arg.text), sb}};
        }
        else if constexpr (std::is_same_v<T, tpp::EmitExprInstr>)
        {
            std::string effPol = arg.policy.empty() ? activePolicy : arg.policy;
            auto fl = lookupField(arg.expr.path, ir);

            EmitExprData data;
            data.expr = {
                arg.expr.path,
                std::make_unique<TypeKind>(typeRefToContext(arg.expr.resolvedType)),
                fl.recursive,
                fl.optional
            };
            data.sb = sb;
            data.useRuntimePolicy = false;

            if (effPol == "none")
            {
                // explicit opt-out — no policy at all
            }
            else if (!effPol.empty())
            {
                data.staticPolicyLit = stringLiteral(effPol);
                data.staticPolicyId = sanitizeIdentifier(effPol);
            }
            else if (!ir.policies.all().empty())
            {
                data.useRuntimePolicy = true;
            }
            return {std::move(data)};
        }
        else if constexpr (std::is_same_v<T, tpp::AlignCellInstr>)
        {
            return {Instruction_AlignCell{}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::ForInstr>>)
        {
            int s = scope++;
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;
            auto elemType = getElemType(arg->collection.resolvedType);
            bool hasAlign = arg->hasAlign;

            auto forData = std::make_unique<ForData>();
            forData->scopeId = s;
            forData->collPath = fixCppPath(arg->collection.path, ir);
            forData->elemType = std::make_unique<TypeKind>(typeRefToContext(elemType));
            forData->varName = arg->varName;
            forData->hasEnum = !arg->enumeratorName.empty();
            forData->enumeratorName = arg->enumeratorName;
            forData->hasSep = !arg->sep.empty();
            forData->sepLit = !arg->sep.empty() ? stringLiteral(arg->sep) : "";
            forData->hasFollowed = !arg->followedBy.empty();
            forData->followedByLit = !arg->followedBy.empty() ? stringLiteral(arg->followedBy) : "";
            forData->hasPreceded = !arg->precededBy.empty();
            forData->precededByLit = !arg->precededBy.empty() ? stringLiteral(arg->precededBy) : "";
            forData->isBlock = arg->isBlock;
            forData->insertCol = arg->insertCol;
            forData->sb = sb;
            forData->hasAlign = hasAlign;
            forData->alignSpec = arg->alignSpec;
            forData->singleAlignChar = arg->alignSpec.size() == 1;

            if (hasAlign)
            {
                auto cellGroups = splitCells(arg->body);
                forData->numCols = (int)cellGroups.size();
                auto cellsVec = std::make_unique<std::vector<AlignCellInfo>>();
                for (int ci = 0; ci < (int)cellGroups.size(); ++ci)
                {
                    std::string cellSb = "_cell" + std::to_string(s) + "_" + std::to_string(ci);
                    auto cellBody = std::make_unique<std::vector<Instruction>>();
                    for (const auto &bi : cellGroups[ci])
                        cellBody->push_back(convertInstruction(bi, cellSb, pol, scope, ir, cfg));
                    cellsVec->push_back({ci, std::move(cellBody)});
                }
                forData->cells = std::move(cellsVec);
                std::vector<std::string> specChars;
                for (char c : arg->alignSpec)
                    specChars.push_back(std::string(1, c));
                forData->alignSpecChars = std::move(specChars);
                forData->body = std::make_unique<std::vector<Instruction>>();
            }
            else
            {
                std::string bodySb = arg->isBlock ? ("_blk" + std::to_string(s)) : sb;
                auto bodyVec = std::make_unique<std::vector<Instruction>>();
                for (const auto &bi : arg->body)
                    bodyVec->push_back(convertInstruction(bi, bodySb, pol, scope, ir, cfg));
                forData->body = std::move(bodyVec);
                forData->numCols = 0;
                forData->cells = std::make_unique<std::vector<AlignCellInfo>>();
            }

            Instruction result;
            result.value.emplace<3>(std::move(forData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::IfInstr>>)
        {
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

            auto ifData = std::make_unique<IfData>();
            ifData->condPath = arg->condExpr.path;
            ifData->condIsBool = std::holds_alternative<tpp::BoolType>(arg->condExpr.resolvedType);
            ifData->isNegated = arg->negated;
            ifData->isBlock = arg->isBlock;
            ifData->insertCol = arg->insertCol;
            ifData->sb = sb;
            ifData->thenScopeId = thenScopeId;
            ifData->elseScopeId = elseScopeId;
            ifData->hasElse = !arg->elseBody.empty();

            auto thenVec = std::make_unique<std::vector<Instruction>>();
            for (const auto &ti : arg->thenBody)
                thenVec->push_back(convertInstruction(ti, thenSb, activePolicy, scope, ir, cfg));
            ifData->thenBody = std::move(thenVec);

            auto elseVec = std::make_unique<std::vector<Instruction>>();
            for (const auto &ei : arg->elseBody)
                elseVec->push_back(convertInstruction(ei, elseSb, activePolicy, scope, ir, cfg));
            ifData->elseBody = std::move(elseVec);

            Instruction result;
            result.value.emplace<4>(std::move(ifData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::SwitchInstr>>)
        {
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;

            // Unwrap OptionalType to find the underlying NamedType for enum lookup
            tpp::TypeRef switchType = arg->expr.resolvedType;
            bool exprIsOptional = false;
            if (auto *opt = std::get_if<std::shared_ptr<tpp::OptionalType>>(&switchType))
            {
                switchType = (*opt)->innerType;
                exprIsOptional = true;
            }

            const tpp::EnumDef *enumDef = nullptr;
            if (auto *nt = std::get_if<tpp::NamedType>(&switchType))
                for (const auto &e : ir.types.enums)
                    if (e.name == nt->name) { enumDef = &e; break; }

            auto casesVec = std::make_unique<std::vector<CaseData>>();
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

                auto caseBody = std::make_unique<std::vector<Instruction>>();
                for (const auto &bi : c.body)
                    caseBody->push_back(convertInstruction(bi, caseSb, pol, scope, ir, cfg));

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

                CaseData caseData;
                caseData.tag = c.tag;
                caseData.tagLit = stringLiteral(c.tag);
                caseData.bindingName = c.bindingName;
                caseData.hasBinding = !c.bindingName.empty();
                caseData.body = std::move(caseBody);
                caseData.scopeId = caseScopeId;
                caseData.isFirst = first;
                caseData.variantIndex = variantIndex;
                caseData.isRecursivePayload = isRecursivePayload;
                if (c.payloadType.has_value())
                    caseData.payloadType = std::make_unique<TypeKind>(typeRefToContext(*c.payloadType));
                else if (caseData.hasBinding && enumDef)
                {
                    // Derive payload type from enum when IR omits it (e.g. optional-wrapped switch)
                    for (const auto &ev : enumDef->variants)
                        if (ev.tag == c.tag && ev.payload.has_value())
                        { caseData.payloadType = std::make_unique<TypeKind>(typeRefToContext(*ev.payload)); break; }
                }

                casesVec->push_back(std::move(caseData));
                first = false;
            }

            std::string enumTypeName;
            if (auto *nt = std::get_if<tpp::NamedType>(&switchType))
                enumTypeName = nt->name;

            // Dereference unique_ptr when the switch expression crosses a recursive/optional field
            std::string switchExprPath = exprIsOptional
                ? "(*" + arg->expr.path + ")"
                : fixCppPath(arg->expr.path, ir);

            auto switchData = std::make_unique<SwitchData>();
            switchData->exprPath = switchExprPath;
            switchData->enumTypeName = enumTypeName;
            switchData->cases = std::move(casesVec);
            switchData->isBlock = arg->isBlock;
            switchData->insertCol = arg->insertCol;
            switchData->sb = sb;

            Instruction result;
            result.value.emplace<5>(std::move(switchData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<tpp::CallInstr>>)
        {
            std::vector<CallArgInfo> argsVec;
            for (size_t i = 0; i < arg->arguments.size(); ++i)
            {
                auto fl = lookupField(arg->arguments[i].path, ir);
                argsVec.push_back({arg->arguments[i].path, fl.recursive, fl.optional});
            }

            CallData callData;
            callData.functionName = cfg.functionPrefix + arg->functionName;
            callData.args = std::move(argsVec);
            callData.sb = sb;
            callData.needsTry = cfg.callNeedsTry && !ir.policies.all().empty();
            callData.policyArg = makePolicyRef(activePolicy, ir);

            return {std::move(callData)};
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
            std::vector<RenderViaOverload> overloadsVec;

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
                            RenderViaOverload ovlData;
                            ovlData.tag = v.tag;
                            ovlData.tagLit = stringLiteral(v.tag);
                            ovlData.isFirst = first;
                            if (v.payload.has_value())
                                ovlData.payloadType = std::make_unique<TypeKind>(typeRefToContext(*v.payload));
                            overloadsVec.push_back(std::move(ovlData));
                            first = false;
                        }
                    }
                }
            }

            RenderViaData rvia;
            rvia.scopeId = s;
            rvia.collPath = fixCppPath(arg->collection.path, ir);
            rvia.elemType = std::make_unique<TypeKind>(typeRefToContext(elemType));
            rvia.functionName = cfg.functionPrefix + arg->functionName;
            rvia.hasSep = !arg->sep.empty();
            rvia.sepLit = !arg->sep.empty() ? stringLiteral(arg->sep) : "";
            rvia.hasFollowed = !arg->followedBy.empty();
            rvia.followedByLit = !arg->followedBy.empty() ? stringLiteral(arg->followedBy) : "";
            rvia.hasPreceded = !arg->precededBy.empty();
            rvia.precededByLit = !arg->precededBy.empty() ? stringLiteral(arg->precededBy) : "";
            rvia.sb = sb;
            rvia.isSingleOverload = isSingleOverload;
            rvia.overloads = std::move(overloadsVec);
            rvia.needsTry = cfg.callNeedsTry && !ir.policies.all().empty();
            rvia.policyArg = makePolicyRef(
                arg->policy.empty() ? activePolicy : arg->policy, ir);

            return {std::move(rvia)};
        }
        else
        {
            return {EmitData{"", ""}};
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

// ═══════════════════════════════════════════════════════════════════════════════
// Build CodegenInput — shared across all backends
// ═══════════════════════════════════════════════════════════════════════════════

/// Build the shared CodegenInput from the IR.
/// Populates structs, enums, functions, and hasRecursiveTypes.
/// Backend-specific fields (rawTypedefs, iRepJson, functionPrefix, includes,
/// namespaceName) are set to defaults and should be overridden by callers.
inline CodegenInput buildCodegenInput(const tpp::IR &ir)
{
    // ── Structs ──
    std::vector<StructInfo> structs;
    bool hasRecursiveTypes = false;
    for (const auto &s : ir.types.structs)
    {
        std::vector<FieldInfo> fields;
        for (const auto &f : s.fields)
        {
            bool isOpt = std::holds_alternative<std::shared_ptr<tpp::OptionalType>>(f.type);
            TypeKind innerTypeCtx = typeRefToContext(f.type);
            if (isOpt)
            {
                auto &opt = std::get<std::shared_ptr<tpp::OptionalType>>(f.type);
                innerTypeCtx = typeRefToContext(opt->innerType);
            }

            FieldInfo field;
            field.name = f.name;
            field.type = std::make_unique<TypeKind>(typeRefToContext(f.type));
            field.recursive = f.recursive;
            field.isOptional = isOpt;
            field.recursiveOptional = f.recursive && isOpt;
            field.innerType = std::make_unique<TypeKind>(std::move(innerTypeCtx));
            if (!f.doc.empty()) field.doc = f.doc;
            fields.push_back(std::move(field));
            if (f.recursive) hasRecursiveTypes = true;
        }
        StructInfo structInfo;
        structInfo.name = s.name;
        structInfo.fields = std::move(fields);
        if (!s.doc.empty()) structInfo.doc = s.doc;
        structs.push_back(std::move(structInfo));
    }

    // ── Enums ──
    std::vector<EnumInfo> enums;
    for (const auto &e : ir.types.enums)
    {
        std::vector<VariantInfo> variants;
        bool hasRecursive = false;
        int variantIndex = 0;
        for (const auto &v : e.variants)
        {
            VariantInfo variant;
            variant.tag = v.tag;
            variant.recursive = v.recursive;
            variant.index = variantIndex++;
            if (v.payload.has_value())
                variant.payload = std::make_unique<TypeKind>(typeRefToContext(*v.payload));
            if (!v.doc.empty()) variant.doc = v.doc;
            variants.push_back(std::move(variant));
            if (v.recursive) hasRecursive = true;
        }
        EnumInfo enumInfo;
        enumInfo.name = e.name;
        enumInfo.variants = std::move(variants);
        enumInfo.hasRecursiveVariants = hasRecursive;
        if (!e.doc.empty()) enumInfo.doc = e.doc;
        enums.push_back(std::move(enumInfo));
    }

    // ── Functions ──
    std::vector<FunctionInfo> functions;
    for (const auto &fn : ir.functions)
    {
        std::vector<ParamInfo> params;
        for (const auto &p : fn.params)
            params.push_back({p.name, std::make_unique<TypeKind>(typeRefToContext(p.type))});
        FunctionInfo funcInfo;
        funcInfo.name = fn.name;
        funcInfo.params = std::move(params);
        if (!fn.doc.empty()) funcInfo.doc = fn.doc;
        functions.push_back(std::move(funcInfo));
    }

    CodegenInput result;
    result.structs = std::move(structs);
    result.enums = std::move(enums);
    result.functions = std::move(functions);
    result.hasRecursiveTypes = hasRecursiveTypes;
    return result;
}

/// Build the policy context.
inline std::vector<PolicyInfo> buildPolicyContext(const tpp::IR &ir, const std::string &nullLiteral)
{
    std::vector<PolicyInfo> policies;
    for (const auto &[tag_, pol] : ir.policies.all())
    {
        PolicyInfo pj;
        pj.tagLit = stringLiteral(pol.tag);
        pj.identifier = sanitizeIdentifier(pol.tag);
        pj.hasLength = pol.length.has_value();
        pj.minVal = pol.length.has_value() && pol.length->min.has_value()
                       ? std::to_string(*pol.length->min) : nullLiteral;
        pj.maxVal = pol.length.has_value() && pol.length->max.has_value()
                       ? std::to_string(*pol.length->max) : nullLiteral;
        pj.hasRejectIf = pol.rejectIf.has_value();
        pj.rejectIfRegexLit = pol.rejectIf.has_value()
                                 ? stringLiteral(pol.rejectIf->regex) : "";
        pj.rejectMsgLit = pol.rejectIf.has_value()
                             ? stringLiteral(pol.rejectIf->message) : "";

        std::vector<PolicyRequireInfo> reqArr;
        for (const auto &r : pol.require)
            reqArr.push_back({
                stringLiteral(r.regex),
                !r.replace.empty() ? stringLiteral(r.compiled_replace) : "",
                !r.replace.empty()
            });
        pj.require = std::move(reqArr);
        pj.hasRequire = !pol.require.empty();

        std::vector<PolicyReplacementInfo> replArr;
        for (const auto &r : pol.replacements)
            replArr.push_back({stringLiteral(r.find), stringLiteral(r.replace)});
        pj.replacements = std::move(replArr);
        pj.hasReplacements = !pol.replacements.empty();

        std::vector<PolicyOutputFilterInfo> ofArr;
        for (const auto &f : pol.outputFilter)
            ofArr.push_back({stringLiteral(f.regex)});
        pj.outputFilter = std::move(ofArr);
        pj.hasOutputFilter = !pol.outputFilter.empty();

        policies.push_back(std::move(pj));
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
