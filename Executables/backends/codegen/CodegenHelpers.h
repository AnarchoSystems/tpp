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
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

namespace codegen {

inline bool isPublicIRTypeName(const std::string &name)
{
    static const std::array<std::string_view, 7> irTypeNames = {
        "IR", "TypeKind", "FieldDef", "StructDef", "VariantDef", "EnumDef", "FunctionDef"};
    return std::find(irTypeNames.begin(), irTypeNames.end(), name) != irTypeNames.end();
}

inline std::string qualifyPublicIRTypeName(const std::string &name)
{
    return isPublicIRTypeName(name) ? "tpp::" + name : name;
}

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
    ParsedBackendCommandLine<Mode> result{};

    if (argc < 2)
    {
        printUsage();
        std::exit(EXIT_FAILURE);
    }

    std::string command = argv[1];
    if (command == "-h" || command == "--help")
    {
        printUsage();
        std::exit(EXIT_SUCCESS);
    }

    auto modeIt = commands.find(command);
    if (modeIt == commands.end())
    {
        std::cerr << "Unknown command: " << command << "\n\n";
        printUsage();
        std::exit(EXIT_FAILURE);
    }
    result.mode = modeIt->second;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            std::exit(EXIT_SUCCESS);
        }
        else if (arg == "-ns")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "-ns requires a namespace argument\n";
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

