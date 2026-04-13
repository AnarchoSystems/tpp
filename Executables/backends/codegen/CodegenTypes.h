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

struct TypeKind;
struct PolicyRef;
struct Instruction;
struct ExprInfo;
struct EmitData;
struct EmitExprData;
struct AlignCellInfo;
struct ForData;
struct CaseData;
struct IfData;
struct SwitchData;
struct CallArgInfo;
struct CallData;
struct RenderViaOverload;
struct RenderViaData;
struct PolicyReplacementInfo;
struct PolicyRequireInfo;
struct PolicyOutputFilterInfo;
struct PolicyInfo;
struct FieldInfo;
struct StructInfo;
struct VariantInfo;
struct EnumInfo;
struct ParamInfo;
struct FunctionInfo;
struct CodegenInput;
struct RenderFunctionDef;
struct RenderFunctionsInput;

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
struct ExprInfo
{
    std::string path;
    std::unique_ptr<TypeKind> type;
    bool isRecursive;
    bool isOptional;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Instruction IR types — for rendering function generation\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct ExprInfo\n{\n    path : string;\n    type : TypeKind;\n    isRecursive : bool;\n    isOptional : bool;\n}";
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
    ExprInfo expr;
    std::string sb;
    std::optional<std::string> staticPolicyLit;
    std::optional<std::string> staticPolicyId;
    bool useRuntimePolicy;
    static std::string tpp_typedefs() noexcept
    {
        return "struct EmitExprData\n{\n    expr : ExprInfo;\n    sb : string;\n    staticPolicyLit : optional<string>;\n    staticPolicyId : optional<string>;\n    useRuntimePolicy : bool;\n}";
    }
};
struct AlignCellInfo
{
    int cellIndex;
    std::unique_ptr<std::vector<Instruction>> body;
    static std::string tpp_typedefs() noexcept
    {
        return "struct AlignCellInfo\n{\n    cellIndex : int;\n    body : list<Instruction>;\n}";
    }
};
struct ForData
{
    int scopeId;
    std::string collPath;
    std::unique_ptr<TypeKind> elemType;
    std::string varName;
    bool hasEnum;
    std::string enumeratorName;
    std::unique_ptr<std::vector<Instruction>> body;
    bool hasSep;
    std::string sepLit;
    bool hasFollowed;
    std::string followedByLit;
    bool hasPreceded;
    std::string precededByLit;
    bool isBlock;
    int insertCol;
    std::string sb;
    bool hasAlign;
    std::string alignSpec;
    std::unique_ptr<std::vector<AlignCellInfo>> cells;
    int numCols;
    std::vector<std::string> alignSpecChars;
    bool singleAlignChar;
    static std::string tpp_typedefs() noexcept
    {
        return "struct ForData\n{\n    scopeId : int;\n    collPath : string;\n    elemType : TypeKind;\n    varName : string;\n    hasEnum : bool;\n    enumeratorName : string;\n    body : list<Instruction>;\n    hasSep : bool;\n    sepLit : string;\n    hasFollowed : bool;\n    followedByLit : string;\n    hasPreceded : bool;\n    precededByLit : string;\n    isBlock : bool;\n    insertCol : int;\n    sb : string;\n    hasAlign : bool;\n    alignSpec : string;\n    cells : list<AlignCellInfo>;\n    numCols : int;\n    alignSpecChars : list<string>;\n    singleAlignChar : bool;\n}";
    }
};
struct CaseData
{
    std::string tag;
    std::string tagLit;
    std::string bindingName;
    bool hasBinding;
    std::unique_ptr<TypeKind> payloadType;
    std::unique_ptr<std::vector<Instruction>> body;
    int scopeId;
    bool isFirst;
    int variantIndex;
    bool isRecursivePayload;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CaseData\n{\n    tag : string;\n    tagLit : string;\n    bindingName : string;\n    hasBinding : bool;\n    payloadType : optional<TypeKind>;\n    body : list<Instruction>;\n    scopeId : int;\n    isFirst : bool;\n    variantIndex : int;\n    isRecursivePayload : bool;\n}";
    }
};
struct IfData
{
    std::string condPath;
    bool condIsBool;
    bool isNegated;
    std::unique_ptr<std::vector<Instruction>> thenBody;
    std::unique_ptr<std::vector<Instruction>> elseBody;
    bool hasElse;
    bool isBlock;
    int insertCol;
    std::string sb;
    int thenScopeId;
    int elseScopeId;
    static std::string tpp_typedefs() noexcept
    {
        return "struct IfData\n{\n    condPath : string;\n    condIsBool : bool;\n    isNegated : bool;\n    thenBody : list<Instruction>;\n    elseBody : list<Instruction>;\n    hasElse : bool;\n    isBlock : bool;\n    insertCol : int;\n    sb : string;\n    thenScopeId : int;\n    elseScopeId : int;\n}";
    }
};
struct SwitchData
{
    std::string exprPath;
    std::string enumTypeName;
    std::unique_ptr<std::vector<CaseData>> cases;
    std::unique_ptr<CaseData> defaultCase;
    bool isBlock;
    int insertCol;
    std::string sb;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SwitchData\n{\n    exprPath : string;\n    enumTypeName : string;\n    cases : list<CaseData>;\n    defaultCase : optional<CaseData>;\n    isBlock : bool;\n    insertCol : int;\n    sb : string;\n}";
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
struct RenderViaOverload
{
    std::string tag;
    std::string tagLit;
    bool isFirst;
    std::unique_ptr<TypeKind> payloadType;
    static std::string tpp_typedefs() noexcept
    {
        return "struct RenderViaOverload\n{\n    tag : string;\n    tagLit : string;\n    isFirst : bool;\n    payloadType : optional<TypeKind>;\n}";
    }
};
struct RenderViaData
{
    int scopeId;
    std::string collPath;
    bool isCollection;
    std::unique_ptr<TypeKind> elemType;
    std::string enumTypeName;
    std::string functionName;
    bool hasSep;
    std::string sepLit;
    bool hasFollowed;
    std::string followedByLit;
    bool hasPreceded;
    std::string precededByLit;
    std::string sb;
    bool isSingleOverload;
    std::vector<RenderViaOverload> overloads;
    std::optional<PolicyRef> policyArg;
    bool needsTry;
    static std::string tpp_typedefs() noexcept
    {
        return "struct RenderViaData\n{\n    scopeId : int;\n    collPath : string;\n    isCollection : bool;\n    elemType : TypeKind;\n    enumTypeName : string;\n    functionName : string;\n    hasSep : bool;\n    sepLit : string;\n    hasFollowed : bool;\n    followedByLit : string;\n    hasPreceded : bool;\n    precededByLit : string;\n    sb : string;\n    isSingleOverload : bool;\n    overloads : list<RenderViaOverload>;\n    policyArg : optional<PolicyRef>;\n    needsTry : bool;\n}";
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
    std::string replaceLit;
    bool hasReplace;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyRequireInfo\n{\n    regexLit : string;\n    replaceLit : string;\n    hasReplace : bool;\n}";
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
    bool hasLength;
    std::string minVal;
    std::string maxVal;
    bool hasRejectIf;
    std::string rejectIfRegexLit;
    std::string rejectMsgLit;
    std::vector<PolicyRequireInfo> require;
    bool hasRequire;
    std::vector<PolicyReplacementInfo> replacements;
    bool hasReplacements;
    std::vector<PolicyOutputFilterInfo> outputFilter;
    bool hasOutputFilter;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyInfo\n{\n    tagLit : string;\n    identifier : string;\n    hasLength : bool;\n    minVal : string;\n    maxVal : string;\n    hasRejectIf : bool;\n    rejectIfRegexLit : string;\n    rejectMsgLit : string;\n    require : list<PolicyRequireInfo>;\n    hasRequire : bool;\n    replacements : list<PolicyReplacementInfo>;\n    hasReplacements : bool;\n    outputFilter : list<PolicyOutputFilterInfo>;\n    hasOutputFilter : bool;\n}";
    }
};
struct FieldInfo
{
    std::string name;
    std::unique_ptr<TypeKind> type;
    bool recursive;
    bool isOptional;
    bool recursiveOptional;
    std::unique_ptr<TypeKind> innerType;
    std::optional<std::string> doc;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Codegen input types — shared type-generation data across all backends\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct FieldInfo\n{\n    name : string;\n    type : TypeKind;\n    recursive : bool;\n    isOptional : bool;\n    recursiveOptional : bool;\n    innerType : TypeKind;\n    doc : optional<string>;\n}";
    }
};
struct StructInfo
{
    std::string name;
    std::vector<FieldInfo> fields;
    std::optional<std::string> doc;
    std::string rawTypedefs;
    static std::string tpp_typedefs() noexcept
    {
        return "struct StructInfo\n{\n    name : string;\n    fields : list<FieldInfo>;\n    doc : optional<string>;\n    rawTypedefs : string;\n}";
    }
};
struct VariantInfo
{
    std::string tag;
    std::unique_ptr<TypeKind> payload;
    bool recursive;
    int index;
    std::optional<std::string> doc;
    static std::string tpp_typedefs() noexcept
    {
        return "struct VariantInfo\n{\n    tag : string;\n    payload : optional<TypeKind>;\n    recursive : bool;\n    index : int;\n    doc : optional<string>;\n}";
    }
};
struct EnumInfo
{
    std::string name;
    std::vector<VariantInfo> variants;
    bool hasRecursiveVariants;
    std::optional<std::string> doc;
    std::string rawTypedefs;
    static std::string tpp_typedefs() noexcept
    {
        return "struct EnumInfo\n{\n    name : string;\n    variants : list<VariantInfo>;\n    hasRecursiveVariants : bool;\n    doc : optional<string>;\n    rawTypedefs : string;\n}";
    }
};
struct ParamInfo
{
    std::string name;
    std::unique_ptr<TypeKind> type;
    static std::string tpp_typedefs() noexcept
    {
        return "struct ParamInfo\n{\n    name : string;\n    type : TypeKind;\n}";
    }
};
struct FunctionInfo
{
    std::string name;
    std::vector<ParamInfo> params;
    std::optional<std::string> doc;
    static std::string tpp_typedefs() noexcept
    {
        return "struct FunctionInfo\n{\n    name : string;\n    params : list<ParamInfo>;\n    doc : optional<string>;\n}";
    }
};
struct CodegenInput
{
    std::vector<StructInfo> structs;
    std::vector<EnumInfo> enums;
    std::vector<FunctionInfo> functions;
    bool hasRecursiveTypes;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CodegenInput\n{\n    structs : list<StructInfo>;\n    enums : list<EnumInfo>;\n    functions : list<FunctionInfo>;\n    hasRecursiveTypes : bool;\n}";
    }
};
struct RenderFunctionDef
{
    std::string name;
    std::vector<ParamInfo> params;
    std::unique_ptr<std::vector<Instruction>> body;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// Function rendering context (shared across all backends)\n// ═══════════════════════════════════════════════════════════════════════════════\n\nstruct RenderFunctionDef\n{\n    name : string;\n    params : list<ParamInfo>;\n    body : list<Instruction>;\n}";
    }
};
struct RenderFunctionsInput
{
    std::vector<RenderFunctionDef> functions;
    bool hasPolicies;
    std::vector<PolicyInfo> policies;
    std::string functionPrefix;
    std::optional<std::string> namespaceName;
    std::string staticModifier;
    std::optional<std::vector<std::string>> includes;
    std::optional<bool> externalRuntime;
    static std::string tpp_typedefs() noexcept
    {
        return "struct RenderFunctionsInput\n{\n    functions : list<RenderFunctionDef>;\n    hasPolicies : bool;\n    policies : list<PolicyInfo>;\n    functionPrefix : string;\n    namespaceName : optional<string>;\n    staticModifier : string;\n    includes : optional<list<string>>;\n    externalRuntime : optional<bool>;\n}";
    }
};
struct TypeKind_Str
{
    friend void to_json(nlohmann::json& j, const TypeKind_Str&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, TypeKind_Str&) {}
};
struct TypeKind_Int
{
    friend void to_json(nlohmann::json& j, const TypeKind_Int&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, TypeKind_Int&) {}
};
struct TypeKind_Bool
{
    friend void to_json(nlohmann::json& j, const TypeKind_Bool&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, TypeKind_Bool&) {}
};
struct TypeKind
{
    using Value = std::variant<TypeKind_Str, TypeKind_Int, TypeKind_Bool, std::string, std::unique_ptr<TypeKind>, std::unique_ptr<TypeKind>>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "// Shared type definitions for all tpp code generation backends.\n// Contains instruction IR types and policy types used identically\n// by tpp2swift, tpp2java, and any future backend.\n\nenum TypeKind\n{\n    Str,\n    Int,\n    Bool,\n    Named(string),\n    List(TypeKind),\n    Optional(TypeKind)\n}";
    }
};
struct Instruction_AlignCell
{
    friend void to_json(nlohmann::json& j, const Instruction_AlignCell&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, Instruction_AlignCell&) {}
};
struct Instruction
{
    using Value = std::variant<EmitData, EmitExprData, Instruction_AlignCell, std::unique_ptr<ForData>, std::unique_ptr<IfData>, std::unique_ptr<SwitchData>, CallData, RenderViaData>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "enum Instruction\n{\n    Emit(EmitData),\n    EmitExpr(EmitExprData),\n    AlignCell,\n    For(ForData),\n    If(IfData),\n    Switch(SwitchData),\n    Call(CallData),\n    RenderVia(RenderViaData)\n}";
    }
};

