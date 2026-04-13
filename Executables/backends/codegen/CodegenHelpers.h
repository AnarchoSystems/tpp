// CodegenHelpers.h — shared helpers for tpp code generation backends.
//
// Provides context-building functions for the template-based code
// generation pipeline (tpp2cpp, tpp2java, tpp2swift).

#pragma once

#include <tpp/IR.h>
#include "CodegenTypes.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>

namespace codegen {

// ═══════════════════════════════════════════════════════════════════════════════
// Type → template context helpers
// ═══════════════════════════════════════════════════════════════════════════════

/// Converts a tpp::TypeKind to the codegen::TypeKind struct.
inline TypeKind typeKindToContext(const tpp::TypeKind &t)
{
    TypeKind result;
    switch (t.value.index())
    {
    case 0: result.value = TypeKind_Str{}; break;
    case 1: result.value = TypeKind_Int{}; break;
    case 2: result.value = TypeKind_Bool{}; break;
    case 3: result.value.emplace<3>(std::get<3>(t.value)); break;
    case 4: result.value.emplace<4>(std::make_unique<TypeKind>(typeKindToContext(*std::get<4>(t.value)))); break;
    case 5: result.value.emplace<5>(std::make_unique<TypeKind>(typeKindToContext(*std::get<5>(t.value)))); break;
    default: result.value = TypeKind_Str{}; break;
    }
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

/// Deep-clone a TypeKind via JSON round-trip (TypeKind is non-copyable due to unique_ptr).
inline tpp::TypeKind cloneTypeKind(const tpp::TypeKind &t)
{
    nlohmann::json j;
    to_json(j, t);
    tpp::TypeKind result;
    from_json(j, result);
    return result;
}

/// Compare two TypeKind values for equality via JSON serialization.
inline bool typeKindEqual(const tpp::TypeKind &a, const tpp::TypeKind &b)
{
    nlohmann::json ja, jb;
    to_json(ja, a);
    to_json(jb, b);
    return ja == jb;
}

/// Extract element type from a collection's List TypeKind.
inline tpp::TypeKind getElemType(const tpp::TypeKind &t)
{
    if (t.value.index() == 4) // List
        return cloneTypeKind(*std::get<4>(t.value));
    return cloneTypeKind(t);
}

/// Split instructions at AlignCell boundaries into separate cell bodies.
/// Returns pointers into the original vector (Instruction is non-copyable).
inline std::vector<std::vector<const tpp::Instruction*>> splitCells(
    const std::vector<tpp::Instruction> &body)
{
    std::vector<std::vector<const tpp::Instruction*>> cells;
    cells.emplace_back();
    for (const auto &instr : body)
    {
        if (std::holds_alternative<tpp::Instruction_AlignCell>(instr.value))
            cells.emplace_back();
        else
            cells.back().push_back(&instr);
    }
    return cells;
}

/// Sanitize a policy tag into a valid identifier (replace non-alnum with _).
inline std::string sanitizeIdentifier(const std::string &tag);

/// Build a PolicyRef from the active policy state.
inline std::optional<PolicyRef> makePolicyRef(
    const std::string &activePolicy, const tpp::IR &ir)
{
    if (ir.policies.empty()) return std::nullopt;
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
    for (const auto &s : ir.structs)
        for (const auto &f : s.fields)
            if (f.name == fieldName)
                return {f.recursive,
                        f.type && f.type->value.index() == 5};
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
                std::make_unique<TypeKind>(typeKindToContext(*arg.expr.type)),
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
            else if (!ir.policies.empty())
            {
                data.useRuntimePolicy = true;
            }
            return {std::move(data)};
        }
        else if constexpr (std::is_same_v<T, tpp::Instruction_AlignCell>)
        {
            return {Instruction_AlignCell{}};
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::ForInstr>>)
        {
            int s = scope++;
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;
            auto elemType = getElemType(*arg->collection.type);
            bool hasAlign = arg->hasAlign;

            auto forData = std::make_unique<ForData>();
            forData->scopeId = s;
            forData->collPath = fixCppPath(arg->collection.path, ir);
            forData->elemType = std::make_unique<TypeKind>(typeKindToContext(elemType));
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
                auto cellGroups = splitCells(*arg->body);
                forData->numCols = (int)cellGroups.size();
                auto cellsVec = std::make_unique<std::vector<AlignCellInfo>>();
                for (int ci = 0; ci < (int)cellGroups.size(); ++ci)
                {
                    std::string cellSb = "_cell" + std::to_string(s) + "_" + std::to_string(ci);
                    auto cellBody = std::make_unique<std::vector<Instruction>>();
                    for (const auto *bi : cellGroups[ci])
                        cellBody->push_back(convertInstruction(*bi, cellSb, pol, scope, ir, cfg));
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
                for (const auto &bi : *arg->body)
                    bodyVec->push_back(convertInstruction(bi, bodySb, pol, scope, ir, cfg));
                forData->body = std::move(bodyVec);
                forData->numCols = 0;
                forData->cells = std::make_unique<std::vector<AlignCellInfo>>();
            }

            Instruction result;
            result.value.emplace<3>(std::move(forData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::IfInstr>>)
        {
            int thenScopeId = 0, elseScopeId = 0;
            std::string thenSb = sb, elseSb = sb;
            if (arg->isBlock)
            {
                thenScopeId = scope++;
                thenSb = "_blk" + std::to_string(thenScopeId);
                if (!arg->elseBody->empty())
                {
                    elseScopeId = scope++;
                    elseSb = "_blk" + std::to_string(elseScopeId);
                }
            }

            auto ifData = std::make_unique<IfData>();
            ifData->condPath = arg->condExpr.path;
            ifData->condIsBool = arg->condExpr.type->value.index() == 2;
            ifData->isNegated = arg->negated;
            ifData->isBlock = arg->isBlock;
            ifData->insertCol = arg->insertCol;
            ifData->sb = sb;
            ifData->thenScopeId = thenScopeId;
            ifData->elseScopeId = elseScopeId;
            ifData->hasElse = !arg->elseBody->empty();

            auto thenVec = std::make_unique<std::vector<Instruction>>();
            for (const auto &ti : *arg->thenBody)
                thenVec->push_back(convertInstruction(ti, thenSb, activePolicy, scope, ir, cfg));
            ifData->thenBody = std::move(thenVec);

            auto elseVec = std::make_unique<std::vector<Instruction>>();
            for (const auto &ei : *arg->elseBody)
                elseVec->push_back(convertInstruction(ei, elseSb, activePolicy, scope, ir, cfg));
            ifData->elseBody = std::move(elseVec);

            Instruction result;
            result.value.emplace<4>(std::move(ifData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::SwitchInstr>>)
        {
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;

            // Unwrap OptionalType to find the underlying NamedType for enum lookup
            const tpp::TypeKind *switchType = arg->expr.type.get();
            bool exprIsOptional = false;
            if (switchType->value.index() == 5)
            {
                switchType = std::get<5>(switchType->value).get();
                exprIsOptional = true;
            }

            const tpp::EnumDef *enumDef = nullptr;
            if (switchType->value.index() == 3)
            {
                const auto &typeName = std::get<3>(switchType->value);
                for (const auto &e : ir.enums)
                    if (e.name == typeName) { enumDef = &e; break; }
            }

            auto casesVec = std::make_unique<std::vector<CaseData>>();
            bool first = true;
            for (const auto &c : *arg->cases)
            {
                int caseScopeId = 0;
                std::string caseSb = sb;
                if (arg->isBlock)
                {
                    caseScopeId = scope++;
                    caseSb = "_blk" + std::to_string(caseScopeId);
                }

                auto caseBody = std::make_unique<std::vector<Instruction>>();
                for (const auto &bi : *c.body)
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
                                                 && enumDef->variants[vi].payload != nullptr;
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
                if (c.payloadType)
                    caseData.payloadType = std::make_unique<TypeKind>(typeKindToContext(*c.payloadType));
                else if (caseData.hasBinding && enumDef)
                {
                    // Derive payload type from enum when IR omits it (e.g. optional-wrapped switch)
                    for (const auto &ev : enumDef->variants)
                        if (ev.tag == c.tag && ev.payload)
                        { caseData.payloadType = std::make_unique<TypeKind>(typeKindToContext(*ev.payload)); break; }
                }

                casesVec->push_back(std::move(caseData));
                first = false;
            }

            std::unique_ptr<CaseData> defaultCase;
            if (arg->defaultCase)
            {
                const auto &c = *arg->defaultCase;
                int caseScopeId = 0;
                std::string caseSb = sb;
                if (arg->isBlock)
                {
                    caseScopeId = scope++;
                    caseSb = "_blk" + std::to_string(caseScopeId);
                }

                auto caseBody = std::make_unique<std::vector<Instruction>>();
                for (const auto &bi : *c.body)
                    caseBody->push_back(convertInstruction(bi, caseSb, pol, scope, ir, cfg));

                defaultCase = std::make_unique<CaseData>();
                defaultCase->tag = c.tag;
                defaultCase->tagLit = stringLiteral(c.tag);
                defaultCase->bindingName = c.bindingName;
                defaultCase->hasBinding = !c.bindingName.empty();
                defaultCase->body = std::move(caseBody);
                defaultCase->scopeId = caseScopeId;
                defaultCase->isFirst = first;
                defaultCase->variantIndex = -1;
                defaultCase->isRecursivePayload = false;
                if (c.payloadType)
                    defaultCase->payloadType = std::make_unique<TypeKind>(typeKindToContext(*c.payloadType));
            }

            std::string enumTypeName;
            if (switchType->value.index() == 3)
                enumTypeName = std::get<3>(switchType->value);

            // Dereference unique_ptr when the switch expression crosses a recursive/optional field
            std::string switchExprPath = exprIsOptional
                ? "(*" + arg->expr.path + ")"
                : fixCppPath(arg->expr.path, ir);

            auto switchData = std::make_unique<SwitchData>();
            switchData->exprPath = switchExprPath;
            switchData->enumTypeName = enumTypeName;
            switchData->cases = std::move(casesVec);
            switchData->defaultCase = std::move(defaultCase);
            switchData->isBlock = arg->isBlock;
            switchData->insertCol = arg->insertCol;
            switchData->sb = sb;

            Instruction result;
            result.value.emplace<5>(std::move(switchData));
            return result;
        }
        else if constexpr (std::is_same_v<T, tpp::CallInstr>)
        {
            std::vector<CallArgInfo> argsVec;
            for (size_t i = 0; i < arg.arguments.size(); ++i)
            {
                auto fl = lookupField(arg.arguments[i].path, ir);
                argsVec.push_back({arg.arguments[i].path, fl.recursive, fl.optional});
            }

            CallData callData;
            callData.functionName = cfg.functionPrefix + arg.functionName;
            callData.args = std::move(argsVec);
            callData.sb = sb;
            callData.needsTry = cfg.callNeedsTry && !ir.policies.empty();
            callData.policyArg = makePolicyRef(activePolicy, ir);

            return {std::move(callData)};
        }
        else if constexpr (std::is_same_v<T, tpp::RenderViaInstr>)
        {
            int s = scope++;
            bool isCollection = arg.collection.type->value.index() == 4;
            auto elemType = getElemType(*arg.collection.type);

            std::vector<const tpp::FunctionDef *> overloads;
            for (const auto &fn : ir.functions)
                if (fn.name == arg.functionName)
                    overloads.push_back(&fn);

            bool isSingleOverload = overloads.size() == 1 && overloads[0]->params.size() == 1
                && overloads[0]->params[0].type && typeKindEqual(*overloads[0]->params[0].type, elemType);
            std::vector<RenderViaOverload> overloadsVec;
            std::string enumTypeName;

            if (!isSingleOverload)
            {
                const tpp::EnumDef *enumDef = nullptr;
                if (elemType.value.index() == 3)
                {
                    enumTypeName = std::get<3>(elemType.value);
                    for (const auto &e : ir.enums)
                        if (e.name == enumTypeName) { enumDef = &e; break; }
                }

                if (enumDef)
                {
                    bool first = true;
                    for (const auto &v : enumDef->variants)
                    {
                        const tpp::FunctionDef *match = nullptr;
                        for (auto *ovl : overloads)
                        {
                            if (!v.payload)
                            {
                                if (ovl->params.empty())
                                {
                                    match = ovl;
                                    break;
                                }
                                continue;
                            }
                            if (ovl->params.size() == 1
                                && ovl->params[0].type
                                && typeKindEqual(*ovl->params[0].type, *v.payload))
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
                            if (v.payload)
                                ovlData.payloadType = std::make_unique<TypeKind>(typeKindToContext(*v.payload));
                            overloadsVec.push_back(std::move(ovlData));
                            first = false;
                        }
                    }
                }
            }

            RenderViaData rvia;
            rvia.scopeId = s;
            rvia.collPath = fixCppPath(arg.collection.path, ir);
            rvia.isCollection = isCollection;
            rvia.elemType = std::make_unique<TypeKind>(typeKindToContext(elemType));
            rvia.enumTypeName = enumTypeName;
            rvia.functionName = cfg.functionPrefix + arg.functionName;
            rvia.hasSep = !arg.sep.empty();
            rvia.sepLit = !arg.sep.empty() ? stringLiteral(arg.sep) : "";
            rvia.hasFollowed = !arg.followedBy.empty();
            rvia.followedByLit = !arg.followedBy.empty() ? stringLiteral(arg.followedBy) : "";
            rvia.hasPreceded = !arg.precededBy.empty();
            rvia.precededByLit = !arg.precededBy.empty() ? stringLiteral(arg.precededBy) : "";
            rvia.sb = sb;
            rvia.isSingleOverload = isSingleOverload;
            rvia.overloads = std::move(overloadsVec);
            rvia.needsTry = cfg.callNeedsTry && !ir.policies.empty();
            rvia.policyArg = makePolicyRef(
                arg.policy.empty() ? activePolicy : arg.policy, ir);

            return {std::move(rvia)};
        }
        else
        {
            return {EmitData{"", ""}};
        }
    }, instr.value);
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
/// Backend-specific data (includes, namespace, etc.) should be passed as
/// additional template arguments — not stored in CodegenInput.
inline CodegenInput buildCodegenInput(const tpp::IR &ir)
{
    // ── Structs ──
    std::vector<StructInfo> structs;
    bool hasRecursiveTypes = false;
    for (const auto &s : ir.structs)
    {
        std::vector<FieldInfo> fields;
        for (const auto &f : s.fields)
        {
            bool isOpt = f.type && f.type->value.index() == 5;
            TypeKind innerTypeCtx = typeKindToContext(*f.type);
            if (isOpt)
                innerTypeCtx = typeKindToContext(*std::get<5>(f.type->value));

            FieldInfo field;
            field.name = f.name;
            field.type = std::make_unique<TypeKind>(typeKindToContext(*f.type));
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
        structInfo.rawTypedefs = s.rawTypedefs;
        if (!s.doc.empty()) structInfo.doc = s.doc;
        structs.push_back(std::move(structInfo));
    }

    // ── Enums ──
    std::vector<EnumInfo> enums;
    for (const auto &e : ir.enums)
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
            if (v.payload)
                variant.payload = std::make_unique<TypeKind>(typeKindToContext(*v.payload));
            if (!v.doc.empty()) variant.doc = v.doc;
            variants.push_back(std::move(variant));
            if (v.recursive) hasRecursive = true;
        }
        EnumInfo enumInfo;
        enumInfo.name = e.name;
        enumInfo.variants = std::move(variants);
        enumInfo.hasRecursiveVariants = hasRecursive;
        enumInfo.rawTypedefs = e.rawTypedefs;
        if (!e.doc.empty()) enumInfo.doc = e.doc;
        enums.push_back(std::move(enumInfo));
    }

    // ── Functions ──
    std::vector<FunctionInfo> functions;
    for (const auto &fn : ir.functions)
    {
        std::vector<ParamInfo> params;
        for (const auto &p : fn.params)
            params.push_back({p.name, std::make_unique<TypeKind>(typeKindToContext(*p.type))});
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
    for (const auto &pol : ir.policies)
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
                !r.replace.empty() ? stringLiteral(r.compiledReplace) : "",
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
// Unified buildFunctionsContext
// ═══════════════════════════════════════════════════════════════════════════════

struct BuildFunctionsConfig
{
    std::string nullLiteral;
    std::string staticModifier;
    bool callNeedsTry = false;
    std::vector<std::string> includes;
};

inline RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir,
    const std::string &functionPrefix,
    const std::string &namespaceName,
    const BuildFunctionsConfig &bfCfg)
{
    bool hasPol = !ir.policies.empty();

    ConvertConfig cfg;
    cfg.functionPrefix = functionPrefix;
    cfg.callNeedsTry = bfCfg.callNeedsTry;

    std::vector<RenderFunctionDef> functions;
    for (const auto &fn : ir.functions)
    {
        std::vector<ParamInfo> params;
        for (size_t i = 0; i < fn.params.size(); ++i)
            params.push_back({fn.params[i].name,
                              std::make_unique<TypeKind>(typeKindToContext(*fn.params[i].type))});

        int scope = 0;
        auto body = std::make_unique<std::vector<Instruction>>();
        for (const auto &instr : *fn.body)
            body->push_back(convertInstruction(instr, "_sb", fn.policy, scope, ir, cfg));

        RenderFunctionDef def;
        def.name = fn.name;
        def.params = std::move(params);
        def.body = std::move(body);
        functions.push_back(std::move(def));
    }

    RenderFunctionsInput result;
    result.functions = std::move(functions);
    result.hasPolicies = hasPol;
    result.policies = hasPol ? buildPolicyContext(ir, bfCfg.nullLiteral) : std::vector<PolicyInfo>{};
    result.functionPrefix = functionPrefix;
    result.staticModifier = bfCfg.staticModifier;
    if (!bfCfg.includes.empty())
        result.includes = bfCfg.includes;
    if (!namespaceName.empty())
        result.namespaceName = namespaceName;
    return result;
}

} // namespace codegen
