#pragma once
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>
namespace tpp {
template<typename _TppT>
inline nlohmann::json _tpp_j(const _TppT& v) { return nlohmann::json(v); }
template<typename _TppT>
inline nlohmann::json _tpp_j(const std::unique_ptr<_TppT>& v) { return v ? nlohmann::json(*v) : nlohmann::json(); }

struct TypeKind;
struct Instruction;
struct SourcePosition;
struct SourceRange;
struct ExprInfo;
struct EmitInstr;
struct EmitExprInstr;
struct ForInstr;
struct CaseInstr;
struct IfInstr;
struct SwitchInstr;
struct CallInstr;
struct RenderViaInstr;
struct ParamDef;
struct FunctionDef;
struct FieldDef;
struct StructDef;
struct VariantDef;
struct EnumDef;
struct PolicyReplacement;
struct PolicyRequire;
struct PolicyOutputFilter;
struct PolicyLength;
struct PolicyRejectIf;
struct PolicyDef;
struct IR;

struct SourcePosition
{
    int line;
    int character;
    static std::string tpp_typedefs() noexcept
    {
        return "// ═══════════════════════════════════════════════════════════════════════════════\n// tpp Intermediate Representation — backend-neutral type definitions.\n//\n// These types encode the full output of the tpp compiler: schema info\n// (structs, enums), template functions as typed instruction trees, and\n// policy definitions. Backends can consume this IR directly, including\n// generators for non-C++ languages.\n// ═══════════════════════════════════════════════════════════════════════════════\n\n// ── Source location (optional — populated when --source-ranges is set) ───────\n\nstruct SourcePosition\n{\n    line : int;\n    character : int;\n}";
    }
};
struct SourceRange
{
    SourcePosition start;
    SourcePosition end;
    static std::string tpp_typedefs() noexcept
    {
        return "struct SourceRange\n{\n    start : SourcePosition;\n    end : SourcePosition;\n}";
    }
};
/// Expression with resolved type.
struct ExprInfo
{
    std::string path;
    std::unique_ptr<TypeKind> type;
    static std::string tpp_typedefs() noexcept
    {
        return "// ── Instruction types (backend-neutral) ─────────────────────────────────────\n\n/// Expression with resolved type.\nstruct ExprInfo\n{\n    path : string;\n    type : TypeKind;\n}";
    }
};
/// Emit literal text.
struct EmitInstr
{
    std::string text;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Emit literal text.\nstruct EmitInstr\n{\n    text : string;\n}";
    }
};
/// Emit an interpolated expression value.
struct EmitExprInstr
{
    ExprInfo expr;
    std::string policy;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Emit an interpolated expression value.\nstruct EmitExprInstr\n{\n    expr : ExprInfo;\n    policy : string;\n}";
    }
};
/// For loop over a collection.
struct ForInstr
{
    std::string varName;
    std::string enumeratorName;
    ExprInfo collection;
    std::unique_ptr<std::vector<Instruction>> body;
    std::string sep;
    std::string followedBy;
    std::string precededBy;
    bool isBlock;
    int insertCol;
    std::string policy;
    bool hasAlign;
    std::string alignSpec;
    static std::string tpp_typedefs() noexcept
    {
        return "/// For loop over a collection.\nstruct ForInstr\n{\n    varName : string;\n    enumeratorName : string;\n    collection : ExprInfo;\n    body : list<Instruction>;\n    sep : string;\n    followedBy : string;\n    precededBy : string;\n    isBlock : bool;\n    insertCol : int;\n    policy : string;\n    hasAlign : bool;\n    alignSpec : string;\n}";
    }
};
/// One case branch in a switch.
struct CaseInstr
{
    std::string tag;
    std::string bindingName;
    std::unique_ptr<TypeKind> payloadType;
    std::unique_ptr<std::vector<Instruction>> body;
    static std::string tpp_typedefs() noexcept
    {
        return "/// One case branch in a switch.\nstruct CaseInstr\n{\n    tag : string;\n    bindingName : string;\n    payloadType : optional<TypeKind>;\n    body : list<Instruction>;\n}";
    }
};
/// Conditional.
struct IfInstr
{
    ExprInfo condExpr;
    bool negated;
    std::unique_ptr<std::vector<Instruction>> thenBody;
    std::unique_ptr<std::vector<Instruction>> elseBody;
    bool isBlock;
    int insertCol;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Conditional.\nstruct IfInstr\n{\n    condExpr : ExprInfo;\n    negated : bool;\n    thenBody : list<Instruction>;\n    elseBody : list<Instruction>;\n    isBlock : bool;\n    insertCol : int;\n}";
    }
};
/// Switch on a variant expression.
struct SwitchInstr
{
    ExprInfo expr;
    std::unique_ptr<std::vector<CaseInstr>> cases;
    std::unique_ptr<CaseInstr> defaultCase;
    bool isBlock;
    int insertCol;
    std::string policy;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Switch on a variant expression.\nstruct SwitchInstr\n{\n    expr : ExprInfo;\n    cases : list<CaseInstr>;\n    defaultCase : optional<CaseInstr>;\n    isBlock : bool;\n    insertCol : int;\n    policy : string;\n}";
    }
};
/// Direct function call.
struct CallInstr
{
    std::string functionName;
    std::vector<ExprInfo> arguments;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Direct function call.\nstruct CallInstr\n{\n    functionName : string;\n    arguments : list<ExprInfo>;\n}";
    }
};
/// Render-via: call a function for each element in a collection or single enum.
struct RenderViaInstr
{
    ExprInfo collection;
    std::string functionName;
    std::string sep;
    std::string followedBy;
    std::string precededBy;
    bool isBlock;
    int insertCol;
    std::string policy;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Render-via: call a function for each element in a collection or single enum.\nstruct RenderViaInstr\n{\n    collection : ExprInfo;\n    functionName : string;\n    sep : string;\n    followedBy : string;\n    precededBy : string;\n    isBlock : bool;\n    insertCol : int;\n    policy : string;\n}";
    }
};
struct ParamDef
{
    std::string name;
    std::unique_ptr<TypeKind> type;
    static std::string tpp_typedefs() noexcept
    {
        return "// ── Template function definition ────────────────────────────────────────────\n\nstruct ParamDef\n{\n    name : string;\n    type : TypeKind;\n}";
    }
};
struct FunctionDef
{
    std::string name;
    std::vector<ParamDef> params;
    std::unique_ptr<std::vector<Instruction>> body;
    std::string policy;
    std::string doc;
    std::optional<SourceRange> sourceRange;
    static std::string tpp_typedefs() noexcept
    {
        return "struct FunctionDef\n{\n    name : string;\n    params : list<ParamDef>;\n    body : list<Instruction>;\n    policy : string;\n    doc : string;\n    sourceRange : optional<SourceRange>;\n}";
    }
};
struct FieldDef
{
    std::string name;
    std::unique_ptr<TypeKind> type;
    bool recursive;
    std::string doc;
    std::optional<SourceRange> sourceRange;
    static std::string tpp_typedefs() noexcept
    {
        return "// ── Schema types (for type generation) ──────────────────────────────────────\n\nstruct FieldDef\n{\n    name : string;\n    type : TypeKind;\n    recursive : bool;\n    doc : string;\n    sourceRange : optional<SourceRange>;\n}";
    }
};
struct StructDef
{
    std::string name;
    std::vector<FieldDef> fields;
    std::string doc;
    std::optional<SourceRange> sourceRange;
    std::string rawTypedefs;
    static std::string tpp_typedefs() noexcept
    {
        return "struct StructDef\n{\n    name : string;\n    fields : list<FieldDef>;\n    doc : string;\n    sourceRange : optional<SourceRange>;\n    rawTypedefs : string;\n}";
    }
};
struct VariantDef
{
    std::string tag;
    std::unique_ptr<TypeKind> payload;
    bool recursive;
    std::string doc;
    std::optional<SourceRange> sourceRange;
    static std::string tpp_typedefs() noexcept
    {
        return "struct VariantDef\n{\n    tag : string;\n    payload : optional<TypeKind>;\n    recursive : bool;\n    doc : string;\n    sourceRange : optional<SourceRange>;\n}";
    }
};
struct EnumDef
{
    std::string name;
    std::vector<VariantDef> variants;
    std::string doc;
    std::optional<SourceRange> sourceRange;
    std::string rawTypedefs;
    static std::string tpp_typedefs() noexcept
    {
        return "struct EnumDef\n{\n    name : string;\n    variants : list<VariantDef>;\n    doc : string;\n    sourceRange : optional<SourceRange>;\n    rawTypedefs : string;\n}";
    }
};
struct PolicyReplacement
{
    std::string find;
    std::string replace;
    static std::string tpp_typedefs() noexcept
    {
        return "// ── Policy definitions ──────────────────────────────────────────────────────\n\nstruct PolicyReplacement\n{\n    find : string;\n    replace : string;\n}";
    }
};
struct PolicyRequire
{
    std::string regex;
    std::string replace;
    std::string compiledReplace;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyRequire\n{\n    regex : string;\n    replace : string;\n    compiledReplace : string;\n}";
    }
};
struct PolicyOutputFilter
{
    std::string regex;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyOutputFilter\n{\n    regex : string;\n}";
    }
};
struct PolicyLength
{
    std::optional<int> min;
    std::optional<int> max;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyLength\n{\n    min : optional<int>;\n    max : optional<int>;\n}";
    }
};
struct PolicyRejectIf
{
    std::string regex;
    std::string message;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyRejectIf\n{\n    regex : string;\n    message : string;\n}";
    }
};
struct PolicyDef
{
    std::string tag;
    std::optional<PolicyLength> length;
    std::optional<PolicyRejectIf> rejectIf;
    std::vector<PolicyRequire> require;
    std::vector<PolicyReplacement> replacements;
    std::vector<PolicyOutputFilter> outputFilter;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyDef\n{\n    tag : string;\n    length : optional<PolicyLength>;\n    rejectIf : optional<PolicyRejectIf>;\n    require : list<PolicyRequire>;\n    replacements : list<PolicyReplacement>;\n    outputFilter : list<PolicyOutputFilter>;\n}";
    }
};
struct IR
{
    int versionMajor;
    int versionMinor;
    int versionPatch;
    std::vector<StructDef> structs;
    std::vector<EnumDef> enums;
    std::vector<FunctionDef> functions;
    std::vector<PolicyDef> policies;
    static std::string tpp_typedefs() noexcept
    {
        return "// ── Top-level IR ─────────────────────────────────────────────────────────────\n\nstruct IR\n{\n    versionMajor : int;\n    versionMinor : int;\n    versionPatch : int;\n    structs : list<StructDef>;\n    enums : list<EnumDef>;\n    functions : list<FunctionDef>;\n    policies : list<PolicyDef>;\n}";
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
        return "// ── Type system ───────────────────────────────────────────────────────────────\n\nenum TypeKind\n{\n    Str,\n    Int,\n    Bool,\n    Named(string),\n    List(TypeKind),\n    Optional(TypeKind)\n}";
    }
};
struct Instruction_AlignCell
{
    friend void to_json(nlohmann::json& j, const Instruction_AlignCell&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, Instruction_AlignCell&) {}
};
struct Instruction
{
    using Value = std::variant<EmitInstr, EmitExprInstr, Instruction_AlignCell, std::unique_ptr<ForInstr>, std::unique_ptr<IfInstr>, std::unique_ptr<SwitchInstr>, CallInstr, RenderViaInstr>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "enum Instruction\n{\n    Emit(EmitInstr),\n    EmitExpr(EmitExprInstr),\n    AlignCell,\n    For(ForInstr),\n    If(IfInstr),\n    Switch(SwitchInstr),\n    Call(CallInstr),\n    RenderVia(RenderViaInstr)\n}";
    }
};

