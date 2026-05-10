#pragma once
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>
namespace codegen {
template<typename _TppT>
inline nlohmann::json _tpp_j(const _TppT& v) { return nlohmann::json(v); }
template<typename _TppT>
inline nlohmann::json _tpp_j(const std::unique_ptr<_TppT>& v) { return v ? nlohmann::json(*v) : nlohmann::json(); }

struct PolicyRef;
struct RenderValueRef;
struct RenderExprInfo;
struct EmitData;
struct EmitExprData;
struct BeginCapturedBlockData;
struct AlignCellInfo;
struct ForData;
struct CaseData;
struct IfData;
struct SwitchData;
struct CallData;
struct PolicyReplacementInfo;
struct PolicyRequireInfo;
struct PolicyOutputFilterInfo;
struct PolicyInfo;
struct ParamInfo;
struct SourceFieldDef;
struct SourceStructDef;
struct SourceVariantDef;
struct SourceEnumDef;
struct SourceInput;
struct RenderFunctionDef;
struct RenderFunctionsInput;
struct RenderTypeKind;
struct RenderInstruction;

struct PolicyRef_Pure
{
    friend void to_json(nlohmann::json& j, const PolicyRef_Pure&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, PolicyRef_Pure&) {}
};
struct PolicyRef_Runtime
{
    friend void to_json(nlohmann::json& j, const PolicyRef_Runtime&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, PolicyRef_Runtime&) {}
};
struct PolicyRef
{
    using Value = std::variant<std::string, PolicyRef_Pure, PolicyRef_Runtime>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "enum PolicyRef\n{\n    Named(string),\n    Pure,\n    Runtime\n}";
    }
};
struct RenderValueRef
{
    std::string path;
    bool isRecursive;
    bool isOptional;
    static std::string tpp_typedefs() noexcept
    {
        return "struct RenderValueRef\n{\n    path : string;\n    isRecursive : bool;\n    isOptional : bool;\n}";
    }
};
struct RenderExprInfo
{
    RenderValueRef ref;
    std::unique_ptr<RenderTypeKind> type;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Instruction IR types — for rendering function generation\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct RenderExprInfo\n{\n    ref : RenderValueRef;\n    type : RenderTypeKind;\n}";
    }
};
struct EmitData
{
    std::string textLit;
    static std::string tpp_typedefs() noexcept
    {
        return "struct EmitData\n{\n    textLit : string;\n}";
    }
};
struct EmitExprData
{
    RenderExprInfo expr;
    std::optional<std::string> staticPolicyId;
    bool useRuntimePolicy;
    static std::string tpp_typedefs() noexcept
    {
        return "struct EmitExprData\n{\n    expr : RenderExprInfo;\n    staticPolicyId : optional<string>;\n    useRuntimePolicy : bool;\n}";
    }
};
struct BeginCapturedBlockData
{
    std::optional<int> blockIndentInParentBlock;
    static std::string tpp_typedefs() noexcept
    {
        return "struct BeginCapturedBlockData\n{\n    blockIndentInParentBlock : optional<int>;\n}";
    }
};
struct AlignCellInfo
{
    int cellIndex;
    std::unique_ptr<std::vector<RenderInstruction>> body;
    static std::string tpp_typedefs() noexcept
    {
        return "struct AlignCellInfo\n{\n    cellIndex : int;\n    body : list<RenderInstruction>;\n}";
    }
};
struct ForData
{
    int scopeId;
    RenderValueRef collection;
    std::unique_ptr<RenderTypeKind> elemType;
    std::string varName;
    std::optional<std::string> enumeratorName;
    std::unique_ptr<std::vector<RenderInstruction>> body;
    std::optional<std::string> sepLit;
    std::optional<std::string> followedByLit;
    std::optional<std::string> precededByLit;
    bool capturesBody;
    std::optional<int> bodyBlockIndentInParentBlock;
    std::unique_ptr<std::vector<AlignCellInfo>> cells;
    int numCols;
    std::vector<std::string> alignSpecChars;
    static std::string tpp_typedefs() noexcept
    {
        return "struct ForData\n{\n    scopeId : int;\n    collection : RenderValueRef;\n    elemType : RenderTypeKind;\n    varName : string;\n    enumeratorName : optional<string>;\n    body : optional<list<RenderInstruction>>;\n    sepLit : optional<string>;\n    followedByLit : optional<string>;\n    precededByLit : optional<string>;\n    capturesBody : bool;\n    bodyBlockIndentInParentBlock : optional<int>;\n    cells : optional<list<AlignCellInfo>>;\n    numCols : int;\n    alignSpecChars : list<string>;\n}";
    }
};
struct CaseData
{
    std::string tag;
    std::string tagLit;
    std::optional<std::string> bindingName;
    std::unique_ptr<RenderTypeKind> payloadType;
    std::unique_ptr<std::vector<RenderInstruction>> body;
    int variantIndex;
    bool isRecursivePayload;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CaseData\n{\n    tag : string;\n    tagLit : string;\n    bindingName : optional<string>;\n    payloadType : optional<RenderTypeKind>;\n    body : optional<list<RenderInstruction>>;\n    variantIndex : int;\n    isRecursivePayload : bool;\n}";
    }
};
struct IfData
{
    std::string condPath;
    bool condIsBool;
    bool isNegated;
    std::unique_ptr<std::vector<RenderInstruction>> thenBody;
    std::unique_ptr<std::vector<RenderInstruction>> elseBody;
    static std::string tpp_typedefs() noexcept
    {
        return "struct IfData\n{\n    condPath : string;\n    condIsBool : bool;\n    isNegated : bool;\n    thenBody : list<RenderInstruction>;\n    elseBody : optional<list<RenderInstruction>>;\n}";
    }
};
struct SwitchData
{
    RenderValueRef expr;
    std::unique_ptr<std::vector<CaseData>> cases;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SwitchData\n{\n    expr : RenderValueRef;\n    cases : list<CaseData>;\n}";
    }
};
struct CallData
{
    std::string functionName;
    std::vector<RenderValueRef> args;
    std::optional<PolicyRef> policyArg;
    bool needsTry;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CallData\n{\n    functionName : string;\n    args : list<RenderValueRef>;\n    policyArg : optional<PolicyRef>;\n    needsTry : bool;\n}";
    }
};
struct PolicyReplacementInfo
{
    std::string findLit;
    std::string replaceLit;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Policy types\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct PolicyReplacementInfo\n{\n    findLit : string;\n    replaceLit : string;\n}";
    }
};
struct PolicyRequireInfo
{
    std::string regexLit;
    std::optional<std::string> replaceLit;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyRequireInfo\n{\n    regexLit : string;\n    replaceLit : optional<string>;\n}";
    }
};
struct PolicyOutputFilterInfo
{
    std::string regexLit;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyOutputFilterInfo\n{\n    regexLit : string;\n}";
    }
};
struct PolicyInfo
{
    std::string tagLit;
    std::string identifier;
    std::optional<std::string> minVal;
    std::optional<std::string> maxVal;
    std::optional<std::string> rejectIfRegexLit;
    std::optional<std::string> rejectMsgLit;
    std::optional<std::vector<PolicyRequireInfo>> require;
    std::optional<std::vector<PolicyReplacementInfo>> replacements;
    std::optional<std::vector<PolicyOutputFilterInfo>> outputFilter;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyInfo\n{\n    tagLit : string;\n    identifier : string;\n    minVal : optional<string>;\n    maxVal : optional<string>;\n    rejectIfRegexLit : optional<string>;\n    rejectMsgLit : optional<string>;\n    require : optional<list<PolicyRequireInfo>>;\n    replacements : optional<list<PolicyReplacementInfo>>;\n    outputFilter : optional<list<PolicyOutputFilterInfo>>;\n}";
    }
};
struct ParamInfo
{
    std::string name;
    std::unique_ptr<RenderTypeKind> type;
    static std::string tpp_typedefs() noexcept
    {
        return "struct ParamInfo\n{\n    name : string;\n    type : RenderTypeKind;\n}";
    }
};
struct SourceFieldDef
{
    std::string name;
    std::unique_ptr<RenderTypeKind> type;
    bool recursive;
    std::optional<std::string> docComment;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Declaration-source context (shared across source-generating backends)\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct SourceFieldDef\n{\n    name : string;\n    type : RenderTypeKind;\n    recursive : bool;\n    docComment : optional<string>;\n}";
    }
};
struct SourceStructDef
{
    std::string name;
    std::vector<SourceFieldDef> fields;
    std::optional<std::string> docComment;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SourceStructDef\n{\n    name : string;\n    fields : list<SourceFieldDef>;\n    docComment : optional<string>;\n}";
    }
};
struct SourceVariantDef
{
    std::string tag;
    std::unique_ptr<RenderTypeKind> payload;
    std::optional<std::string> docComment;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SourceVariantDef\n{\n    tag : string;\n    payload : optional<RenderTypeKind>;\n    docComment : optional<string>;\n}";
    }
};
struct SourceEnumDef
{
    std::string name;
    std::vector<SourceVariantDef> variants;
    std::optional<std::string> docComment;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SourceEnumDef\n{\n    name : string;\n    variants : list<SourceVariantDef>;\n    docComment : optional<string>;\n}";
    }
};
struct SourceInput
{
    std::vector<SourceEnumDef> enums;
    std::vector<SourceStructDef> structs;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SourceInput\n{\n    enums : list<SourceEnumDef>;\n    structs : list<SourceStructDef>;\n}";
    }
};
struct RenderFunctionDef
{
    std::string name;
    std::vector<ParamInfo> params;
    std::unique_ptr<std::vector<RenderInstruction>> body;
    std::optional<std::string> doc;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Function rendering context (shared across all backends)\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct RenderFunctionDef\n{\n    name : string;\n    params : list<ParamInfo>;\n    body : list<RenderInstruction>;\n    doc : optional<string>;\n}";
    }
};
struct RenderFunctionsInput
{
    std::vector<RenderFunctionDef> functions;
    std::optional<std::vector<PolicyInfo>> policies;
    std::string functionPrefix;
    std::optional<std::string> namespaceName;
    bool needsStatic;
    std::optional<std::vector<std::string>> includes;
    static std::string tpp_typedefs() noexcept
    {
        return "struct RenderFunctionsInput\n{\n    functions : list<RenderFunctionDef>;\n    policies : optional<list<PolicyInfo>>;\n    functionPrefix : string;\n    namespaceName : optional<string>;\n    needsStatic : bool;\n    includes : optional<list<string>>;\n}";
    }
};
struct RenderTypeKind_Str
{
    friend void to_json(nlohmann::json& j, const RenderTypeKind_Str&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, RenderTypeKind_Str&) {}
};
struct RenderTypeKind_Int
{
    friend void to_json(nlohmann::json& j, const RenderTypeKind_Int&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, RenderTypeKind_Int&) {}
};
struct RenderTypeKind_Bool
{
    friend void to_json(nlohmann::json& j, const RenderTypeKind_Bool&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, RenderTypeKind_Bool&) {}
};
struct RenderTypeKind
{
    using Value = std::variant<RenderTypeKind_Str, RenderTypeKind_Int, RenderTypeKind_Bool, std::string, std::unique_ptr<RenderTypeKind>, std::unique_ptr<RenderTypeKind>>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "// Shared render-model definitions for all tpp code generation backends.\n// These types are not semantic IR; they are the execution-oriented adapter\n// used only for lowering template instruction trees into backend emitters.\n\nenum RenderTypeKind\n{\n    Str,\n    Int,\n    Bool,\n    Named(string),\n    List(RenderTypeKind),\n    Optional(RenderTypeKind)\n}";
    }
};
struct RenderInstruction_AlignCell
{
    friend void to_json(nlohmann::json& j, const RenderInstruction_AlignCell&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, RenderInstruction_AlignCell&) {}
};
struct RenderInstruction_EmitCapturedBlock
{
    friend void to_json(nlohmann::json& j, const RenderInstruction_EmitCapturedBlock&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, RenderInstruction_EmitCapturedBlock&) {}
};
struct RenderInstruction
{
    using Value = std::variant<EmitData, EmitExprData, RenderInstruction_AlignCell, BeginCapturedBlockData, RenderInstruction_EmitCapturedBlock, std::unique_ptr<ForData>, std::unique_ptr<IfData>, std::unique_ptr<SwitchData>, CallData>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "enum RenderInstruction\n{\n    Emit(EmitData),\n    EmitExpr(EmitExprData),\n    AlignCell,\n    BeginCapturedBlock(BeginCapturedBlockData),\n    EmitCapturedBlock,\n    For(ForData),\n    If(IfData),\n    Switch(SwitchData),\n    Call(CallData)\n}";
    }
};