inline void from_json(const nlohmann::json& j, TypeKind& v);
inline void to_json(nlohmann::json& j, const TypeKind& v);
inline void from_json(const nlohmann::json& j, PolicyRef& v);
inline void to_json(nlohmann::json& j, const PolicyRef& v);
inline void from_json(const nlohmann::json& j, Instruction& v);
inline void to_json(nlohmann::json& j, const Instruction& v);
inline void from_json(const nlohmann::json& j, ExprInfo& v);
inline void to_json(nlohmann::json& j, const ExprInfo& v);
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
inline void from_json(const nlohmann::json& j, RenderViaOverload& v);
inline void to_json(nlohmann::json& j, const RenderViaOverload& v);
inline void from_json(const nlohmann::json& j, RenderViaData& v);
inline void to_json(nlohmann::json& j, const RenderViaData& v);
inline void from_json(const nlohmann::json& j, PolicyReplacementInfo& v);
inline void to_json(nlohmann::json& j, const PolicyReplacementInfo& v);
inline void from_json(const nlohmann::json& j, PolicyRequireInfo& v);
inline void to_json(nlohmann::json& j, const PolicyRequireInfo& v);
inline void from_json(const nlohmann::json& j, PolicyOutputFilterInfo& v);
inline void to_json(nlohmann::json& j, const PolicyOutputFilterInfo& v);
inline void from_json(const nlohmann::json& j, PolicyInfo& v);
inline void to_json(nlohmann::json& j, const PolicyInfo& v);
inline void from_json(const nlohmann::json& j, FieldInfo& v);
inline void to_json(nlohmann::json& j, const FieldInfo& v);
inline void from_json(const nlohmann::json& j, StructInfo& v);
inline void to_json(nlohmann::json& j, const StructInfo& v);
inline void from_json(const nlohmann::json& j, VariantInfo& v);
inline void to_json(nlohmann::json& j, const VariantInfo& v);
inline void from_json(const nlohmann::json& j, EnumInfo& v);
inline void to_json(nlohmann::json& j, const EnumInfo& v);
inline void from_json(const nlohmann::json& j, ParamInfo& v);
inline void to_json(nlohmann::json& j, const ParamInfo& v);
inline void from_json(const nlohmann::json& j, FunctionInfo& v);
inline void to_json(nlohmann::json& j, const FunctionInfo& v);
inline void from_json(const nlohmann::json& j, CodegenInput& v);
inline void to_json(nlohmann::json& j, const CodegenInput& v);
inline void from_json(const nlohmann::json& j, RenderFunctionDef& v);
inline void to_json(nlohmann::json& j, const RenderFunctionDef& v);
inline void from_json(const nlohmann::json& j, RenderFunctionsInput& v);
inline void to_json(nlohmann::json& j, const RenderFunctionsInput& v);

