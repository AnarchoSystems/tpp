// CodegenHelpers.h — shared helpers for tpp code generation backends.
//
// Provides context-building functions for the template-based code
// generation pipeline (tpp2cpp, tpp2java, tpp2swift).

#pragma once

#include <tpp/IR.h>
#include <tpp/Policy.h>
#include "CodegenTypes.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>

namespace codegen {

// ═══════════════════════════════════════════════════════════════════════════════
// Shared backend CLI helpers
// ═══════════════════════════════════════════════════════════════════════════════

template <typename Mode>
struct ParsedBackendCommandLine
{
    Mode mode;
    std::string inputFile;
    std::string namespaceName;
    bool externalRuntime = false;
    std::vector<std::string> includes;
};

template <typename Mode>
inline ParsedBackendCommandLine<Mode> parseBackendCommandLine(
    int argc,
    char *argv[],
    const std::map<std::string, Mode> &commands,
    void (*printUsage)(),
    bool allowIncludes = false)
{
    int cmdIndex = 0;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            std::exit(EXIT_SUCCESS);
        }
        if (!arg.empty() && arg[0] != '-')
        {
            cmdIndex = i;
            break;
        }
        if (arg == "-ns" || arg == "--input" || (allowIncludes && arg == "-i"))
            ++i;
    }

    if (cmdIndex == 0)
    {
        printUsage();
        std::exit(EXIT_FAILURE);
    }

    auto commandIt = commands.find(argv[cmdIndex]);
    if (commandIt == commands.end())
    {
        std::cerr << "Unknown command: " << argv[cmdIndex] << "\nUse -h for usage info.\n";
        std::exit(EXIT_FAILURE);
    }

    ParsedBackendCommandLine<Mode> result;
    result.mode = commandIt->second;

    for (int i = 1; i < argc; ++i)
    {
        if (i == cmdIndex)
            continue;

        std::string arg = argv[i];
        if (arg == "-ns")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "-ns requires a name argument\n";
                std::exit(EXIT_FAILURE);
            }
            result.namespaceName = argv[++i];
        }
        else if (arg == "--input")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--input requires a file argument\n";
                std::exit(EXIT_FAILURE);
            }
            result.inputFile = argv[++i];
        }
        else if (allowIncludes && arg == "-i")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "-i requires a file argument\n";
                std::exit(EXIT_FAILURE);
            }
            result.includes.push_back(argv[++i]);
        }
        else if (arg == "--extern-runtime")
        {
            result.externalRuntime = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\nUse -h for usage info.\n";
            std::exit(EXIT_FAILURE);
        }
    }

    return result;
}

inline std::string readTextInputOrExit(const std::string &inputFile)
{
    if (!inputFile.empty())
    {
        std::ifstream inputStream(inputFile);
        if (!inputStream.is_open())
        {
            std::cerr << "Could not open input file: " << inputFile << std::endl;
            std::exit(EXIT_FAILURE);
        }
        return std::string(std::istreambuf_iterator<char>(inputStream), std::istreambuf_iterator<char>());
    }

    return std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
}