/// Converts a semantic IR type into the render-model type used by emit templates.
inline RenderTypeKind typeKindToContext(const tpp::TypeKind &t)
{
    RenderTypeKind result;
    switch (t.value.index())
    {
    case 0: result.value = RenderTypeKind_Str{}; break;
    case 1: result.value = RenderTypeKind_Int{}; break;
    case 2: result.value = RenderTypeKind_Bool{}; break;
    case 3: result.value.emplace<3>(qualifyPublicIRTypeName(std::get<3>(t.value))); break;
    case 4: result.value.emplace<4>(std::make_unique<RenderTypeKind>(typeKindToContext(*std::get<4>(t.value)))); break;
    case 5: result.value.emplace<5>(std::make_unique<RenderTypeKind>(typeKindToContext(*std::get<5>(t.value)))); break;
    default: result.value = RenderTypeKind_Str{}; break;
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

inline RenderExprInfo exprInfoToContext(const tpp::ExprInfo &expr)
{
    RenderExprInfo result;
    result.path = expr.path;
    result.type = std::make_unique<RenderTypeKind>(typeKindToContext(*expr.type));
    result.isRecursive = expr.isRecursive;
    result.isOptional = expr.isOptional;
    return result;
}

/// Recursively convert an IR instruction to the render-model instruction tree.
inline RenderInstruction convertInstruction(
    const tpp::Instruction &instr, const std::string &sb,
    const std::string &activePolicy, int &scope, const tpp::IR &ir,
    const ConvertConfig &cfg)
{
    return std::visit([&](auto &&arg) -> RenderInstruction
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
            return {RenderInstruction_AlignCell{}};
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::ForInstr>>)
        {
            int s = scope++;
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;
            auto elemType = getElemType(*arg->collection.type);

            auto forData = std::make_unique<ForData>();
            forData->scopeId = s;
            forData->collPath = arg->collection.path;
            forData->collIsRecursive = arg->collection.isRecursive;
            forData->collIsOptional = arg->collection.isOptional;
            forData->elemType = std::make_unique<RenderTypeKind>(typeKindToContext(elemType));
            forData->varName = arg->varName;
            forData->enumeratorName = arg->enumeratorName;
            if (arg->sep.has_value())
                forData->sepLit = stringLiteral(*arg->sep);
            if (arg->followedBy.has_value())
                forData->followedByLit = stringLiteral(*arg->followedBy);
            if (arg->precededBy.has_value())
                forData->precededByLit = stringLiteral(*arg->precededBy);
            forData->isBlock = arg->isBlock;
            forData->insertCol = arg->insertCol;
            forData->sb = sb;
            forData->alignSpec = arg->alignSpec;
            forData->singleAlignChar = arg->alignSpec.has_value() && arg->alignSpec->size() == 1;

            if (arg->alignSpec.has_value())
            {
                auto cellGroups = splitCells(*arg->body);
                forData->numCols = (int)cellGroups.size();
                auto cellsVec = std::make_unique<std::vector<AlignCellInfo>>();
                for (int ci = 0; ci < (int)cellGroups.size(); ++ci)
                {
                    std::string cellSb = "_cell" + std::to_string(s) + "_" + std::to_string(ci);
                    auto cellBody = std::make_unique<std::vector<RenderInstruction>>();
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
            }
            else
            {
                std::string bodySb = arg->isBlock ? ("_blk" + std::to_string(s)) : sb;
                auto bodyVec = std::make_unique<std::vector<RenderInstruction>>();
                for (const auto &bi : *arg->body)
                    bodyVec->push_back(convertInstruction(bi, bodySb, pol, scope, ir, cfg));
                forData->body = std::move(bodyVec);
                forData->numCols = 0;
            }

            RenderInstruction result;
            result.value.emplace<3>(std::move(forData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::IfInstr>>)
        {
            int thenScopeId = 0, elseScopeId = 0;
            std::string thenSb = sb, elseSb = sb;
            bool elseBodyPresent = !arg->elseBody->empty();
            if (arg->isBlock)
            {
                thenScopeId = scope++;
                thenSb = "_blk" + std::to_string(thenScopeId);
                if (elseBodyPresent)
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

            auto thenVec = std::make_unique<std::vector<RenderInstruction>>();
            for (const auto &ti : *arg->thenBody)
                thenVec->push_back(convertInstruction(ti, thenSb, activePolicy, scope, ir, cfg));
            ifData->thenBody = std::move(thenVec);

            if (elseBodyPresent)
            {
                auto elseVec = std::make_unique<std::vector<RenderInstruction>>();
                for (const auto &ei : *arg->elseBody)
                    elseVec->push_back(convertInstruction(ei, elseSb, activePolicy, scope, ir, cfg));
                ifData->elseBody = std::move(elseVec);
            }

            RenderInstruction result;
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

                auto caseBody = std::make_unique<std::vector<RenderInstruction>>();
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
                if (!caseBody->empty())
                    caseData.body = std::move(caseBody);
                caseData.scopeId = caseScopeId;
                caseData.isFirst = first;
                caseData.variantIndex = variantIndex;
                caseData.isRecursivePayload = c.payloadIsRecursive;
                if (c.payloadType)
                    caseData.payloadType = std::make_unique<RenderTypeKind>(typeKindToContext(*c.payloadType));
                else if (caseData.bindingName.has_value() && enumDef)
                {
                    // Derive payload type from enum when IR omits it (e.g. optional-wrapped switch)
                    for (const auto &ev : enumDef->variants)
                        if (ev.tag == c.tag && ev.payload)
                        { caseData.payloadType = std::make_unique<RenderTypeKind>(typeKindToContext(*ev.payload)); break; }
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

            RenderInstruction result;
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

/// Build the policy context.
inline std::vector<PolicyInfo> buildPolicyContext(const tpp::IR &ir, const std::string &nullLiteral)
{
    std::vector<PolicyInfo> policies;
    for (const auto &pol : ir.policies)
    {
        PolicyInfo pj;
        pj.tagLit = stringLiteral(pol.tag);
        pj.identifier = sanitizeIdentifier(pol.tag);
        if (pol.length.has_value() && pol.length->min.has_value())
            pj.minVal = std::to_string(*pol.length->min);
        if (pol.length.has_value() && pol.length->max.has_value())
            pj.maxVal = std::to_string(*pol.length->max);
        if (pol.rejectIf.has_value())
        {
            pj.rejectIfRegexLit = stringLiteral(pol.rejectIf->regex);
            pj.rejectMsgLit = stringLiteral(pol.rejectIf->message);
        }

        std::vector<PolicyRequireInfo> reqArr;
        for (const auto &r : pol.require)
            reqArr.push_back({
                stringLiteral(r.regex),
                !r.replace.empty() ? std::make_optional(stringLiteral(tpp::precompile_replace(r.replace))) : std::nullopt
            });
        if (!reqArr.empty())
            pj.require = std::move(reqArr);

        std::vector<PolicyReplacementInfo> replArr;
        for (const auto &r : pol.replacements)
            replArr.push_back({stringLiteral(r.find), stringLiteral(r.replace)});
        if (!replArr.empty())
            pj.replacements = std::move(replArr);

        std::vector<PolicyOutputFilterInfo> ofArr;
        for (const auto &f : pol.outputFilter)
            ofArr.push_back({stringLiteral(f.regex)});
        if (!ofArr.empty())
            pj.outputFilter = std::move(ofArr);

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

    static BuildFunctionsConfig forSwift(const std::string &namespaceName, bool policiesPresent)
    {
        BuildFunctionsConfig cfg;
        cfg.nullLiteral = "nil";
        cfg.staticModifier = namespaceName.empty() ? "" : "static ";
        cfg.callNeedsTry = policiesPresent;
        return cfg;
    }
};

inline RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir,
    const std::string &functionPrefix,
    const std::string &namespaceName,
    const BuildFunctionsConfig &bfCfg)
{
    bool policiesPresent = !ir.policies.empty();

    ConvertConfig cfg;
    cfg.functionPrefix = functionPrefix;
    cfg.callNeedsTry = bfCfg.callNeedsTry;

    std::vector<RenderFunctionDef> functions;
    for (const auto &fn : ir.functions)
    {
        std::vector<ParamInfo> params;
        for (size_t i = 0; i < fn.params.size(); ++i)
            params.push_back({fn.params[i].name,
                              std::make_unique<RenderTypeKind>(typeKindToContext(*fn.params[i].type))});

        int scope = 0;
        auto body = std::make_unique<std::vector<RenderInstruction>>();
        for (const auto &instr : *fn.body)
            body->push_back(convertInstruction(instr, "_sb", fn.policy, scope, ir, cfg));

        RenderFunctionDef def;
        def.name = fn.name;
        def.params = std::move(params);
        def.body = std::move(body);
        if (!fn.doc.empty())
            def.doc = fn.doc;
        functions.push_back(std::move(def));
    }

    RenderFunctionsInput result;
    result.functions = std::move(functions);
    if (policiesPresent)
        result.policies = buildPolicyContext(ir, bfCfg.nullLiteral);
    result.functionPrefix = functionPrefix;
    result.staticModifier = bfCfg.staticModifier;
    if (!bfCfg.includes.empty())
        result.includes = bfCfg.includes;
    if (!namespaceName.empty())
        result.namespaceName = namespaceName;
    return result;
}

} // namespace codegen