inline void from_json(const nlohmann::json& j, PolicyRef& v);
inline void to_json(nlohmann::json& j, const PolicyRef& v);
inline void from_json(const nlohmann::json& j, RenderTypeKind& v);
inline void to_json(nlohmann::json& j, const RenderTypeKind& v);
inline void from_json(const nlohmann::json& j, RenderInstruction& v);
inline void to_json(nlohmann::json& j, const RenderInstruction& v);
inline void from_json(const nlohmann::json& j, RenderValueRef& v);
inline void to_json(nlohmann::json& j, const RenderValueRef& v);
inline void from_json(const nlohmann::json& j, RenderExprInfo& v);
inline void to_json(nlohmann::json& j, const RenderExprInfo& v);
inline void from_json(const nlohmann::json& j, EmitData& v);
inline void to_json(nlohmann::json& j, const EmitData& v);
inline void from_json(const nlohmann::json& j, EmitExprData& v);
inline void to_json(nlohmann::json& j, const EmitExprData& v);
inline void from_json(const nlohmann::json& j, BeginCapturedBlockData& v);
inline void to_json(nlohmann::json& j, const BeginCapturedBlockData& v);
inline void from_json(const nlohmann::json& j, AlignCellInfo& v);
inline void to_json(nlohmann::json& j, const AlignCellInfo& v);
inline void from_json(const nlohmann::json& j, ForData& v);
inline void to_json(nlohmann::json& j, const ForData& v);
inline void from_json(const nlohmann::json& j, CaseData& v);
inline void to_json(nlohmann::json& j, const CaseData& v);
inline void from_json(const nlohmann::json& j, IfData& v);
inline void to_json(nlohmann::json& j, const IfData& v);
inline void from_json(const nlohmann::json& j, SwitchData& v);
inline void to_json(nlohmann::json& j, const SwitchData& v);
inline void from_json(const nlohmann::json& j, CallData& v);
inline void to_json(nlohmann::json& j, const CallData& v);
inline void from_json(const nlohmann::json& j, PolicyReplacementInfo& v);
inline void to_json(nlohmann::json& j, const PolicyReplacementInfo& v);
inline void from_json(const nlohmann::json& j, PolicyRequireInfo& v);
inline void to_json(nlohmann::json& j, const PolicyRequireInfo& v);
inline void from_json(const nlohmann::json& j, PolicyOutputFilterInfo& v);
inline void to_json(nlohmann::json& j, const PolicyOutputFilterInfo& v);
inline void from_json(const nlohmann::json& j, PolicyInfo& v);
inline void to_json(nlohmann::json& j, const PolicyInfo& v);
inline void from_json(const nlohmann::json& j, ParamInfo& v);
inline void to_json(nlohmann::json& j, const ParamInfo& v);
inline void from_json(const nlohmann::json& j, SourceFieldDef& v);
inline void to_json(nlohmann::json& j, const SourceFieldDef& v);
inline void from_json(const nlohmann::json& j, SourceStructDef& v);
inline void to_json(nlohmann::json& j, const SourceStructDef& v);
inline void from_json(const nlohmann::json& j, SourceVariantDef& v);
inline void to_json(nlohmann::json& j, const SourceVariantDef& v);
inline void from_json(const nlohmann::json& j, SourceEnumDef& v);
inline void to_json(nlohmann::json& j, const SourceEnumDef& v);
inline void from_json(const nlohmann::json& j, SourceInput& v);
inline void to_json(nlohmann::json& j, const SourceInput& v);
inline void from_json(const nlohmann::json& j, RenderFunctionDef& v);
inline void to_json(nlohmann::json& j, const RenderFunctionDef& v);
inline void from_json(const nlohmann::json& j, RenderFunctionsInput& v);
inline void to_json(nlohmann::json& j, const RenderFunctionsInput& v);