inline tpp::IR parseIRJsonOrExit(const std::string &inputJson)
{
    try
    {
        return nlohmann::json::parse(inputJson).get<tpp::IR>();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse input JSON: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

inline tpp::IR loadIRInputOrExit(const std::string &inputFile)
{
    return parseIRJsonOrExit(readTextInputOrExit(inputFile));
}

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

inline ExprInfo exprInfoToContext(const tpp::ExprInfo &expr)
{
    ExprInfo result;
    result.path = expr.path;
    result.type = std::make_unique<TypeKind>(typeKindToContext(*expr.type));
    result.isRecursive = expr.isRecursive;
    result.isOptional = expr.isOptional;
    return result;
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

            EmitExprData data;
            data.expr = exprInfoToContext(arg.expr);
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
            bool hasAlign = arg->alignSpec.has_value();

            auto forData = std::make_unique<ForData>();
            forData->scopeId = s;
            forData->collPath = arg->collection.path;
            forData->collIsRecursive = arg->collection.isRecursive;
            forData->collIsOptional = arg->collection.isOptional;
            forData->elemType = std::make_unique<TypeKind>(typeKindToContext(elemType));
            forData->varName = arg->varName;
            forData->hasEnum = arg->enumeratorName.has_value();
            forData->enumeratorName = arg->enumeratorName.value_or("");
            forData->hasSep = arg->sep.has_value();
            forData->sepLit = arg->sep.has_value() ? stringLiteral(*arg->sep) : "";
            forData->hasFollowed = arg->followedBy.has_value();
            forData->followedByLit = arg->followedBy.has_value() ? stringLiteral(*arg->followedBy) : "";
            forData->hasPreceded = arg->precededBy.has_value();
            forData->precededByLit = arg->precededBy.has_value() ? stringLiteral(*arg->precededBy) : "";
            forData->isBlock = arg->isBlock;
            forData->insertCol = arg->insertCol;
            forData->sb = sb;
            forData->hasAlign = hasAlign;
            forData->alignSpec = arg->alignSpec.value_or("");
            forData->singleAlignChar = arg->alignSpec.has_value() && arg->alignSpec->size() == 1;

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
                if (arg->alignSpec.has_value())
                    for (char c : *arg->alignSpec)
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
            if (switchType->value.index() == 5)
            {
                switchType = std::get<5>(switchType->value).get();
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
                if (enumDef)
                {
                    for (int vi = 0; vi < (int)enumDef->variants.size(); ++vi)
                    {
                        if (enumDef->variants[vi].tag == c.tag)
                        {
                            variantIndex = vi;
                            break;
                        }
                    }
                }

                CaseData caseData;
                caseData.tag = c.tag;
                caseData.tagLit = stringLiteral(c.tag);
                caseData.bindingName = c.bindingName
                    ? std::make_optional(c.bindingName->name)
                    : std::nullopt;
                caseData.body = std::move(caseBody);
                caseData.scopeId = caseScopeId;
                caseData.isFirst = first;
                caseData.variantIndex = variantIndex;
                caseData.isRecursivePayload = c.payloadIsRecursive;
                if (c.payloadType)
                    caseData.payloadType = std::make_unique<TypeKind>(typeKindToContext(*c.payloadType));
                else if (caseData.bindingName.has_value() && enumDef)
                {
                    // Derive payload type from enum when IR omits it (e.g. optional-wrapped switch)
                    for (const auto &ev : enumDef->variants)
                        if (ev.tag == c.tag && ev.payload)
                        { caseData.payloadType = std::make_unique<TypeKind>(typeKindToContext(*ev.payload)); break; }
                }

                casesVec->push_back(std::move(caseData));
                first = false;
            }

            std::string enumTypeName;
            if (switchType->value.index() == 3)
                enumTypeName = std::get<3>(switchType->value);

            auto switchData = std::make_unique<SwitchData>();
            switchData->exprPath = arg->expr.path;
            switchData->exprIsRecursive = arg->expr.isRecursive;
            switchData->exprIsOptional = arg->expr.isOptional;
            switchData->enumTypeName = enumTypeName;
            switchData->cases = std::move(casesVec);
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
                argsVec.push_back({arg.arguments[i].path,
                                   arg.arguments[i].isRecursive,
                                   arg.arguments[i].isOptional});

            CallData callData;
            callData.functionName = cfg.functionPrefix + arg.functionName;
            callData.args = std::move(argsVec);
            callData.sb = sb;
            callData.needsTry = cfg.callNeedsTry && !ir.policies.empty();
            callData.policyArg = makePolicyRef(activePolicy, ir);

            return {std::move(callData)};
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
                !r.replace.empty() ? stringLiteral(tpp::compiler::precompile_replace(r.replace)) : "",
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

    static BuildFunctionsConfig forCpp(const std::vector<std::string> &includes = {})
    {
        BuildFunctionsConfig cfg;
        cfg.nullLiteral = "std::nullopt";
        cfg.staticModifier = "";
        cfg.callNeedsTry = false;
        cfg.includes = includes;
        return cfg;
    }

    static BuildFunctionsConfig forJava()
    {
        BuildFunctionsConfig cfg;
        cfg.nullLiteral = "null";
        cfg.staticModifier = "static ";
        cfg.callNeedsTry = false;
        return cfg;
    }

    static BuildFunctionsConfig forSwift(const std::string &namespaceName, bool hasPolicies)
    {
        BuildFunctionsConfig cfg;
        cfg.nullLiteral = "nil";
        cfg.staticModifier = namespaceName.empty() ? "" : "static ";
        cfg.callNeedsTry = hasPolicies;
        return cfg;
    }
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