inline void from_json(const nlohmann::json& j, TypeKind& v)
{
    if (j.contains("Str")) v.value.emplace<0>();
    else     if (j.contains("Int")) v.value.emplace<1>();
    else     if (j.contains("Bool")) v.value.emplace<2>();
    else     if (j.contains("Named")) v.value.emplace<3>(j["Named"].get<std::string>());
    else     if (j.contains("List")) v.value.emplace<4>(std::make_unique<TypeKind>(j["List"].get<TypeKind>()));
    else     if (j.contains("Optional")) v.value.emplace<5>(std::make_unique<TypeKind>(j["Optional"].get<TypeKind>()));
}
inline void to_json(nlohmann::json& j, const TypeKind& v)
{
    const char* _tags[] = {"Str", "Int", "Bool", "Named", "List", "Optional"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
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
inline void from_json(const nlohmann::json& j, Instruction& v)
{
    if (j.contains("Emit")) v.value.emplace<0>(j["Emit"].get<EmitData>());
    else     if (j.contains("EmitExpr")) v.value.emplace<1>(j["EmitExpr"].get<EmitExprData>());
    else     if (j.contains("AlignCell")) v.value.emplace<2>();
    else     if (j.contains("For")) v.value.emplace<3>(std::make_unique<ForData>(j["For"].get<ForData>()));
    else     if (j.contains("If")) v.value.emplace<4>(std::make_unique<IfData>(j["If"].get<IfData>()));
    else     if (j.contains("Switch")) v.value.emplace<5>(std::make_unique<SwitchData>(j["Switch"].get<SwitchData>()));
    else     if (j.contains("Call")) v.value.emplace<6>(j["Call"].get<CallData>());
    else     if (j.contains("RenderVia")) v.value.emplace<7>(j["RenderVia"].get<RenderViaData>());
}
inline void to_json(nlohmann::json& j, const Instruction& v)
{
    const char* _tags[] = {"Emit", "EmitExpr", "AlignCell", "For", "If", "Switch", "Call", "RenderVia"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, ExprInfo& v)
{
    j.at("path").get_to(v.path);
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
    j.at("isRecursive").get_to(v.isRecursive);
    j.at("isOptional").get_to(v.isOptional);
}
inline void to_json(nlohmann::json& j, const ExprInfo& v)
{
    j = nlohmann::json{};
    j["path"] = v.path;
    j["type"] = *v.type;
    j["isRecursive"] = v.isRecursive;
    j["isOptional"] = v.isOptional;
}
inline void from_json(const nlohmann::json& j, EmitData& v)
{
    j.at("textLit").get_to(v.textLit);
    j.at("sb").get_to(v.sb);
}
inline void to_json(nlohmann::json& j, const EmitData& v)
{
    j = nlohmann::json{};
    j["textLit"] = v.textLit;
    j["sb"] = v.sb;
}
inline void from_json(const nlohmann::json& j, EmitExprData& v)
{
    j.at("expr").get_to(v.expr);
    j.at("sb").get_to(v.sb);
    if (j.contains("staticPolicyLit") && !j.at("staticPolicyLit").is_null()) v.staticPolicyLit = j.at("staticPolicyLit").get<std::string>();
    if (j.contains("staticPolicyId") && !j.at("staticPolicyId").is_null()) v.staticPolicyId = j.at("staticPolicyId").get<std::string>();
    j.at("useRuntimePolicy").get_to(v.useRuntimePolicy);
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
    j.at("cellIndex").get_to(v.cellIndex);
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
}
inline void to_json(nlohmann::json& j, const AlignCellInfo& v)
{
    j = nlohmann::json{};
    j["cellIndex"] = v.cellIndex;
    j["body"] = *v.body;
}
inline void from_json(const nlohmann::json& j, ForData& v)
{
    j.at("scopeId").get_to(v.scopeId);
    j.at("collPath").get_to(v.collPath);
    v.elemType = std::make_unique<TypeKind>(j.at("elemType").get<TypeKind>());
    j.at("varName").get_to(v.varName);
    j.at("hasEnum").get_to(v.hasEnum);
    j.at("enumeratorName").get_to(v.enumeratorName);
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
    j.at("hasSep").get_to(v.hasSep);
    j.at("sepLit").get_to(v.sepLit);
    j.at("hasFollowed").get_to(v.hasFollowed);
    j.at("followedByLit").get_to(v.followedByLit);
    j.at("hasPreceded").get_to(v.hasPreceded);
    j.at("precededByLit").get_to(v.precededByLit);
    j.at("isBlock").get_to(v.isBlock);
    j.at("insertCol").get_to(v.insertCol);
    j.at("sb").get_to(v.sb);
    j.at("hasAlign").get_to(v.hasAlign);
    j.at("alignSpec").get_to(v.alignSpec);
    v.cells = std::make_unique<std::vector<AlignCellInfo>>(j.at("cells").get<std::vector<AlignCellInfo>>());
    j.at("numCols").get_to(v.numCols);
    j.at("alignSpecChars").get_to(v.alignSpecChars);
    j.at("singleAlignChar").get_to(v.singleAlignChar);
}
inline void to_json(nlohmann::json& j, const ForData& v)
{
    j = nlohmann::json{};
    j["scopeId"] = v.scopeId;
    j["collPath"] = v.collPath;
    j["elemType"] = *v.elemType;
    j["varName"] = v.varName;
    j["hasEnum"] = v.hasEnum;
    j["enumeratorName"] = v.enumeratorName;
    j["body"] = *v.body;
    j["hasSep"] = v.hasSep;
    j["sepLit"] = v.sepLit;
    j["hasFollowed"] = v.hasFollowed;
    j["followedByLit"] = v.followedByLit;
    j["hasPreceded"] = v.hasPreceded;
    j["precededByLit"] = v.precededByLit;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["sb"] = v.sb;
    j["hasAlign"] = v.hasAlign;
    j["alignSpec"] = v.alignSpec;
    j["cells"] = *v.cells;
    j["numCols"] = v.numCols;
    j["alignSpecChars"] = v.alignSpecChars;
    j["singleAlignChar"] = v.singleAlignChar;
}
inline void from_json(const nlohmann::json& j, CaseData& v)
{
    j.at("tag").get_to(v.tag);
    j.at("tagLit").get_to(v.tagLit);
    j.at("bindingName").get_to(v.bindingName);
    j.at("hasBinding").get_to(v.hasBinding);
    if (j.contains("payloadType") && !j.at("payloadType").is_null()) v.payloadType = std::make_unique<TypeKind>(j.at("payloadType").get<TypeKind>());
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
    j.at("scopeId").get_to(v.scopeId);
    j.at("isFirst").get_to(v.isFirst);
    j.at("variantIndex").get_to(v.variantIndex);
    j.at("isRecursivePayload").get_to(v.isRecursivePayload);
}
inline void to_json(nlohmann::json& j, const CaseData& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    j["tagLit"] = v.tagLit;
    j["bindingName"] = v.bindingName;
    j["hasBinding"] = v.hasBinding;
    if (v.payloadType) j["payloadType"] = *v.payloadType;
    j["body"] = *v.body;
    j["scopeId"] = v.scopeId;
    j["isFirst"] = v.isFirst;
    j["variantIndex"] = v.variantIndex;
    j["isRecursivePayload"] = v.isRecursivePayload;
}
inline void from_json(const nlohmann::json& j, IfData& v)
{
    j.at("condPath").get_to(v.condPath);
    j.at("condIsBool").get_to(v.condIsBool);
    j.at("isNegated").get_to(v.isNegated);
    v.thenBody = std::make_unique<std::vector<Instruction>>(j.at("thenBody").get<std::vector<Instruction>>());
    v.elseBody = std::make_unique<std::vector<Instruction>>(j.at("elseBody").get<std::vector<Instruction>>());
    j.at("hasElse").get_to(v.hasElse);
    j.at("isBlock").get_to(v.isBlock);
    j.at("insertCol").get_to(v.insertCol);
    j.at("sb").get_to(v.sb);
    j.at("thenScopeId").get_to(v.thenScopeId);
    j.at("elseScopeId").get_to(v.elseScopeId);
}
inline void to_json(nlohmann::json& j, const IfData& v)
{
    j = nlohmann::json{};
    j["condPath"] = v.condPath;
    j["condIsBool"] = v.condIsBool;
    j["isNegated"] = v.isNegated;
    j["thenBody"] = *v.thenBody;
    j["elseBody"] = *v.elseBody;
    j["hasElse"] = v.hasElse;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["sb"] = v.sb;
    j["thenScopeId"] = v.thenScopeId;
    j["elseScopeId"] = v.elseScopeId;
}
inline void from_json(const nlohmann::json& j, SwitchData& v)
{
    j.at("exprPath").get_to(v.exprPath);
    j.at("enumTypeName").get_to(v.enumTypeName);
    v.cases = std::make_unique<std::vector<CaseData>>(j.at("cases").get<std::vector<CaseData>>());
    if (j.contains("defaultCase") && !j.at("defaultCase").is_null()) v.defaultCase = std::make_unique<CaseData>(j.at("defaultCase").get<CaseData>());
    j.at("isBlock").get_to(v.isBlock);
    j.at("insertCol").get_to(v.insertCol);
    j.at("sb").get_to(v.sb);
}
inline void to_json(nlohmann::json& j, const SwitchData& v)
{
    j = nlohmann::json{};
    j["exprPath"] = v.exprPath;
    j["enumTypeName"] = v.enumTypeName;
    j["cases"] = *v.cases;
    if (v.defaultCase) j["defaultCase"] = *v.defaultCase;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["sb"] = v.sb;
}
inline void from_json(const nlohmann::json& j, CallArgInfo& v)
{
    j.at("path").get_to(v.path);
    j.at("isRecursive").get_to(v.isRecursive);
    j.at("isOptional").get_to(v.isOptional);
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
    j.at("functionName").get_to(v.functionName);
    j.at("args").get_to(v.args);
    if (j.contains("policyArg") && !j.at("policyArg").is_null()) v.policyArg = j.at("policyArg").get<PolicyRef>();
    j.at("sb").get_to(v.sb);
    j.at("needsTry").get_to(v.needsTry);
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
inline void from_json(const nlohmann::json& j, RenderViaOverload& v)
{
    j.at("tag").get_to(v.tag);
    j.at("tagLit").get_to(v.tagLit);
    j.at("isFirst").get_to(v.isFirst);
    if (j.contains("payloadType") && !j.at("payloadType").is_null()) v.payloadType = std::make_unique<TypeKind>(j.at("payloadType").get<TypeKind>());
}
inline void to_json(nlohmann::json& j, const RenderViaOverload& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    j["tagLit"] = v.tagLit;
    j["isFirst"] = v.isFirst;
    if (v.payloadType) j["payloadType"] = *v.payloadType;
}
inline void from_json(const nlohmann::json& j, RenderViaData& v)
{
    j.at("scopeId").get_to(v.scopeId);
    j.at("collPath").get_to(v.collPath);
    j.at("isCollection").get_to(v.isCollection);
    v.elemType = std::make_unique<TypeKind>(j.at("elemType").get<TypeKind>());
    j.at("enumTypeName").get_to(v.enumTypeName);
    j.at("functionName").get_to(v.functionName);
    j.at("hasSep").get_to(v.hasSep);
    j.at("sepLit").get_to(v.sepLit);
    j.at("hasFollowed").get_to(v.hasFollowed);
    j.at("followedByLit").get_to(v.followedByLit);
    j.at("hasPreceded").get_to(v.hasPreceded);
    j.at("precededByLit").get_to(v.precededByLit);
    j.at("sb").get_to(v.sb);
    j.at("isSingleOverload").get_to(v.isSingleOverload);
    j.at("overloads").get_to(v.overloads);
    if (j.contains("policyArg") && !j.at("policyArg").is_null()) v.policyArg = j.at("policyArg").get<PolicyRef>();
    j.at("needsTry").get_to(v.needsTry);
}
inline void to_json(nlohmann::json& j, const RenderViaData& v)
{
    j = nlohmann::json{};
    j["scopeId"] = v.scopeId;
    j["collPath"] = v.collPath;
    j["isCollection"] = v.isCollection;
    j["elemType"] = *v.elemType;
    j["enumTypeName"] = v.enumTypeName;
    j["functionName"] = v.functionName;
    j["hasSep"] = v.hasSep;
    j["sepLit"] = v.sepLit;
    j["hasFollowed"] = v.hasFollowed;
    j["followedByLit"] = v.followedByLit;
    j["hasPreceded"] = v.hasPreceded;
    j["precededByLit"] = v.precededByLit;
    j["sb"] = v.sb;
    j["isSingleOverload"] = v.isSingleOverload;
    j["overloads"] = v.overloads;
    if (v.policyArg.has_value()) j["policyArg"] = *v.policyArg;
    j["needsTry"] = v.needsTry;
}
inline void from_json(const nlohmann::json& j, PolicyReplacementInfo& v)
{
    j.at("findLit").get_to(v.findLit);
    j.at("replaceLit").get_to(v.replaceLit);
}
inline void to_json(nlohmann::json& j, const PolicyReplacementInfo& v)
{
    j = nlohmann::json{};
    j["findLit"] = v.findLit;
    j["replaceLit"] = v.replaceLit;
}
inline void from_json(const nlohmann::json& j, PolicyRequireInfo& v)
{
    j.at("regexLit").get_to(v.regexLit);
    j.at("replaceLit").get_to(v.replaceLit);
    j.at("hasReplace").get_to(v.hasReplace);
}
inline void to_json(nlohmann::json& j, const PolicyRequireInfo& v)
{
    j = nlohmann::json{};
    j["regexLit"] = v.regexLit;
    j["replaceLit"] = v.replaceLit;
    j["hasReplace"] = v.hasReplace;
}
inline void from_json(const nlohmann::json& j, PolicyOutputFilterInfo& v)
{
    j.at("regexLit").get_to(v.regexLit);
}
inline void to_json(nlohmann::json& j, const PolicyOutputFilterInfo& v)
{
    j = nlohmann::json{};
    j["regexLit"] = v.regexLit;
}
inline void from_json(const nlohmann::json& j, PolicyInfo& v)
{
    j.at("tagLit").get_to(v.tagLit);
    j.at("identifier").get_to(v.identifier);
    j.at("hasLength").get_to(v.hasLength);
    j.at("minVal").get_to(v.minVal);
    j.at("maxVal").get_to(v.maxVal);
    j.at("hasRejectIf").get_to(v.hasRejectIf);
    j.at("rejectIfRegexLit").get_to(v.rejectIfRegexLit);
    j.at("rejectMsgLit").get_to(v.rejectMsgLit);
    j.at("require").get_to(v.require);
    j.at("hasRequire").get_to(v.hasRequire);
    j.at("replacements").get_to(v.replacements);
    j.at("hasReplacements").get_to(v.hasReplacements);
    j.at("outputFilter").get_to(v.outputFilter);
    j.at("hasOutputFilter").get_to(v.hasOutputFilter);
}
inline void to_json(nlohmann::json& j, const PolicyInfo& v)
{
    j = nlohmann::json{};
    j["tagLit"] = v.tagLit;
    j["identifier"] = v.identifier;
    j["hasLength"] = v.hasLength;
    j["minVal"] = v.minVal;
    j["maxVal"] = v.maxVal;
    j["hasRejectIf"] = v.hasRejectIf;
    j["rejectIfRegexLit"] = v.rejectIfRegexLit;
    j["rejectMsgLit"] = v.rejectMsgLit;
    j["require"] = v.require;
    j["hasRequire"] = v.hasRequire;
    j["replacements"] = v.replacements;
    j["hasReplacements"] = v.hasReplacements;
    j["outputFilter"] = v.outputFilter;
    j["hasOutputFilter"] = v.hasOutputFilter;
}
inline void from_json(const nlohmann::json& j, FieldInfo& v)
{
    j.at("name").get_to(v.name);
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
    j.at("recursive").get_to(v.recursive);
    j.at("isOptional").get_to(v.isOptional);
    j.at("recursiveOptional").get_to(v.recursiveOptional);
    v.innerType = std::make_unique<TypeKind>(j.at("innerType").get<TypeKind>());
    if (j.contains("doc") && !j.at("doc").is_null()) v.doc = j.at("doc").get<std::string>();
}
inline void to_json(nlohmann::json& j, const FieldInfo& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["type"] = *v.type;
    j["recursive"] = v.recursive;
    j["isOptional"] = v.isOptional;
    j["recursiveOptional"] = v.recursiveOptional;
    j["innerType"] = *v.innerType;
    if (v.doc.has_value()) j["doc"] = *v.doc;
}
inline void from_json(const nlohmann::json& j, StructInfo& v)
{
    j.at("name").get_to(v.name);
    j.at("fields").get_to(v.fields);
    if (j.contains("doc") && !j.at("doc").is_null()) v.doc = j.at("doc").get<std::string>();
    j.at("rawTypedefs").get_to(v.rawTypedefs);
}
inline void to_json(nlohmann::json& j, const StructInfo& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["fields"] = v.fields;
    if (v.doc.has_value()) j["doc"] = *v.doc;
    j["rawTypedefs"] = v.rawTypedefs;
}
inline void from_json(const nlohmann::json& j, VariantInfo& v)
{
    j.at("tag").get_to(v.tag);
    if (j.contains("payload") && !j.at("payload").is_null()) v.payload = std::make_unique<TypeKind>(j.at("payload").get<TypeKind>());
    j.at("recursive").get_to(v.recursive);
    j.at("index").get_to(v.index);
    if (j.contains("doc") && !j.at("doc").is_null()) v.doc = j.at("doc").get<std::string>();
}
inline void to_json(nlohmann::json& j, const VariantInfo& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    if (v.payload) j["payload"] = *v.payload;
    j["recursive"] = v.recursive;
    j["index"] = v.index;
    if (v.doc.has_value()) j["doc"] = *v.doc;
}
inline void from_json(const nlohmann::json& j, EnumInfo& v)
{
    j.at("name").get_to(v.name);
    j.at("variants").get_to(v.variants);
    j.at("hasRecursiveVariants").get_to(v.hasRecursiveVariants);
    if (j.contains("doc") && !j.at("doc").is_null()) v.doc = j.at("doc").get<std::string>();
    j.at("rawTypedefs").get_to(v.rawTypedefs);
}
inline void to_json(nlohmann::json& j, const EnumInfo& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["variants"] = v.variants;
    j["hasRecursiveVariants"] = v.hasRecursiveVariants;
    if (v.doc.has_value()) j["doc"] = *v.doc;
    j["rawTypedefs"] = v.rawTypedefs;
}
inline void from_json(const nlohmann::json& j, ParamInfo& v)
{
    j.at("name").get_to(v.name);
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
}
inline void to_json(nlohmann::json& j, const ParamInfo& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["type"] = *v.type;
}
inline void from_json(const nlohmann::json& j, FunctionInfo& v)
{
    j.at("name").get_to(v.name);
    j.at("params").get_to(v.params);
    if (j.contains("doc") && !j.at("doc").is_null()) v.doc = j.at("doc").get<std::string>();
}
inline void to_json(nlohmann::json& j, const FunctionInfo& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["params"] = v.params;
    if (v.doc.has_value()) j["doc"] = *v.doc;
}
inline void from_json(const nlohmann::json& j, CodegenInput& v)
{
    j.at("structs").get_to(v.structs);
    j.at("enums").get_to(v.enums);
    j.at("functions").get_to(v.functions);
    j.at("hasRecursiveTypes").get_to(v.hasRecursiveTypes);
}
inline void to_json(nlohmann::json& j, const CodegenInput& v)
{
    j = nlohmann::json{};
    j["structs"] = v.structs;
    j["enums"] = v.enums;
    j["functions"] = v.functions;
    j["hasRecursiveTypes"] = v.hasRecursiveTypes;
}
inline void from_json(const nlohmann::json& j, RenderFunctionDef& v)
{
    j.at("name").get_to(v.name);
    j.at("params").get_to(v.params);
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
}
inline void to_json(nlohmann::json& j, const RenderFunctionDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["params"] = v.params;
    j["body"] = *v.body;
}
inline void from_json(const nlohmann::json& j, RenderFunctionsInput& v)
{
    j.at("functions").get_to(v.functions);
    j.at("hasPolicies").get_to(v.hasPolicies);
    j.at("policies").get_to(v.policies);
    j.at("functionPrefix").get_to(v.functionPrefix);
    if (j.contains("namespaceName") && !j.at("namespaceName").is_null()) v.namespaceName = j.at("namespaceName").get<std::string>();
    j.at("staticModifier").get_to(v.staticModifier);
    if (j.contains("includes") && !j.at("includes").is_null()) v.includes = j.at("includes").get<std::vector<std::string>>();
    if (j.contains("externalRuntime") && !j.at("externalRuntime").is_null()) v.externalRuntime = j.at("externalRuntime").get<bool>();
}
inline void to_json(nlohmann::json& j, const RenderFunctionsInput& v)
{
    j = nlohmann::json{};
    j["functions"] = v.functions;
    j["hasPolicies"] = v.hasPolicies;
    j["policies"] = v.policies;
    j["functionPrefix"] = v.functionPrefix;
    if (v.namespaceName.has_value()) j["namespaceName"] = *v.namespaceName;
    j["staticModifier"] = v.staticModifier;
    if (v.includes.has_value()) j["includes"] = *v.includes;
    if (v.externalRuntime.has_value()) j["externalRuntime"] = *v.externalRuntime;
}
} // namespace codegen