inline void from_json(const nlohmann::json& j, PolicyRef& v)
{
    if (j.contains("Named")) v.value.emplace<0>(j["Named"].get<std::string>());

    else if (j.contains("Pure")) v.value.emplace<1>();

    else if (j.contains("Runtime")) v.value.emplace<2>();
}
inline void to_json(nlohmann::json& j, const PolicyRef& v)
{
    const char* _tags[] = {"Named", "Pure", "Runtime"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, RenderTypeKind& v)
{
    if (j.contains("Str")) v.value.emplace<0>();

    else if (j.contains("Int")) v.value.emplace<1>();

    else if (j.contains("Bool")) v.value.emplace<2>();

    else if (j.contains("Named")) v.value.emplace<3>(j["Named"].get<std::string>());

    else if (j.contains("List")) v.value.emplace<4>(std::make_unique<RenderTypeKind>(j["List"].get<RenderTypeKind>()));

    else if (j.contains("Optional")) v.value.emplace<5>(std::make_unique<RenderTypeKind>(j["Optional"].get<RenderTypeKind>()));
}
inline void to_json(nlohmann::json& j, const RenderTypeKind& v)
{
    const char* _tags[] = {"Str", "Int", "Bool", "Named", "List", "Optional"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, RenderInstruction& v)
{
    if (j.contains("Emit")) v.value.emplace<0>(j["Emit"].get<EmitData>());

    else if (j.contains("EmitExpr")) v.value.emplace<1>(j["EmitExpr"].get<EmitExprData>());

    else if (j.contains("AlignCell")) v.value.emplace<2>();

    else if (j.contains("BeginCapturedBlock")) v.value.emplace<3>(j["BeginCapturedBlock"].get<BeginCapturedBlockData>());

    else if (j.contains("EmitCapturedBlock")) v.value.emplace<4>();

    else if (j.contains("For")) v.value.emplace<5>(std::make_unique<ForData>(j["For"].get<ForData>()));

    else if (j.contains("If")) v.value.emplace<6>(std::make_unique<IfData>(j["If"].get<IfData>()));

    else if (j.contains("Switch")) v.value.emplace<7>(std::make_unique<SwitchData>(j["Switch"].get<SwitchData>()));

    else if (j.contains("Call")) v.value.emplace<8>(j["Call"].get<CallData>());
}
inline void to_json(nlohmann::json& j, const RenderInstruction& v)
{
    const char* _tags[] = {"Emit", "EmitExpr", "AlignCell", "BeginCapturedBlock", "EmitCapturedBlock", "For", "If", "Switch", "Call"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, RenderValueRef& v)
{
    v.path = j.at("path").get<std::string>();
    v.isRecursive = j.at("isRecursive").get<bool>();
    v.isOptional = j.at("isOptional").get<bool>();
}
inline void to_json(nlohmann::json& j, const RenderValueRef& v)
{
    j = nlohmann::json{};
    j["path"] = v.path;
    j["isRecursive"] = v.isRecursive;
    j["isOptional"] = v.isOptional;
}
inline void from_json(const nlohmann::json& j, RenderExprInfo& v)
{
    v.ref = j.at("ref").get<RenderValueRef>();
    v.type = std::make_unique<RenderTypeKind>(j.at("type").get<RenderTypeKind>());
}
inline void to_json(nlohmann::json& j, const RenderExprInfo& v)
{
    j = nlohmann::json{};
    j["ref"] = v.ref;
    j["type"] = *v.type;
}
inline void from_json(const nlohmann::json& j, EmitData& v)
{
    v.textLit = j.at("textLit").get<std::string>();
}
inline void to_json(nlohmann::json& j, const EmitData& v)
{
    j = nlohmann::json{};
    j["textLit"] = v.textLit;
}
inline void from_json(const nlohmann::json& j, EmitExprData& v)
{
    v.expr = j.at("expr").get<RenderExprInfo>();
    if (j.contains("staticPolicyId") && !j.at("staticPolicyId").is_null()) v.staticPolicyId = j.at("staticPolicyId").get<std::string>();
    v.useRuntimePolicy = j.at("useRuntimePolicy").get<bool>();
}
inline void to_json(nlohmann::json& j, const EmitExprData& v)
{
    j = nlohmann::json{};
    j["expr"] = v.expr;
    if (v.staticPolicyId.has_value()) j["staticPolicyId"] = *v.staticPolicyId;
    j["useRuntimePolicy"] = v.useRuntimePolicy;
}
inline void from_json(const nlohmann::json& j, BeginCapturedBlockData& v)
{
    if (j.contains("blockIndentInParentBlock") && !j.at("blockIndentInParentBlock").is_null()) v.blockIndentInParentBlock = j.at("blockIndentInParentBlock").get<int>();
}
inline void to_json(nlohmann::json& j, const BeginCapturedBlockData& v)
{
    j = nlohmann::json{};
    if (v.blockIndentInParentBlock.has_value()) j["blockIndentInParentBlock"] = *v.blockIndentInParentBlock;
}
inline void from_json(const nlohmann::json& j, AlignCellInfo& v)
{
    v.cellIndex = j.at("cellIndex").get<int>();
    v.body = std::make_unique<std::vector<RenderInstruction>>(j.at("body").get<std::vector<RenderInstruction>>());
}
inline void to_json(nlohmann::json& j, const AlignCellInfo& v)
{
    j = nlohmann::json{};
    j["cellIndex"] = v.cellIndex;
    j["body"] = *v.body;
}
inline void from_json(const nlohmann::json& j, ForData& v)
{
    v.scopeId = j.at("scopeId").get<int>();
    v.collection = j.at("collection").get<RenderValueRef>();
    v.elemType = std::make_unique<RenderTypeKind>(j.at("elemType").get<RenderTypeKind>());
    v.varName = j.at("varName").get<std::string>();
    if (j.contains("enumeratorName") && !j.at("enumeratorName").is_null()) v.enumeratorName = j.at("enumeratorName").get<std::string>();
    if (j.contains("body") && !j.at("body").is_null()) v.body = std::make_unique<std::vector<RenderInstruction>>(j.at("body").get<std::vector<RenderInstruction>>());
    if (j.contains("sepLit") && !j.at("sepLit").is_null()) v.sepLit = j.at("sepLit").get<std::string>();
    if (j.contains("followedByLit") && !j.at("followedByLit").is_null()) v.followedByLit = j.at("followedByLit").get<std::string>();
    if (j.contains("precededByLit") && !j.at("precededByLit").is_null()) v.precededByLit = j.at("precededByLit").get<std::string>();
    v.capturesBody = j.at("capturesBody").get<bool>();
    if (j.contains("bodyBlockIndentInParentBlock") && !j.at("bodyBlockIndentInParentBlock").is_null()) v.bodyBlockIndentInParentBlock = j.at("bodyBlockIndentInParentBlock").get<int>();
    if (j.contains("cells") && !j.at("cells").is_null()) v.cells = std::make_unique<std::vector<AlignCellInfo>>(j.at("cells").get<std::vector<AlignCellInfo>>());
    v.numCols = j.at("numCols").get<int>();
    v.alignSpecChars = j.at("alignSpecChars").get<std::vector<std::string>>();
}
inline void to_json(nlohmann::json& j, const ForData& v)
{
    j = nlohmann::json{};
    j["scopeId"] = v.scopeId;
    j["collection"] = v.collection;
    j["elemType"] = *v.elemType;
    j["varName"] = v.varName;
    if (v.enumeratorName.has_value()) j["enumeratorName"] = *v.enumeratorName;
    if (v.body) j["body"] = *v.body;
    if (v.sepLit.has_value()) j["sepLit"] = *v.sepLit;
    if (v.followedByLit.has_value()) j["followedByLit"] = *v.followedByLit;
    if (v.precededByLit.has_value()) j["precededByLit"] = *v.precededByLit;
    j["capturesBody"] = v.capturesBody;
    if (v.bodyBlockIndentInParentBlock.has_value()) j["bodyBlockIndentInParentBlock"] = *v.bodyBlockIndentInParentBlock;
    if (v.cells) j["cells"] = *v.cells;
    j["numCols"] = v.numCols;
    j["alignSpecChars"] = v.alignSpecChars;
}
inline void from_json(const nlohmann::json& j, CaseData& v)
{
    v.tag = j.at("tag").get<std::string>();
    v.tagLit = j.at("tagLit").get<std::string>();
    if (j.contains("bindingName") && !j.at("bindingName").is_null()) v.bindingName = j.at("bindingName").get<std::string>();
    if (j.contains("payloadType") && !j.at("payloadType").is_null()) v.payloadType = std::make_unique<RenderTypeKind>(j.at("payloadType").get<RenderTypeKind>());
    if (j.contains("body") && !j.at("body").is_null()) v.body = std::make_unique<std::vector<RenderInstruction>>(j.at("body").get<std::vector<RenderInstruction>>());
    v.variantIndex = j.at("variantIndex").get<int>();
    v.isRecursivePayload = j.at("isRecursivePayload").get<bool>();
}
inline void to_json(nlohmann::json& j, const CaseData& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    j["tagLit"] = v.tagLit;
    if (v.bindingName.has_value()) j["bindingName"] = *v.bindingName;
    if (v.payloadType) j["payloadType"] = *v.payloadType;
    if (v.body) j["body"] = *v.body;
    j["variantIndex"] = v.variantIndex;
    j["isRecursivePayload"] = v.isRecursivePayload;
}
inline void from_json(const nlohmann::json& j, IfData& v)
{
    v.condPath = j.at("condPath").get<std::string>();
    v.condIsBool = j.at("condIsBool").get<bool>();
    v.isNegated = j.at("isNegated").get<bool>();
    v.thenBody = std::make_unique<std::vector<RenderInstruction>>(j.at("thenBody").get<std::vector<RenderInstruction>>());
    if (j.contains("elseBody") && !j.at("elseBody").is_null()) v.elseBody = std::make_unique<std::vector<RenderInstruction>>(j.at("elseBody").get<std::vector<RenderInstruction>>());
}
inline void to_json(nlohmann::json& j, const IfData& v)
{
    j = nlohmann::json{};
    j["condPath"] = v.condPath;
    j["condIsBool"] = v.condIsBool;
    j["isNegated"] = v.isNegated;
    j["thenBody"] = *v.thenBody;
    if (v.elseBody) j["elseBody"] = *v.elseBody;
}
inline void from_json(const nlohmann::json& j, SwitchData& v)
{
    v.expr = j.at("expr").get<RenderValueRef>();
    v.cases = std::make_unique<std::vector<CaseData>>(j.at("cases").get<std::vector<CaseData>>());
}
inline void to_json(nlohmann::json& j, const SwitchData& v)
{
    j = nlohmann::json{};
    j["expr"] = v.expr;
    j["cases"] = *v.cases;
}
inline void from_json(const nlohmann::json& j, CallData& v)
{
    v.functionName = j.at("functionName").get<std::string>();
    v.args = j.at("args").get<std::vector<RenderValueRef>>();
    if (j.contains("policyArg") && !j.at("policyArg").is_null()) v.policyArg = j.at("policyArg").get<PolicyRef>();
    v.needsTry = j.at("needsTry").get<bool>();
}
inline void to_json(nlohmann::json& j, const CallData& v)
{
    j = nlohmann::json{};
    j["functionName"] = v.functionName;
    j["args"] = v.args;
    if (v.policyArg.has_value()) j["policyArg"] = *v.policyArg;
    j["needsTry"] = v.needsTry;
}
inline void from_json(const nlohmann::json& j, PolicyReplacementInfo& v)
{
    v.findLit = j.at("findLit").get<std::string>();
    v.replaceLit = j.at("replaceLit").get<std::string>();
}
inline void to_json(nlohmann::json& j, const PolicyReplacementInfo& v)
{
    j = nlohmann::json{};
    j["findLit"] = v.findLit;
    j["replaceLit"] = v.replaceLit;
}
inline void from_json(const nlohmann::json& j, PolicyRequireInfo& v)
{
    v.regexLit = j.at("regexLit").get<std::string>();
    if (j.contains("replaceLit") && !j.at("replaceLit").is_null()) v.replaceLit = j.at("replaceLit").get<std::string>();
}
inline void to_json(nlohmann::json& j, const PolicyRequireInfo& v)
{
    j = nlohmann::json{};
    j["regexLit"] = v.regexLit;
    if (v.replaceLit.has_value()) j["replaceLit"] = *v.replaceLit;
}
inline void from_json(const nlohmann::json& j, PolicyOutputFilterInfo& v)
{
    v.regexLit = j.at("regexLit").get<std::string>();
}
inline void to_json(nlohmann::json& j, const PolicyOutputFilterInfo& v)
{
    j = nlohmann::json{};
    j["regexLit"] = v.regexLit;
}
inline void from_json(const nlohmann::json& j, PolicyInfo& v)
{
    v.tagLit = j.at("tagLit").get<std::string>();
    v.identifier = j.at("identifier").get<std::string>();
    if (j.contains("minVal") && !j.at("minVal").is_null()) v.minVal = j.at("minVal").get<std::string>();
    if (j.contains("maxVal") && !j.at("maxVal").is_null()) v.maxVal = j.at("maxVal").get<std::string>();
    if (j.contains("rejectIfRegexLit") && !j.at("rejectIfRegexLit").is_null()) v.rejectIfRegexLit = j.at("rejectIfRegexLit").get<std::string>();
    if (j.contains("rejectMsgLit") && !j.at("rejectMsgLit").is_null()) v.rejectMsgLit = j.at("rejectMsgLit").get<std::string>();
    if (j.contains("require") && !j.at("require").is_null()) v.require = j.at("require").get<std::vector<PolicyRequireInfo>>();
    if (j.contains("replacements") && !j.at("replacements").is_null()) v.replacements = j.at("replacements").get<std::vector<PolicyReplacementInfo>>();
    if (j.contains("outputFilter") && !j.at("outputFilter").is_null()) v.outputFilter = j.at("outputFilter").get<std::vector<PolicyOutputFilterInfo>>();
}
inline void to_json(nlohmann::json& j, const PolicyInfo& v)
{
    j = nlohmann::json{};
    j["tagLit"] = v.tagLit;
    j["identifier"] = v.identifier;
    if (v.minVal.has_value()) j["minVal"] = *v.minVal;
    if (v.maxVal.has_value()) j["maxVal"] = *v.maxVal;
    if (v.rejectIfRegexLit.has_value()) j["rejectIfRegexLit"] = *v.rejectIfRegexLit;
    if (v.rejectMsgLit.has_value()) j["rejectMsgLit"] = *v.rejectMsgLit;
    if (v.require.has_value()) j["require"] = *v.require;
    if (v.replacements.has_value()) j["replacements"] = *v.replacements;
    if (v.outputFilter.has_value()) j["outputFilter"] = *v.outputFilter;
}
inline void from_json(const nlohmann::json& j, ParamInfo& v)
{
    v.name = j.at("name").get<std::string>();
    v.type = std::make_unique<RenderTypeKind>(j.at("type").get<RenderTypeKind>());
}
inline void to_json(nlohmann::json& j, const ParamInfo& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["type"] = *v.type;
}
inline void from_json(const nlohmann::json& j, SourceFieldDef& v)
{
    v.name = j.at("name").get<std::string>();
    v.type = std::make_unique<RenderTypeKind>(j.at("type").get<RenderTypeKind>());
    v.recursive = j.at("recursive").get<bool>();
    if (j.contains("docComment") && !j.at("docComment").is_null()) v.docComment = j.at("docComment").get<std::string>();
}
inline void to_json(nlohmann::json& j, const SourceFieldDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["type"] = *v.type;
    j["recursive"] = v.recursive;
    if (v.docComment.has_value()) j["docComment"] = *v.docComment;
}
inline void from_json(const nlohmann::json& j, SourceStructDef& v)
{
    v.name = j.at("name").get<std::string>();
    v.fields = j.at("fields").get<std::vector<SourceFieldDef>>();
    if (j.contains("docComment") && !j.at("docComment").is_null()) v.docComment = j.at("docComment").get<std::string>();
}
inline void to_json(nlohmann::json& j, const SourceStructDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["fields"] = v.fields;
    if (v.docComment.has_value()) j["docComment"] = *v.docComment;
}
inline void from_json(const nlohmann::json& j, SourceVariantDef& v)
{
    v.tag = j.at("tag").get<std::string>();
    if (j.contains("payload") && !j.at("payload").is_null()) v.payload = std::make_unique<RenderTypeKind>(j.at("payload").get<RenderTypeKind>());
    if (j.contains("docComment") && !j.at("docComment").is_null()) v.docComment = j.at("docComment").get<std::string>();
}
inline void to_json(nlohmann::json& j, const SourceVariantDef& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    if (v.payload) j["payload"] = *v.payload;
    if (v.docComment.has_value()) j["docComment"] = *v.docComment;
}
inline void from_json(const nlohmann::json& j, SourceEnumDef& v)
{
    v.name = j.at("name").get<std::string>();
    v.variants = j.at("variants").get<std::vector<SourceVariantDef>>();
    if (j.contains("docComment") && !j.at("docComment").is_null()) v.docComment = j.at("docComment").get<std::string>();
}
inline void to_json(nlohmann::json& j, const SourceEnumDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["variants"] = v.variants;
    if (v.docComment.has_value()) j["docComment"] = *v.docComment;
}
inline void from_json(const nlohmann::json& j, SourceInput& v)
{
    v.enums = j.at("enums").get<std::vector<SourceEnumDef>>();
    v.structs = j.at("structs").get<std::vector<SourceStructDef>>();
}
inline void to_json(nlohmann::json& j, const SourceInput& v)
{
    j = nlohmann::json{};
    j["enums"] = v.enums;
    j["structs"] = v.structs;
}
inline void from_json(const nlohmann::json& j, RenderFunctionDef& v)
{
    v.name = j.at("name").get<std::string>();
    v.params = j.at("params").get<std::vector<ParamInfo>>();
    v.body = std::make_unique<std::vector<RenderInstruction>>(j.at("body").get<std::vector<RenderInstruction>>());
    if (j.contains("doc") && !j.at("doc").is_null()) v.doc = j.at("doc").get<std::string>();
}
inline void to_json(nlohmann::json& j, const RenderFunctionDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["params"] = v.params;
    j["body"] = *v.body;
    if (v.doc.has_value()) j["doc"] = *v.doc;
}
inline void from_json(const nlohmann::json& j, RenderFunctionsInput& v)
{
    v.functions = j.at("functions").get<std::vector<RenderFunctionDef>>();
    if (j.contains("policies") && !j.at("policies").is_null()) v.policies = j.at("policies").get<std::vector<PolicyInfo>>();
    v.functionPrefix = j.at("functionPrefix").get<std::string>();
    if (j.contains("namespaceName") && !j.at("namespaceName").is_null()) v.namespaceName = j.at("namespaceName").get<std::string>();
    v.needsStatic = j.at("needsStatic").get<bool>();
    if (j.contains("includes") && !j.at("includes").is_null()) v.includes = j.at("includes").get<std::vector<std::string>>();
}
inline void to_json(nlohmann::json& j, const RenderFunctionsInput& v)
{
    j = nlohmann::json{};
    j["functions"] = v.functions;
    if (v.policies.has_value()) j["policies"] = *v.policies;
    j["functionPrefix"] = v.functionPrefix;
    if (v.namespaceName.has_value()) j["namespaceName"] = *v.namespaceName;
    j["needsStatic"] = v.needsStatic;
    if (v.includes.has_value()) j["includes"] = *v.includes;
}
} // namespace codegen