inline void from_json(const nlohmann::json& j, TypeKind& v);
inline void to_json(nlohmann::json& j, const TypeKind& v);
inline void from_json(const nlohmann::json& j, Instruction& v);
inline void to_json(nlohmann::json& j, const Instruction& v);
inline void from_json(const nlohmann::json& j, SourcePosition& v);
inline void to_json(nlohmann::json& j, const SourcePosition& v);
inline void from_json(const nlohmann::json& j, SourceRange& v);
inline void to_json(nlohmann::json& j, const SourceRange& v);
inline void from_json(const nlohmann::json& j, ExprInfo& v);
inline void to_json(nlohmann::json& j, const ExprInfo& v);
inline void from_json(const nlohmann::json& j, EmitInstr& v);
inline void to_json(nlohmann::json& j, const EmitInstr& v);
inline void from_json(const nlohmann::json& j, EmitExprInstr& v);
inline void to_json(nlohmann::json& j, const EmitExprInstr& v);
inline void from_json(const nlohmann::json& j, ForInstr& v);
inline void to_json(nlohmann::json& j, const ForInstr& v);
inline void from_json(const nlohmann::json& j, CaseInstr& v);
inline void to_json(nlohmann::json& j, const CaseInstr& v);
inline void from_json(const nlohmann::json& j, IfInstr& v);
inline void to_json(nlohmann::json& j, const IfInstr& v);
inline void from_json(const nlohmann::json& j, SwitchInstr& v);
inline void to_json(nlohmann::json& j, const SwitchInstr& v);
inline void from_json(const nlohmann::json& j, CallInstr& v);
inline void to_json(nlohmann::json& j, const CallInstr& v);
inline void from_json(const nlohmann::json& j, RenderViaInstr& v);
inline void to_json(nlohmann::json& j, const RenderViaInstr& v);
inline void from_json(const nlohmann::json& j, ParamDef& v);
inline void to_json(nlohmann::json& j, const ParamDef& v);
inline void from_json(const nlohmann::json& j, FunctionDef& v);
inline void to_json(nlohmann::json& j, const FunctionDef& v);
inline void from_json(const nlohmann::json& j, FieldDef& v);
inline void to_json(nlohmann::json& j, const FieldDef& v);
inline void from_json(const nlohmann::json& j, StructDef& v);
inline void to_json(nlohmann::json& j, const StructDef& v);
inline void from_json(const nlohmann::json& j, VariantDef& v);
inline void to_json(nlohmann::json& j, const VariantDef& v);
inline void from_json(const nlohmann::json& j, EnumDef& v);
inline void to_json(nlohmann::json& j, const EnumDef& v);
inline void from_json(const nlohmann::json& j, PolicyReplacement& v);
inline void to_json(nlohmann::json& j, const PolicyReplacement& v);
inline void from_json(const nlohmann::json& j, PolicyRequire& v);
inline void to_json(nlohmann::json& j, const PolicyRequire& v);
inline void from_json(const nlohmann::json& j, PolicyOutputFilter& v);
inline void to_json(nlohmann::json& j, const PolicyOutputFilter& v);
inline void from_json(const nlohmann::json& j, PolicyLength& v);
inline void to_json(nlohmann::json& j, const PolicyLength& v);
inline void from_json(const nlohmann::json& j, PolicyRejectIf& v);
inline void to_json(nlohmann::json& j, const PolicyRejectIf& v);
inline void from_json(const nlohmann::json& j, PolicyDef& v);
inline void to_json(nlohmann::json& j, const PolicyDef& v);
inline void from_json(const nlohmann::json& j, IR& v);
inline void to_json(nlohmann::json& j, const IR& v);

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
inline void from_json(const nlohmann::json& j, Instruction& v)
{
    if (j.contains("Emit")) v.value.emplace<0>(j["Emit"].get<EmitInstr>());
    else     if (j.contains("EmitExpr")) v.value.emplace<1>(j["EmitExpr"].get<EmitExprInstr>());
    else     if (j.contains("AlignCell")) v.value.emplace<2>();
    else     if (j.contains("For")) v.value.emplace<3>(std::make_unique<ForInstr>(j["For"].get<ForInstr>()));
    else     if (j.contains("If")) v.value.emplace<4>(std::make_unique<IfInstr>(j["If"].get<IfInstr>()));
    else     if (j.contains("Switch")) v.value.emplace<5>(std::make_unique<SwitchInstr>(j["Switch"].get<SwitchInstr>()));
    else     if (j.contains("Call")) v.value.emplace<6>(j["Call"].get<CallInstr>());
    else     if (j.contains("RenderVia")) v.value.emplace<7>(j["RenderVia"].get<RenderViaInstr>());
}
inline void to_json(nlohmann::json& j, const Instruction& v)
{
    const char* _tags[] = {"Emit", "EmitExpr", "AlignCell", "For", "If", "Switch", "Call", "RenderVia"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, SourcePosition& v)
{
    j.at("line").get_to(v.line);
    j.at("character").get_to(v.character);
}
inline void to_json(nlohmann::json& j, const SourcePosition& v)
{
    j = nlohmann::json{};
    j["line"] = v.line;
    j["character"] = v.character;
}
inline void from_json(const nlohmann::json& j, SourceRange& v)
{
    j.at("start").get_to(v.start);
    j.at("end").get_to(v.end);
}
inline void to_json(nlohmann::json& j, const SourceRange& v)
{
    j = nlohmann::json{};
    j["start"] = v.start;
    j["end"] = v.end;
}
inline void from_json(const nlohmann::json& j, ExprInfo& v)
{
    j.at("path").get_to(v.path);
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
}
inline void to_json(nlohmann::json& j, const ExprInfo& v)
{
    j = nlohmann::json{};
    j["path"] = v.path;
    j["type"] = *v.type;
}
inline void from_json(const nlohmann::json& j, EmitInstr& v)
{
    j.at("text").get_to(v.text);
}
inline void to_json(nlohmann::json& j, const EmitInstr& v)
{
    j = nlohmann::json{};
    j["text"] = v.text;
}
inline void from_json(const nlohmann::json& j, EmitExprInstr& v)
{
    j.at("expr").get_to(v.expr);
    j.at("policy").get_to(v.policy);
}
inline void to_json(nlohmann::json& j, const EmitExprInstr& v)
{
    j = nlohmann::json{};
    j["expr"] = v.expr;
    j["policy"] = v.policy;
}
inline void from_json(const nlohmann::json& j, ForInstr& v)
{
    j.at("varName").get_to(v.varName);
    j.at("enumeratorName").get_to(v.enumeratorName);
    j.at("collection").get_to(v.collection);
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
    j.at("sep").get_to(v.sep);
    j.at("followedBy").get_to(v.followedBy);
    j.at("precededBy").get_to(v.precededBy);
    j.at("isBlock").get_to(v.isBlock);
    j.at("insertCol").get_to(v.insertCol);
    j.at("policy").get_to(v.policy);
    j.at("hasAlign").get_to(v.hasAlign);
    j.at("alignSpec").get_to(v.alignSpec);
}
inline void to_json(nlohmann::json& j, const ForInstr& v)
{
    j = nlohmann::json{};
    j["varName"] = v.varName;
    j["enumeratorName"] = v.enumeratorName;
    j["collection"] = v.collection;
    j["body"] = *v.body;
    j["sep"] = v.sep;
    j["followedBy"] = v.followedBy;
    j["precededBy"] = v.precededBy;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["policy"] = v.policy;
    j["hasAlign"] = v.hasAlign;
    j["alignSpec"] = v.alignSpec;
}
inline void from_json(const nlohmann::json& j, CaseInstr& v)
{
    j.at("tag").get_to(v.tag);
    j.at("bindingName").get_to(v.bindingName);
    if (j.contains("payloadType") && !j.at("payloadType").is_null()) v.payloadType = std::make_unique<TypeKind>(j.at("payloadType").get<TypeKind>());
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
}
inline void to_json(nlohmann::json& j, const CaseInstr& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    j["bindingName"] = v.bindingName;
    if (v.payloadType) j["payloadType"] = *v.payloadType;
    j["body"] = *v.body;
}
inline void from_json(const nlohmann::json& j, IfInstr& v)
{
    j.at("condExpr").get_to(v.condExpr);
    j.at("negated").get_to(v.negated);
    v.thenBody = std::make_unique<std::vector<Instruction>>(j.at("thenBody").get<std::vector<Instruction>>());
    v.elseBody = std::make_unique<std::vector<Instruction>>(j.at("elseBody").get<std::vector<Instruction>>());
    j.at("isBlock").get_to(v.isBlock);
    j.at("insertCol").get_to(v.insertCol);
}
inline void to_json(nlohmann::json& j, const IfInstr& v)
{
    j = nlohmann::json{};
    j["condExpr"] = v.condExpr;
    j["negated"] = v.negated;
    j["thenBody"] = *v.thenBody;
    j["elseBody"] = *v.elseBody;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
}
inline void from_json(const nlohmann::json& j, SwitchInstr& v)
{
    j.at("expr").get_to(v.expr);
    v.cases = std::make_unique<std::vector<CaseInstr>>(j.at("cases").get<std::vector<CaseInstr>>());
    if (j.contains("defaultCase") && !j.at("defaultCase").is_null()) v.defaultCase = std::make_unique<CaseInstr>(j.at("defaultCase").get<CaseInstr>());
    j.at("isBlock").get_to(v.isBlock);
    j.at("insertCol").get_to(v.insertCol);
    j.at("policy").get_to(v.policy);
}
inline void to_json(nlohmann::json& j, const SwitchInstr& v)
{
    j = nlohmann::json{};
    j["expr"] = v.expr;
    j["cases"] = *v.cases;
    if (v.defaultCase) j["defaultCase"] = *v.defaultCase;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["policy"] = v.policy;
}
inline void from_json(const nlohmann::json& j, CallInstr& v)
{
    j.at("functionName").get_to(v.functionName);
    j.at("arguments").get_to(v.arguments);
}
inline void to_json(nlohmann::json& j, const CallInstr& v)
{
    j = nlohmann::json{};
    j["functionName"] = v.functionName;
    j["arguments"] = v.arguments;
}
inline void from_json(const nlohmann::json& j, RenderViaInstr& v)
{
    j.at("collection").get_to(v.collection);
    j.at("functionName").get_to(v.functionName);
    j.at("sep").get_to(v.sep);
    j.at("followedBy").get_to(v.followedBy);
    j.at("precededBy").get_to(v.precededBy);
    j.at("isBlock").get_to(v.isBlock);
    j.at("insertCol").get_to(v.insertCol);
    j.at("policy").get_to(v.policy);
}
inline void to_json(nlohmann::json& j, const RenderViaInstr& v)
{
    j = nlohmann::json{};
    j["collection"] = v.collection;
    j["functionName"] = v.functionName;
    j["sep"] = v.sep;
    j["followedBy"] = v.followedBy;
    j["precededBy"] = v.precededBy;
    j["isBlock"] = v.isBlock;
    j["insertCol"] = v.insertCol;
    j["policy"] = v.policy;
}
inline void from_json(const nlohmann::json& j, ParamDef& v)
{
    j.at("name").get_to(v.name);
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
}
inline void to_json(nlohmann::json& j, const ParamDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["type"] = *v.type;
}
inline void from_json(const nlohmann::json& j, FunctionDef& v)
{
    j.at("name").get_to(v.name);
    j.at("params").get_to(v.params);
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
    j.at("policy").get_to(v.policy);
    j.at("doc").get_to(v.doc);
    if (j.contains("sourceRange") && !j.at("sourceRange").is_null()) v.sourceRange = j.at("sourceRange").get<SourceRange>();
}
inline void to_json(nlohmann::json& j, const FunctionDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["params"] = v.params;
    j["body"] = *v.body;
    j["policy"] = v.policy;
    j["doc"] = v.doc;
    if (v.sourceRange.has_value()) j["sourceRange"] = *v.sourceRange;
}
inline void from_json(const nlohmann::json& j, FieldDef& v)
{
    j.at("name").get_to(v.name);
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
    j.at("recursive").get_to(v.recursive);
    j.at("doc").get_to(v.doc);
    if (j.contains("sourceRange") && !j.at("sourceRange").is_null()) v.sourceRange = j.at("sourceRange").get<SourceRange>();
}
inline void to_json(nlohmann::json& j, const FieldDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["type"] = *v.type;
    j["recursive"] = v.recursive;
    j["doc"] = v.doc;
    if (v.sourceRange.has_value()) j["sourceRange"] = *v.sourceRange;
}
inline void from_json(const nlohmann::json& j, StructDef& v)
{
    j.at("name").get_to(v.name);
    j.at("fields").get_to(v.fields);
    j.at("doc").get_to(v.doc);
    if (j.contains("sourceRange") && !j.at("sourceRange").is_null()) v.sourceRange = j.at("sourceRange").get<SourceRange>();
    j.at("rawTypedefs").get_to(v.rawTypedefs);
}
inline void to_json(nlohmann::json& j, const StructDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["fields"] = v.fields;
    j["doc"] = v.doc;
    if (v.sourceRange.has_value()) j["sourceRange"] = *v.sourceRange;
    j["rawTypedefs"] = v.rawTypedefs;
}
inline void from_json(const nlohmann::json& j, VariantDef& v)
{
    j.at("tag").get_to(v.tag);
    if (j.contains("payload") && !j.at("payload").is_null()) v.payload = std::make_unique<TypeKind>(j.at("payload").get<TypeKind>());
    j.at("recursive").get_to(v.recursive);
    j.at("doc").get_to(v.doc);
    if (j.contains("sourceRange") && !j.at("sourceRange").is_null()) v.sourceRange = j.at("sourceRange").get<SourceRange>();
}
inline void to_json(nlohmann::json& j, const VariantDef& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    if (v.payload) j["payload"] = *v.payload;
    j["recursive"] = v.recursive;
    j["doc"] = v.doc;
    if (v.sourceRange.has_value()) j["sourceRange"] = *v.sourceRange;
}
inline void from_json(const nlohmann::json& j, EnumDef& v)
{
    j.at("name").get_to(v.name);
    j.at("variants").get_to(v.variants);
    j.at("doc").get_to(v.doc);
    if (j.contains("sourceRange") && !j.at("sourceRange").is_null()) v.sourceRange = j.at("sourceRange").get<SourceRange>();
    j.at("rawTypedefs").get_to(v.rawTypedefs);
}
inline void to_json(nlohmann::json& j, const EnumDef& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["variants"] = v.variants;
    j["doc"] = v.doc;
    if (v.sourceRange.has_value()) j["sourceRange"] = *v.sourceRange;
    j["rawTypedefs"] = v.rawTypedefs;
}
inline void from_json(const nlohmann::json& j, PolicyReplacement& v)
{
    j.at("find").get_to(v.find);
    j.at("replace").get_to(v.replace);
}
inline void to_json(nlohmann::json& j, const PolicyReplacement& v)
{
    j = nlohmann::json{};
    j["find"] = v.find;
    j["replace"] = v.replace;
}
inline void from_json(const nlohmann::json& j, PolicyRequire& v)
{
    j.at("regex").get_to(v.regex);
    j.at("replace").get_to(v.replace);
    j.at("compiledReplace").get_to(v.compiledReplace);
}
inline void to_json(nlohmann::json& j, const PolicyRequire& v)
{
    j = nlohmann::json{};
    j["regex"] = v.regex;
    j["replace"] = v.replace;
    j["compiledReplace"] = v.compiledReplace;
}
inline void from_json(const nlohmann::json& j, PolicyOutputFilter& v)
{
    j.at("regex").get_to(v.regex);
}
inline void to_json(nlohmann::json& j, const PolicyOutputFilter& v)
{
    j = nlohmann::json{};
    j["regex"] = v.regex;
}
inline void from_json(const nlohmann::json& j, PolicyLength& v)
{
    if (j.contains("min") && !j.at("min").is_null()) v.min = j.at("min").get<int>();
    if (j.contains("max") && !j.at("max").is_null()) v.max = j.at("max").get<int>();
}
inline void to_json(nlohmann::json& j, const PolicyLength& v)
{
    j = nlohmann::json{};
    if (v.min.has_value()) j["min"] = *v.min;
    if (v.max.has_value()) j["max"] = *v.max;
}
inline void from_json(const nlohmann::json& j, PolicyRejectIf& v)
{
    j.at("regex").get_to(v.regex);
    j.at("message").get_to(v.message);
}
inline void to_json(nlohmann::json& j, const PolicyRejectIf& v)
{
    j = nlohmann::json{};
    j["regex"] = v.regex;
    j["message"] = v.message;
}
inline void from_json(const nlohmann::json& j, PolicyDef& v)
{
    j.at("tag").get_to(v.tag);
    if (j.contains("length") && !j.at("length").is_null()) v.length = j.at("length").get<PolicyLength>();
    if (j.contains("rejectIf") && !j.at("rejectIf").is_null()) v.rejectIf = j.at("rejectIf").get<PolicyRejectIf>();
    j.at("require").get_to(v.require);
    j.at("replacements").get_to(v.replacements);
    j.at("outputFilter").get_to(v.outputFilter);
}
inline void to_json(nlohmann::json& j, const PolicyDef& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    if (v.length.has_value()) j["length"] = *v.length;
    if (v.rejectIf.has_value()) j["rejectIf"] = *v.rejectIf;
    j["require"] = v.require;
    j["replacements"] = v.replacements;
    j["outputFilter"] = v.outputFilter;
}
inline void from_json(const nlohmann::json& j, IR& v)
{
    j.at("versionMajor").get_to(v.versionMajor);
    j.at("versionMinor").get_to(v.versionMinor);
    j.at("versionPatch").get_to(v.versionPatch);
    j.at("structs").get_to(v.structs);
    j.at("enums").get_to(v.enums);
    j.at("functions").get_to(v.functions);
    j.at("policies").get_to(v.policies);
}
inline void to_json(nlohmann::json& j, const IR& v)
{
    j = nlohmann::json{};
    j["versionMajor"] = v.versionMajor;
    j["versionMinor"] = v.versionMinor;
    j["versionPatch"] = v.versionPatch;
    j["structs"] = v.structs;
    j["enums"] = v.enums;
    j["functions"] = v.functions;
    j["policies"] = v.policies;
}
} // namespace tpp
