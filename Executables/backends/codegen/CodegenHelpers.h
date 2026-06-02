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
#include <sstream>

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
// Type → template context helpers
// ═══════════════════════════════════════════════════════════════════════════════

/// Converts a semantic IR type into the render-model type used by emit templates.
inline RenderTypeKind typeKindToContext(const tpp::TypeKind &t)
{
    RenderTypeKind result;
    switch (t.value.index())
    {
    case 0: result.value.emplace<0>(); break;
    case 1: result.value.emplace<1>(); break;
    case 2: result.value.emplace<2>(); break;
    case 3: result.value.emplace<3>(qualifyPublicIRTypeName(std::get<3>(t.value))); break;
    case 4: result.value.emplace<4>(std::make_unique<RenderTypeKind>(typeKindToContext(*std::get<4>(t.value)))); break;
    case 5: result.value.emplace<5>(std::make_unique<RenderTypeKind>(typeKindToContext(*std::get<5>(t.value)))); break;
    default: result.value.emplace<0>(); break;
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

inline std::vector<std::string> splitDocLines(const std::string &doc)
{
    std::vector<std::string> lines;
    if (doc.empty())
        return lines;

    size_t start = 0;
    while (start <= doc.size())
    {
        size_t end = doc.find('\n', start);
        std::string line = end == std::string::npos ? doc.substr(start) : doc.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(std::move(line));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    return lines;
}

inline std::string formatBlockDocComment(
    const std::string &doc,
    const std::string &indent,
    const std::string &linePrefix = "")
{
    auto lines = splitDocLines(doc);
    if (lines.empty())
        return {};

    std::ostringstream formatted;
    formatted << indent << "/**\n";
    for (const auto &line : lines)
    {
        formatted << indent;
        if (!linePrefix.empty())
        {
            formatted << linePrefix;
            if (!line.empty())
                formatted << ' ';
        }
        formatted << line << "\n";
    }
    formatted << indent << " */\n";
    return formatted.str();
}

inline std::string formatLineDocComment(
    const std::string &doc,
    const std::string &indent,
    const std::string &linePrefix)
{
    auto lines = splitDocLines(doc);
    if (lines.empty())
        return {};

    std::ostringstream formatted;
    for (const auto &line : lines)
    {
        formatted << indent << linePrefix;
        if (!line.empty())
            formatted << ' ' << line;
        formatted << "\n";
    }
    return formatted.str();
}

template <typename FormatDocComment>
inline void setDocComment(
    nlohmann::json &target,
    const std::string &doc,
    const std::string &indent,
    FormatDocComment &&formatDocComment)
{
    const std::string formatted = formatDocComment(doc, indent);
    if (!formatted.empty())
        target["docComment"] = formatted;
}

template <typename FormatDocComment>
inline nlohmann::json sourceFieldContext(
    const tpp::FieldDef &field,
    const std::string &docIndent,
    FormatDocComment &&formatDocComment)
{
    nlohmann::json fieldJson;
    fieldJson["name"] = field.name;
    fieldJson["type"] = typeKindToContext(*field.type);
    fieldJson["recursive"] = field.recursive;
    setDocComment(fieldJson, field.doc, docIndent, formatDocComment);
    return fieldJson;
}

template <typename FormatDocComment>
inline nlohmann::json sourceStructContext(
    const tpp::StructDef &structDef,
    const std::string &typeDocIndent,
    const std::string &fieldDocIndent,
    FormatDocComment &&formatDocComment)
{
    nlohmann::json structJson;
    structJson["name"] = structDef.name;
    structJson["fields"] = nlohmann::json::array();
    for (const auto &field : structDef.fields)
        structJson["fields"].push_back(sourceFieldContext(field, fieldDocIndent, formatDocComment));
    setDocComment(structJson, structDef.doc, typeDocIndent, formatDocComment);
    return structJson;
}

template <typename FormatDocComment>
inline nlohmann::json sourceVariantContext(
    const tpp::VariantDef &variant,
    const std::string &docIndent,
    FormatDocComment &&formatDocComment)
{
    nlohmann::json variantJson;
    variantJson["tag"] = variant.tag;
    if (variant.payload)
        variantJson["payload"] = typeKindToContext(*variant.payload);
    setDocComment(variantJson, variant.doc, docIndent, formatDocComment);
    return variantJson;
}

template <typename FormatDocComment>
inline nlohmann::json sourceEnumContext(
    const tpp::EnumDef &enumDef,
    const std::string &typeDocIndent,
    const std::string &variantDocIndent,
    FormatDocComment &&formatDocComment)
{
    nlohmann::json enumJson;
    enumJson["name"] = enumDef.name;
    enumJson["variants"] = nlohmann::json::array();
    for (const auto &variant : enumDef.variants)
        enumJson["variants"].push_back(sourceVariantContext(variant, variantDocIndent, formatDocComment));
    setDocComment(enumJson, enumDef.doc, typeDocIndent, formatDocComment);
    return enumJson;
}

template <typename FormatDocComment>
inline nlohmann::json buildSourceInput(
    const tpp::IR &ir,
    bool nested,
    FormatDocComment &&formatDocComment)
{
    nlohmann::json inputJson;
    inputJson["enums"] = nlohmann::json::array();
    inputJson["structs"] = nlohmann::json::array();

    const std::string typeDocIndent = nested ? "    " : "";
    const std::string memberDocIndent = nested ? "        " : "    ";

    for (const auto &enumDef : ir.enums)
        inputJson["enums"].push_back(sourceEnumContext(enumDef, typeDocIndent, memberDocIndent, formatDocComment));
    for (const auto &structDef : ir.structs)
        inputJson["structs"].push_back(sourceStructContext(structDef, typeDocIndent, memberDocIndent, formatDocComment));

    return inputJson;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shared instruction IR → template context conversion
// ═══════════════════════════════════════════════════════════════════════════════

struct CapturedBlockInfo
{
    bool wrapped = false;
    std::optional<int> blockIndentInParentBlock;
    size_t firstIndex = 0;
    size_t pastLastIndex = 0;
};

inline bool isCapturedBlockInstruction(const tpp::Instruction &instr)
{
    return std::holds_alternative<tpp::BeginCapturedBlockInstr>(instr.value) ||
           std::holds_alternative<tpp::Instruction_EmitCapturedBlock>(instr.value);
}

inline CapturedBlockInfo analyzeCapturedBlock(const std::vector<tpp::Instruction> &body)
{
    CapturedBlockInfo info;
    info.pastLastIndex = body.size();

    if (body.size() >= 2 &&
        std::holds_alternative<tpp::BeginCapturedBlockInstr>(body.front().value) &&
        std::holds_alternative<tpp::Instruction_EmitCapturedBlock>(body.back().value))
    {
        info.wrapped = true;
        info.blockIndentInParentBlock = std::get<tpp::BeginCapturedBlockInstr>(body.front().value).blockIndentInParentBlock;
        info.firstIndex = 1;
        info.pastLastIndex = body.size() - 1;
    }

    return info;
}

/// Split instructions at AlignCell boundaries into separate cell bodies.
/// Returns pointers into the original vector (Instruction is non-copyable).
inline std::vector<std::vector<const tpp::Instruction*>> splitCells(
    const std::vector<tpp::Instruction> &body)
{
    std::vector<std::vector<const tpp::Instruction*>> cells;
    cells.emplace_back();
    const auto capturedBlockInfo = analyzeCapturedBlock(body);
    for (size_t index = capturedBlockInfo.firstIndex; index < capturedBlockInfo.pastLastIndex; ++index)
    {
        const auto &instr = body[index];
        if (std::holds_alternative<tpp::Instruction_AlignCell>(instr.value))
            cells.emplace_back();
        else
            cells.back().push_back(&instr);
    }
    return cells;
}

inline const tpp::PolicyDef *findPolicyDefByTag(const tpp::IR &ir, const std::string &tag)
{
    for (const auto &policy : ir.policies)
    {
        if (policy.tag == tag)
            return &policy;
    }
    return nullptr;
}

/// Build a PolicyRef from the active policy state.
inline std::optional<PolicyRef> makePolicyRef(
    const std::string &activePolicy, const tpp::IR &ir)
{
    if (ir.policies.empty()) return std::nullopt;
    if (!activePolicy.empty() && activePolicy != "none")
    {
        const auto *policy = findPolicyDefByTag(ir, activePolicy);
        if (!policy)
            return std::nullopt;
        PolicyRef ref;
        ref.value.emplace<0>(policy->identifier);
        return ref;
    }
    if (activePolicy == "none")
    {
        PolicyRef ref;
        ref.value.emplace<1>();
        return ref;
    }
    PolicyRef ref;
    ref.value.emplace<2>();
    return ref;
}

/// Configuration for language-specific aspects of instruction conversion.
struct ConvertConfig
{
    /// Prefix for function names in generated code (e.g. "render_" or "").
    std::string functionPrefix = "render_";

    /// Whether policy-using call sites need `try` (Swift only).
    bool callNeedsTry = false;

    /// Preserve explicit BeginCapturedBlock/EmitCapturedBlock instructions in converted bodies.
    bool preserveCapturedBlockInstructions = false;
};

inline RenderExprInfo exprInfoToContext(const tpp::ExprInfo &expr)
{
    RenderValueRef valueRef;
    valueRef.path = expr.bindingName;
    for (const auto &segment : expr.segments)
        valueRef.path += "." + segment.field;
    valueRef.isRecursive = expr.isRecursive;
    valueRef.isOptional = expr.isOptional;

    RenderExprInfo result;
    result.ref = std::move(valueRef);
    result.type = std::make_unique<RenderTypeKind>(typeKindToContext(*expr.type));
    return result;
}

inline RenderValueRef exprValueRefToContext(const tpp::ExprInfo &expr)
{
    RenderValueRef result;
    result.path = expr.bindingName;
    for (const auto &segment : expr.segments)
        result.path += "." + segment.field;
    result.isRecursive = expr.isRecursive;
    result.isOptional = expr.isOptional;
    return result;
}

inline std::string exprInfoPath(const tpp::ExprInfo &expr)
{
    std::string path = expr.bindingName;
    for (const auto &segment : expr.segments)
        path += "." + segment.field;
    return path;
}

inline bool pathUsesBinding(const std::string &path, const std::string &bindingName)
{
    return path == bindingName ||
           (path.size() > bindingName.size() &&
            path.compare(0, bindingName.size(), bindingName) == 0 &&
            path[bindingName.size()] == '.');
}

inline bool instructionListUsesBinding(
    const std::vector<tpp::Instruction> &body,
    const std::string &bindingName,
    bool shadowed = false);

inline bool instructionUsesBinding(
    const tpp::Instruction &instr,
    const std::string &bindingName,
    bool shadowed = false)
{
    return std::visit([&](auto &&arg) -> bool
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, tpp::EmitExprInstr>)
        {
            return !shadowed && pathUsesBinding(exprInfoPath(arg.expr), bindingName);
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::ForInstr>>)
        {
            if (!arg)
                return false;

            const bool bodyShadowed = shadowed ||
                arg->varName == bindingName ||
                (arg->enumeratorName.has_value() && *arg->enumeratorName == bindingName);

                 return (!shadowed && pathUsesBinding(exprInfoPath(arg->collection), bindingName)) ||
                   instructionListUsesBinding(*arg->body, bindingName, bodyShadowed);
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::IfInstr>>)
        {
            if (!arg)
                return false;

                 return (!shadowed && pathUsesBinding(exprInfoPath(arg->condExpr), bindingName)) ||
                   instructionListUsesBinding(*arg->thenBody, bindingName, shadowed) ||
                   instructionListUsesBinding(*arg->elseBody, bindingName, shadowed);
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::SwitchInstr>>)
        {
            if (!arg)
                return false;

            if (!shadowed && pathUsesBinding(exprInfoPath(arg->expr), bindingName))
                return true;

            for (const auto &caseInstr : *arg->cases)
            {
                const bool caseShadowed = shadowed ||
                    (caseInstr.bindingName.has_value() && caseInstr.bindingName->name == bindingName);
                if (instructionListUsesBinding(*caseInstr.body, bindingName, caseShadowed))
                    return true;
            }
            return false;
        }
        else if constexpr (std::is_same_v<T, tpp::CallInstr>)
        {
            if (shadowed)
                return false;

            for (const auto &callArg : arg.arguments)
            {
                if (pathUsesBinding(exprInfoPath(callArg), bindingName))
                    return true;
            }
            return false;
        }

        return false;
    }, instr.value);
}

inline bool instructionListUsesBinding(
    const std::vector<tpp::Instruction> &body,
    const std::string &bindingName,
    bool shadowed)
{
    for (const auto &instr : body)
    {
        if (instructionUsesBinding(instr, bindingName, shadowed))
            return true;
    }
    return false;
}

/// Recursively convert an IR instruction to the render-model instruction tree.
inline RenderInstruction convertInstruction(
    const tpp::Instruction &instr,
    const std::string &activePolicy, int &scope, const tpp::IR &ir,
    const ConvertConfig &cfg)
{
    return std::visit([&](auto &&arg) -> RenderInstruction
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, tpp::EmitInstr>)
        {
            return {EmitData{stringLiteral(arg.text)}};
        }
        else if constexpr (std::is_same_v<T, tpp::EmitExprInstr>)
        {
            std::string effPol = arg.policy.empty() ? activePolicy : arg.policy;

            EmitExprData data;
            data.expr = exprInfoToContext(arg.expr);
            data.useRuntimePolicy = false;

            if (effPol == "none")
            {
                // explicit opt-out — no policy at all
            }
            else if (!effPol.empty())
            {
                if (const auto *policy = findPolicyDefByTag(ir, effPol))
                    data.staticPolicyId = policy->identifier;
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
        else if constexpr (std::is_same_v<T, tpp::BeginCapturedBlockInstr>)
        {
            BeginCapturedBlockData data;
            data.blockIndentInParentBlock = arg.blockIndentInParentBlock;
            RenderInstruction result;
            result.value.emplace<3>(std::move(data));
            return result;
        }
        else if constexpr (std::is_same_v<T, tpp::Instruction_EmitCapturedBlock>)
        {
            RenderInstruction result;
            result.value.emplace<4>();
            return result;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::ForInstr>>)
        {
            int s = scope++;
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;
            const bool preserveCapturedBlock = cfg.preserveCapturedBlockInstructions;
            const auto bodyIndent = analyzeCapturedBlock(*arg->body);

            auto forData = std::make_unique<ForData>();
            forData->scopeId = s;
            forData->collection = exprValueRefToContext(arg->collection);
            forData->elemType = std::make_unique<RenderTypeKind>(typeKindToContext(*arg->elementType));
            forData->varName = arg->varName;
            forData->enumeratorName = arg->enumeratorName;
            if (arg->sep.has_value())
                forData->sepLit = stringLiteral(*arg->sep);
            if (arg->followedBy.has_value())
                forData->followedByLit = stringLiteral(*arg->followedBy);
            if (arg->precededBy.has_value())
                forData->precededByLit = stringLiteral(*arg->precededBy);
            forData->capturesBody = bodyIndent.wrapped;
            forData->bodyBlockIndentInParentBlock = bodyIndent.blockIndentInParentBlock;

            if (arg->alignSpec.has_value())
            {
                auto cellGroups = splitCells(*arg->body);
                forData->numCols = (int)cellGroups.size();
                auto cellsVec = std::make_unique<std::vector<AlignCellInfo>>();
                for (int ci = 0; ci < (int)cellGroups.size(); ++ci)
                {
                    auto cellBody = std::make_unique<std::vector<RenderInstruction>>();
                    for (const auto *bi : cellGroups[ci])
                        cellBody->push_back(convertInstruction(*bi, pol, scope, ir, cfg));
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
                auto bodyVec = std::make_unique<std::vector<RenderInstruction>>();
                const size_t firstIndex = bodyIndent.wrapped ? bodyIndent.firstIndex : (preserveCapturedBlock ? 0 : bodyIndent.firstIndex);
                const size_t pastLastIndex = bodyIndent.wrapped ? bodyIndent.pastLastIndex : (preserveCapturedBlock ? arg->body->size() : bodyIndent.pastLastIndex);
                for (size_t index = firstIndex; index < pastLastIndex; ++index)
                    bodyVec->push_back(convertInstruction((*arg->body)[index], pol, scope, ir, cfg));
                forData->body = std::move(bodyVec);
                forData->numCols = 0;
            }

            RenderInstruction result;
            result.value.emplace<5>(std::move(forData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::IfInstr>>)
        {
            const bool preserveCapturedBlock = cfg.preserveCapturedBlockInstructions;
            const auto thenIndent = analyzeCapturedBlock(*arg->thenBody);
            const auto elseIndent = analyzeCapturedBlock(*arg->elseBody);
            bool elseBodyPresent = preserveCapturedBlock
                ? !arg->elseBody->empty()
                : elseIndent.firstIndex < elseIndent.pastLastIndex;

            auto ifData = std::make_unique<IfData>();
            ifData->condPath = exprInfoPath(arg->condExpr);
            ifData->condIsBool = arg->conditionKind.value.index() == 0;
            ifData->isNegated = arg->negated;

            auto thenVec = std::make_unique<std::vector<RenderInstruction>>();
            const size_t thenFirstIndex = preserveCapturedBlock ? 0 : thenIndent.firstIndex;
            const size_t thenPastLastIndex = preserveCapturedBlock ? arg->thenBody->size() : thenIndent.pastLastIndex;
            for (size_t index = thenFirstIndex; index < thenPastLastIndex; ++index)
                thenVec->push_back(convertInstruction((*arg->thenBody)[index], activePolicy, scope, ir, cfg));
            ifData->thenBody = std::move(thenVec);

            if (elseBodyPresent)
            {
                auto elseVec = std::make_unique<std::vector<RenderInstruction>>();
                const size_t elseFirstIndex = preserveCapturedBlock ? 0 : elseIndent.firstIndex;
                const size_t elsePastLastIndex = preserveCapturedBlock ? arg->elseBody->size() : elseIndent.pastLastIndex;
                for (size_t index = elseFirstIndex; index < elsePastLastIndex; ++index)
                    elseVec->push_back(convertInstruction((*arg->elseBody)[index], activePolicy, scope, ir, cfg));
                ifData->elseBody = std::move(elseVec);
            }

            RenderInstruction result;
            result.value.emplace<6>(std::move(ifData));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<tpp::SwitchInstr>>)
        {
            const bool preserveCapturedBlock = cfg.preserveCapturedBlockInstructions;
            std::string pol = arg->policy.empty() ? activePolicy : arg->policy;

            auto casesVec = std::make_unique<std::vector<CaseData>>();
            for (const auto &c : *arg->cases)
            {
                const auto caseIndent = analyzeCapturedBlock(*c.body);

                auto caseBody = std::make_unique<std::vector<RenderInstruction>>();
                const size_t caseFirstIndex = preserveCapturedBlock ? 0 : caseIndent.firstIndex;
                const size_t casePastLastIndex = preserveCapturedBlock ? c.body->size() : caseIndent.pastLastIndex;
                for (size_t index = caseFirstIndex; index < casePastLastIndex; ++index)
                    caseBody->push_back(convertInstruction((*c.body)[index], pol, scope, ir, cfg));

                CaseData caseData;
                caseData.tag = c.tag;
                caseData.tagLit = stringLiteral(c.tag);
                caseData.bindingName = c.bindingName.has_value() && instructionListUsesBinding(*c.body, c.bindingName->name)
                    ? std::make_optional(c.bindingName->name)
                    : std::nullopt;
                if (!caseBody->empty())
                    caseData.body = std::move(caseBody);
                caseData.variantIndex = c.variantIndex;
                caseData.isRecursivePayload = c.payloadIsRecursive;
                if (c.payloadType)
                    caseData.payloadType = std::make_unique<RenderTypeKind>(typeKindToContext(*c.payloadType));

                casesVec->push_back(std::move(caseData));
            }

            auto switchData = std::make_unique<SwitchData>();
            switchData->expr = exprValueRefToContext(arg->expr);
            switchData->cases = std::move(casesVec);

            RenderInstruction result;
            result.value.emplace<7>(std::move(switchData));
            return result;
        }
        else if constexpr (std::is_same_v<T, tpp::CallInstr>)
        {
            std::vector<RenderValueRef> argsVec;
            for (size_t i = 0; i < arg.arguments.size(); ++i)
                argsVec.push_back(exprValueRefToContext(arg.arguments[i]));

            CallData callData;
            callData.functionName = cfg.functionPrefix + arg.functionName;
            callData.args = std::move(argsVec);
            callData.needsTry = cfg.callNeedsTry && !ir.policies.empty();
            callData.policyArg = makePolicyRef(activePolicy, ir);

            RenderInstruction result;
            result.value.emplace<8>(std::move(callData));
            return result;
        }
        else
        {
            return {EmitData{""}};
        }
    }, instr.value);
}

/// Build the policy context.
inline std::vector<PolicyInfo> buildPolicyContext(const tpp::IR &ir)
{
    std::vector<PolicyInfo> policies;
    for (const auto &pol : ir.policies)
    {
        PolicyInfo pj;
        pj.tagLit = stringLiteral(pol.tag);
        pj.identifier = pol.identifier;
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
                r.compiledReplace.has_value() ? std::make_optional(stringLiteral(*r.compiledReplace)) : std::nullopt
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
    bool needsStatic = false;
    bool callNeedsTry = false;
    bool preserveCapturedBlockInstructions = false;
    std::vector<std::string> includes;

    static BuildFunctionsConfig forCpp(const std::vector<std::string> &includes = {})
    {
        BuildFunctionsConfig cfg;
        cfg.needsStatic = false;
        cfg.callNeedsTry = false;
        cfg.preserveCapturedBlockInstructions = true;
        cfg.includes = includes;
        return cfg;
    }

    static BuildFunctionsConfig forJava()
    {
        BuildFunctionsConfig cfg;
        cfg.needsStatic = true;
        cfg.callNeedsTry = false;
        cfg.preserveCapturedBlockInstructions = true;
        return cfg;
    }

    static BuildFunctionsConfig forSwift(const std::string &namespaceName, bool policiesPresent)
    {
        BuildFunctionsConfig cfg;
        cfg.needsStatic = !namespaceName.empty();
        cfg.callNeedsTry = policiesPresent;
        cfg.preserveCapturedBlockInstructions = true;
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
    cfg.preserveCapturedBlockInstructions = bfCfg.preserveCapturedBlockInstructions;

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
        {
            if (!cfg.preserveCapturedBlockInstructions && isCapturedBlockInstruction(instr))
                continue;
            body->push_back(convertInstruction(instr, fn.policy, scope, ir, cfg));
        }

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
        result.policies = buildPolicyContext(ir);
    result.functionPrefix = functionPrefix;
    result.needsStatic = bfCfg.needsStatic;
    if (!bfCfg.includes.empty())
        result.includes = bfCfg.includes;
    if (!namespaceName.empty())
        result.namespaceName = namespaceName;
    return result;
}

} // namespace codegen
