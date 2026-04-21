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
struct RenderExprInfo;
struct EmitData;
struct EmitExprData;
struct AlignCellInfo;
struct ForData;
struct CaseData;
struct IfData;
struct SwitchData;
struct CallArgInfo;
struct CallData;
struct PolicyReplacementInfo;
struct PolicyRequireInfo;
struct PolicyOutputFilterInfo;
struct PolicyInfo;
struct ParamInfo;
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
struct RenderExprInfo
{
    std::string path;
    std::unique_ptr<RenderTypeKind> type;
    bool isRecursive;
    bool isOptional;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Instruction IR types — for rendering function generation\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct RenderExprInfo\n{\n    path : string;\n    type : RenderTypeKind;\n    isRecursive : bool;\n    isOptional : bool;\n}";
    }
};
struct EmitData
{
    std::string textLit;
    std::string sb;
    static std::string tpp_typedefs() noexcept
    {
        return "struct EmitData\n{\n    textLit : string;\n    sb : string;\n}";
    }
};
struct EmitExprData
{
    RenderExprInfo expr;
    std::string sb;
    std::optional<std::string> staticPolicyLit;
    std::optional<std::string> staticPolicyId;
    bool useRuntimePolicy;
    static std::string tpp_typedefs() noexcept
    {
        return "struct EmitExprData\n{\n    expr : RenderExprInfo;\n    sb : string;\n    staticPolicyLit : optional<string>;\n    staticPolicyId : optional<string>;\n    useRuntimePolicy : bool;\n}";
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
    std::string collPath;
    bool collIsRecursive;
    bool collIsOptional;
    std::unique_ptr<RenderTypeKind> elemType;
    std::string varName;
    std::optional<std::string> enumeratorName;
    std::unique_ptr<std::vector<RenderInstruction>> body;
    std::optional<std::string> sepLit;
    std::optional<std::string> followedByLit;
    std::optional<std::string> precededByLit;
    bool isBlock;
    int insertCol;
    std::string sb;
    std::optional<std::string> alignSpec;
    std::unique_ptr<std::vector<AlignCellInfo>> cells;
    int numCols;
    std::vector<std::string> alignSpecChars;
    bool singleAlignChar;
    static std::string tpp_typedefs() noexcept
    {
        return "struct ForData\n{\n    scopeId : int;\n    collPath : string;\n    collIsRecursive : bool;\n    collIsOptional : bool;\n    elemType : RenderTypeKind;\n    varName : string;\n    enumeratorName : optional<string>;\n    body : optional<list<RenderInstruction>>;\n    sepLit : optional<string>;\n    followedByLit : optional<string>;\n    precededByLit : optional<string>;\n    isBlock : bool;\n    insertCol : int;\n    sb : string;\n    alignSpec : optional<string>;\n    cells : optional<list<AlignCellInfo>>;\n    numCols : int;\n    alignSpecChars : list<string>;\n    singleAlignChar : bool;\n}";
    }
};
struct CaseData
{
    std::string tag;
    std::string tagLit;
    std::optional<std::string> bindingName;
    std::unique_ptr<RenderTypeKind> payloadType;
    std::unique_ptr<std::vector<RenderInstruction>> body;
    int scopeId;
    bool isFirst;
    int variantIndex;
    bool isRecursivePayload;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CaseData\n{\n    tag : string;\n    tagLit : string;\n    bindingName : optional<string>;\n    payloadType : optional<RenderTypeKind>;\n    body : optional<list<RenderInstruction>>;\n    scopeId : int;\n    isFirst : bool;\n    variantIndex : int;\n    isRecursivePayload : bool;\n}";
    }
};
struct IfData
{
    std::string condPath;
    bool condIsBool;
    bool isNegated;
    std::unique_ptr<std::vector<RenderInstruction>> thenBody;
    std::unique_ptr<std::vector<RenderInstruction>> elseBody;
    bool isBlock;
    int insertCol;
    std::string sb;
    int thenScopeId;
    int elseScopeId;
    static std::string tpp_typedefs() noexcept
    {
        return "struct IfData\n{\n    condPath : string;\n    condIsBool : bool;\n    isNegated : bool;\n    thenBody : list<RenderInstruction>;\n    elseBody : optional<list<RenderInstruction>>;\n    isBlock : bool;\n    insertCol : int;\n    sb : string;\n    thenScopeId : int;\n    elseScopeId : int;\n}";
    }
};
struct SwitchData
{
    std::string exprPath;
    bool exprIsRecursive;
    bool exprIsOptional;
    std::string enumTypeName;
    std::unique_ptr<std::vector<CaseData>> cases;
    bool isBlock;
    int insertCol;
    std::string sb;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SwitchData\n{\n    exprPath : string;\n    exprIsRecursive : bool;\n    exprIsOptional : bool;\n    enumTypeName : string;\n    cases : list<CaseData>;\n    isBlock : bool;\n    insertCol : int;\n    sb : string;\n}";
    }
};
struct CallArgInfo
{
    std::string path;
    bool isRecursive;
    bool isOptional;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CallArgInfo\n{\n    path : string;\n    isRecursive : bool;\n    isOptional : bool;\n}";
    }
};
struct CallData
{
    std::string functionName;
    std::vector<CallArgInfo> args;
    std::optional<PolicyRef> policyArg;
    std::string sb;
    bool needsTry;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CallData\n{\n    functionName : string;\n    args : list<CallArgInfo>;\n    policyArg : optional<PolicyRef>;\n    sb : string;\n    needsTry : bool;\n}";
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
    std::string staticModifier;
    std::optional<std::vector<std::string>> includes;
    std::optional<bool> externalRuntime;
    static std::string tpp_typedefs() noexcept
    {
        return "struct RenderFunctionsInput\n{\n    functions : list<RenderFunctionDef>;\n    policies : optional<list<PolicyInfo>>;\n    functionPrefix : string;\n    namespaceName : optional<string>;\n    staticModifier : string;\n    includes : optional<list<string>>;\n    externalRuntime : optional<bool>;\n}";
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
struct RenderInstruction
{
    using Value = std::variant<EmitData, EmitExprData, RenderInstruction_AlignCell, std::unique_ptr<ForData>, std::unique_ptr<IfData>, std::unique_ptr<SwitchData>, CallData>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "enum RenderInstruction\n{\n    Emit(EmitData),\n    EmitExpr(EmitExprData),\n    AlignCell,\n    For(ForData),\n    If(IfData),\n    Switch(SwitchData),\n    Call(CallData)\n}";
    }
};

inline void from_json(const nlohmann::json& j, PolicyRef& v);
inline void to_json(nlohmann::json& j, const PolicyRef& v);
inline void from_json(const nlohmann::json& j, RenderTypeKind& v);
inline void to_json(nlohmann::json& j, const RenderTypeKind& v);
inline void from_json(const nlohmann::json& j, RenderInstruction& v);
inline void to_json(nlohmann::json& j, const RenderInstruction& v);
inline void from_json(const nlohmann::json& j, RenderExprInfo& v);
inline void to_json(nlohmann::json& j, const RenderExprInfo& v);
inline void from_json(const nlohmann::json& j, EmitData& v);
inline void to_json(nlohmann::json& j, const EmitData& v);
inline void from_json(const nlohmann::json& j, EmitExprData& v);
inline void to_json(nlohmann::json& j, const EmitExprData& v);
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
inline void from_json(const nlohmann::json& j, CallArgInfo& v);
inline void to_json(nlohmann::json& j, const CallArgInfo& v);
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
inline void from_json(const nlohmann::json& j, RenderFunctionDef& v);
inline void to_json(nlohmann::json& j, const RenderFunctionDef& v);
inline void from_json(const nlohmann::json& j, RenderFunctionsInput& v);
inline void to_json(nlohmann::json& j, const RenderFunctionsInput& v);

inline void from_json(const nlohmann::json& j, PolicyRef& v)
{
    if (j.contains("Named")) v.value.emplace<0>(j["Named"].get<std::string>());
    else     if (j.contains("Pure")) v.value.emplace<1>();
    else     if (j.contains("Runtime")) v.value.emplace<2>();
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
    else     if (j.contains("Int")) v.value.emplace<1>();
    else     if (j.contains("Bool")) v.value.emplace<2>();
    else     if (j.contains("Named")) v.value.emplace<3>(j["Named"].get<std::string>());
    else     if (j.contains("List")) v.value.emplace<4>(std::make_unique<RenderTypeKind>(j["List"].get<RenderTypeKind>()));
    else     if (j.contains("Optional")) v.value.emplace<5>(std::make_unique<RenderTypeKind>(j["Optional"].get<RenderTypeKind>()));
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
    else     if (j.contains("EmitExpr")) v.value.emplace<1>(j["EmitExpr"].get<EmitExprData>());
    else     if (j.contains("AlignCell")) v.value.emplace<2>();
    else     if (j.contains("For")) v.value.emplace<3>(std::make_unique<ForData>(j["For"].get<ForData>()));
    else     if (j.contains("If")) v.value.emplace<4>(std::make_unique<IfData>(j["If"].get<IfData>()));
    else     if (j.contains("Switch")) v.value.emplace<5>(std::make_unique<SwitchData>(j["Switch"].get<SwitchData>()));
    else     if (j.contains("Call")) v.value.emplace<6>(j["Call"].get<CallData>());
}
inline void to_json(nlohmann::json& j, const RenderInstruction& v)
{
    const char* _tags[] = {"Emit", "EmitExpr", "AlignCell", "For", "If", "Switch", "Call"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, RenderExprInfo& v)
{
    v.path = j.at("path").get<std::string>();
    v.type = std::make_unique<RenderTypeKind>(j.at("type").get<RenderTypeKind>());
    v.isRecursive = j.at("isRecursive").get<bool>();
    v.isOptional = j.at("isOptional").get<bool>();
}
inline void to_json(nlohmann::json& j, const RenderExprInfo& v)
{
    j = nlohmann::json{};
    j["path"] = v.path;
    j["type"] = *v.type;
    j["isRecursive"] = v.isRecursive;
    j["isOptional"] = v.isOptional;
}
inline void from_json(const nlohmann::json& j, EmitData& v)
{
    v.textLit = j.at("textLit").get<std::string>();
    v.sb = j.at("sb").get<std::string>();
}
inline void to_json(nlohmann::json& j, const EmitData& v)
{
    j = nlohmann::json{};
    j["textLit"] = v.textLit;
    j["sb"] = v.sb;
}
inline void from_json(const nlohmann::json& j, EmitExprData& v)
{
    v.expr = j.at("expr").get<RenderExprInfo>();
    v.sb = j.at("sb").get<std::string>();
    if (j.contains("staticPolicyLit") && !j.at("staticPolicyLit").is_null()) v.staticPolicyLit = j.at("staticPolicyLit").get<std::string>();
    if (j.contains("staticPolicyId") && !j.at("staticPolicyId").is_null()) v.staticPolicyId = j.at("staticPolicyId").get<std::string>();
    v.useRuntimePolicy = j.at("useRuntimePolicy").get<bool>();
}
inline void to_json(nlohmann::json& j, const EmitExprData& v)
{
    j = nlohmann::json{};
    j["expr"] = v.expr;
    j["sb"] = v.sb;
    if (v.staticPolicyLit.has_value()) j["staticPolicyLit"] = *v.staticPolicyLit;
    if (v.staticPolicyId.has_value()) j["staticPolicyId"] = *v.staticPolicyId;
    j["useRuntimePolicy"] = v.useRuntimePolicy;
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
    v.collPath = j.at("collPath").get<std::string>();
    v.collIsRecursive = j.at("collIsRecursive").get<bool>();
    v.collIsOptional = j.at("collIsOptional").get<bool>();
    v.elemType = std::make_unique<RenderTypeKind>(j.at("elemType").get<RenderTypeKind>());
    v.varName = j.at("varName").get<std::string>();
    if (j.contains("enumeratorName") && !j.at("enumeratorName").is_null()) v.enumeratorName = j.at("enumeratorName").get<std::string>();
    if (j.contains("body") && !j.at("body").is_null()) v.body = std::make_unique<std::vector<RenderInstruction>>(j.at("body").get<std::vector<RenderInstruction>>());
    if (j.contains("sepLit") && !j.at("sepLit").is_null()) v.sepLit = j.at("sepLit").get<std::string>();
    if (j.contains("followedByLit") && !j.at("followedByLit").is_null()) v.followedByLit = j.at("followedByLit").get<std::string>();
    if (j.contains("precededByLit") && !j.at("precededByLit").is_null()) v.precededByLit = j.at("precededByLit").get<std::string>();
    v.isBlock = j.at("isBlock").get<bool>();
    v.insertCol = j.at("insertCol").get<int>();
    v.sb = j.at("sb").get<std::string>();
    if (j.contains("alignSpec") && !j.at("alignSpec").is_null()) v.alignSpec = j.at("alignSpec").get<std::string>();
    if (j.contains("cells") && !j.at("cells").is_null()) v.cells = std::make_unique<std::vector<AlignCellInfo>>(j.at("cells").get<std::vector<AlignCellInfo>>());
    v.numCols = j.at("numCols").get<int>();
    v.alignSpecChars = j.at("alignSpecChars").get<std::vector<std::string>>();
    v.singleAlignChar = j.at("singleAlignChar").get<bool>();
}
inline void to_json(nlohmann::json& j, const ForData& v)
{
    j = nlohmann::json{};
    j["scopeId"] = v.scopeId;
    j["collPath"] = v.collPath;
    j["collIsRecursive"] = v.collIsRecursive;
    j["collIsOptional"] = v.collIsOptional;
    j["elemType"] = *v.elemType;
    j["varName"] = v.varName;
    if (v.enumeratorName.has_value()) j["enumeratorName"] = *v.enumeratorName;
    if (v.body) j["body"] = *v.body;
    if (v.sepLit.has_value()) j["sepLit"] = *v.sepLit;
    if (v.followedByLit.has_value()) j["followedByLit"] = *v.followedByLit;
    if (v.precededByLit.has_value()) j["precededByLit"] = *v.precededByLit;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["sb"] = v.sb;
    if (v.alignSpec.has_value()) j["alignSpec"] = *v.alignSpec;
    if (v.cells) j["cells"] = *v.cells;
    j["numCols"] = v.numCols;
    j["alignSpecChars"] = v.alignSpecChars;
    j["singleAlignChar"] = v.singleAlignChar;
}
inline void from_json(const nlohmann::json& j, CaseData& v)
{
    v.tag = j.at("tag").get<std::string>();
    v.tagLit = j.at("tagLit").get<std::string>();
    if (j.contains("bindingName") && !j.at("bindingName").is_null()) v.bindingName = j.at("bindingName").get<std::string>();
    if (j.contains("payloadType") && !j.at("payloadType").is_null()) v.payloadType = std::make_unique<RenderTypeKind>(j.at("payloadType").get<RenderTypeKind>());
    if (j.contains("body") && !j.at("body").is_null()) v.body = std::make_unique<std::vector<RenderInstruction>>(j.at("body").get<std::vector<RenderInstruction>>());
    v.scopeId = j.at("scopeId").get<int>();
    v.isFirst = j.at("isFirst").get<bool>();
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
    j["scopeId"] = v.scopeId;
    j["isFirst"] = v.isFirst;
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
    v.isBlock = j.at("isBlock").get<bool>();
    v.insertCol = j.at("insertCol").get<int>();
    v.sb = j.at("sb").get<std::string>();
    v.thenScopeId = j.at("thenScopeId").get<int>();
    v.elseScopeId = j.at("elseScopeId").get<int>();
}
inline void to_json(nlohmann::json& j, const IfData& v)
{
    j = nlohmann::json{};
    j["condPath"] = v.condPath;
    j["condIsBool"] = v.condIsBool;
    j["isNegated"] = v.isNegated;
    j["thenBody"] = *v.thenBody;
    if (v.elseBody) j["elseBody"] = *v.elseBody;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["sb"] = v.sb;
    j["thenScopeId"] = v.thenScopeId;
    j["elseScopeId"] = v.elseScopeId;
}
inline void from_json(const nlohmann::json& j, SwitchData& v)
{
    v.exprPath = j.at("exprPath").get<std::string>();
    v.exprIsRecursive = j.at("exprIsRecursive").get<bool>();
    v.exprIsOptional = j.at("exprIsOptional").get<bool>();
    v.enumTypeName = j.at("enumTypeName").get<std::string>();
    v.cases = std::make_unique<std::vector<CaseData>>(j.at("cases").get<std::vector<CaseData>>());
    v.isBlock = j.at("isBlock").get<bool>();
    v.insertCol = j.at("insertCol").get<int>();
    v.sb = j.at("sb").get<std::string>();
}
inline void to_json(nlohmann::json& j, const SwitchData& v)
{
    j = nlohmann::json{};
    j["exprPath"] = v.exprPath;
    j["exprIsRecursive"] = v.exprIsRecursive;
    j["exprIsOptional"] = v.exprIsOptional;
    j["enumTypeName"] = v.enumTypeName;
    j["cases"] = *v.cases;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["sb"] = v.sb;
}
inline void from_json(const nlohmann::json& j, CallArgInfo& v)
{
    v.path = j.at("path").get<std::string>();
    v.isRecursive = j.at("isRecursive").get<bool>();
    v.isOptional = j.at("isOptional").get<bool>();
}
inline void to_json(nlohmann::json& j, const CallArgInfo& v)
{
    j = nlohmann::json{};
    j["path"] = v.path;
    j["isRecursive"] = v.isRecursive;
    j["isOptional"] = v.isOptional;
}
inline void from_json(const nlohmann::json& j, CallData& v)
{
    v.functionName = j.at("functionName").get<std::string>();
    v.args = j.at("args").get<std::vector<CallArgInfo>>();
    if (j.contains("policyArg") && !j.at("policyArg").is_null()) v.policyArg = j.at("policyArg").get<PolicyRef>();
    v.sb = j.at("sb").get<std::string>();
    v.needsTry = j.at("needsTry").get<bool>();
}
inline void to_json(nlohmann::json& j, const CallData& v)
{
    j = nlohmann::json{};
    j["functionName"] = v.functionName;
    j["args"] = v.args;
    if (v.policyArg.has_value()) j["policyArg"] = *v.policyArg;
    j["sb"] = v.sb;
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
    v.staticModifier = j.at("staticModifier").get<std::string>();
    if (j.contains("includes") && !j.at("includes").is_null()) v.includes = j.at("includes").get<std::vector<std::string>>();
    if (j.contains("externalRuntime") && !j.at("externalRuntime").is_null()) v.externalRuntime = j.at("externalRuntime").get<bool>();
}
inline void to_json(nlohmann::json& j, const RenderFunctionsInput& v)
{
    j = nlohmann::json{};
    j["functions"] = v.functions;
    if (v.policies.has_value()) j["policies"] = *v.policies;
    j["functionPrefix"] = v.functionPrefix;
    if (v.namespaceName.has_value()) j["namespaceName"] = *v.namespaceName;
    j["staticModifier"] = v.staticModifier;
    if (v.includes.has_value()) j["includes"] = *v.includes;
    if (v.externalRuntime.has_value()) j["externalRuntime"] = *v.externalRuntime;
}
} // namespace codegen
