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

struct IfConditionKind;
struct SourcePosition;
struct SourceRange;
struct ExprPathSegment;
struct ExprInfo;
struct EmitInstr;
struct EmitExprInstr;
struct BeginCapturedBlockInstr;
struct ForInstr;
struct CaseBinding;
struct CaseInstr;
struct IfInstr;
struct SwitchInstr;
struct CallInstr;
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
struct TypeKind;
struct Instruction;

struct IfConditionKind_Bool
{
    friend void to_json(nlohmann::json& j, const IfConditionKind_Bool&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, IfConditionKind_Bool&) {}
};
struct IfConditionKind_Presence
{
    friend void to_json(nlohmann::json& j, const IfConditionKind_Presence&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, IfConditionKind_Presence&) {}
};
struct IfConditionKind
{
    using Value = std::variant<IfConditionKind_Bool, IfConditionKind_Presence>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "enum IfConditionKind\n{\n    Bool,\n    Presence\n}";
    }
};
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
/**
One named field access in an expression path.
 */
struct ExprPathSegment
{
    std::string field;
    static std::string tpp_typedefs() noexcept
    {
        return "// ── Instruction types (backend-neutral) ─────────────────────────────────────\n\n/// One named field access in an expression path.\nstruct ExprPathSegment\n{\n    field : string;\n}";
    }
};
/**
Expression with resolved type.
 */
struct ExprInfo
{
    std::string bindingName;
    std::vector<ExprPathSegment> segments;
    std::unique_ptr<TypeKind> type;
    bool isRecursive;
    bool isOptional;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Expression with resolved type.\nstruct ExprInfo\n{\n    bindingName : string;\n    segments : list<ExprPathSegment>;\n    type : TypeKind;\n    isRecursive : bool;\n    isOptional : bool;\n}";
    }
};
/**
Emit literal text.
 */
struct EmitInstr
{
    std::string text;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Emit literal text.\nstruct EmitInstr\n{\n    text : string;\n}";
    }
};
/**
Emit an interpolated expression value.
 */
struct EmitExprInstr
{
    ExprInfo expr;
    std::string policy;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Emit an interpolated expression value.\nstruct EmitExprInstr\n{\n    expr : ExprInfo;\n    policy : string;\n}";
    }
};
/**
Begin capturing a block for deferred output.

When the matching `EmitCapturedBlock` instruction closes the scope, the
runtime re-indents the captured block before emitting it. When present,
`blockIndentInParentBlock` records the block's source-derived indentation
baseline relative to the parent block. When absent, placement comes
entirely from the live insertion column at the point where the block is
emitted.
 */
struct BeginCapturedBlockInstr
{
    std::optional<int> blockIndentInParentBlock;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Begin capturing a block for deferred output.\n///\n/// When the matching `EmitCapturedBlock` instruction closes the scope, the\n/// runtime re-indents the captured block before emitting it. When present,\n/// `blockIndentInParentBlock` records the block's source-derived indentation\n/// baseline relative to the parent block. When absent, placement comes\n/// entirely from the live insertion column at the point where the block is\n/// emitted.\nstruct BeginCapturedBlockInstr\n{\n    blockIndentInParentBlock : optional<int>;\n}";
    }
};
/**
For loop over a collection.
 */
struct ForInstr
{
    std::string varName;
    std::optional<std::string> enumeratorName;
    ExprInfo collection;
    std::unique_ptr<TypeKind> elementType;
    std::unique_ptr<std::vector<Instruction>> body;
    std::optional<std::string> sep;
    std::optional<std::string> followedBy;
    std::optional<std::string> precededBy;
    std::string policy;
    std::optional<std::string> alignSpec;
    static std::string tpp_typedefs() noexcept
    {
        return "/// For loop over a collection.\nstruct ForInstr\n{\n    varName : string;\n    enumeratorName : optional<string>;\n    collection : ExprInfo;\n    elementType : TypeKind;\n    body : list<Instruction>;\n    sep : optional<string>;\n    followedBy : optional<string>;\n    precededBy : optional<string>;\n    policy : string;\n    alignSpec : optional<string>;\n}";
    }
};
struct CaseBinding
{
    std::string name;
    bool isRecursive;
    static std::string tpp_typedefs() noexcept
    {
        return "struct CaseBinding\n{\n    name : string;\n    isRecursive : bool;\n}";
    }
};
/**
One case branch in a switch.
 */
struct CaseInstr
{
    std::string tag;
    std::optional<CaseBinding> bindingName;
    std::unique_ptr<TypeKind> payloadType;
    bool payloadIsRecursive;
    std::unique_ptr<std::vector<Instruction>> body;
    int variantIndex;
    static std::string tpp_typedefs() noexcept
    {
        return "/// One case branch in a switch.\nstruct CaseInstr\n{\n    tag : string;\n    bindingName : optional<CaseBinding>;\n    payloadType : optional<TypeKind>;\n    payloadIsRecursive : bool;\n    body : list<Instruction>;\n    variantIndex : int;\n}";
    }
};
/**
Conditional.
 */
struct IfInstr
{
    ExprInfo condExpr;
    IfConditionKind conditionKind;
    bool negated;
    std::unique_ptr<std::vector<Instruction>> thenBody;
    std::unique_ptr<std::vector<Instruction>> elseBody;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Conditional.\nstruct IfInstr\n{\n    condExpr : ExprInfo;\n    conditionKind : IfConditionKind;\n    negated : bool;\n    thenBody : list<Instruction>;\n    elseBody : list<Instruction>;\n}";
    }
};
/**
Switch on a variant expression.
 */
struct SwitchInstr
{
    ExprInfo expr;
    std::unique_ptr<std::vector<CaseInstr>> cases;
    std::string policy;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Switch on a variant expression.\nstruct SwitchInstr\n{\n    expr : ExprInfo;\n    cases : list<CaseInstr>;\n    policy : string;\n}";
    }
};
/**
Direct function call.
 */
struct CallInstr
{
    std::string functionName;
    int functionIndex;
    std::vector<ExprInfo> arguments;
    static std::string tpp_typedefs() noexcept
    {
        return "/// Direct function call.\nstruct CallInstr\n{\n    functionName : string;\n    functionIndex : int;\n    arguments : list<ExprInfo>;\n}";
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
    std::optional<std::string> compiledReplace;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyRequire\n{\n    regex : string;\n    replace : string;\n    compiledReplace : optional<string>;\n}";
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
    std::string identifier;
    std::optional<PolicyLength> length;
    std::optional<PolicyRejectIf> rejectIf;
    std::vector<PolicyRequire> require;
    std::vector<PolicyReplacement> replacements;
    std::vector<PolicyOutputFilter> outputFilter;
    static std::string tpp_typedefs() noexcept
    {
        return "struct PolicyDef\n{\n    tag : string;\n    identifier : string;\n    length : optional<PolicyLength>;\n    rejectIf : optional<PolicyRejectIf>;\n    require : list<PolicyRequire>;\n    replacements : list<PolicyReplacement>;\n    outputFilter : list<PolicyOutputFilter>;\n}";
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
struct Instruction_EmitCapturedBlock
{
    friend void to_json(nlohmann::json& j, const Instruction_EmitCapturedBlock&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, Instruction_EmitCapturedBlock&) {}
};
struct Instruction
{
    using Value = std::variant<EmitInstr, EmitExprInstr, Instruction_AlignCell, std::unique_ptr<ForInstr>, std::unique_ptr<IfInstr>, std::unique_ptr<SwitchInstr>, CallInstr, BeginCapturedBlockInstr, Instruction_EmitCapturedBlock>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return "enum Instruction\n{\n    Emit(EmitInstr),\n    EmitExpr(EmitExprInstr),\n    AlignCell,\n    For(ForInstr),\n    If(IfInstr),\n    Switch(SwitchInstr),\n    Call(CallInstr),\n    BeginCapturedBlock(BeginCapturedBlockInstr),\n    EmitCapturedBlock\n}";
    }
};

inline void from_json(const nlohmann::json& j, IfConditionKind& v);
inline void to_json(nlohmann::json& j, const IfConditionKind& v);
inline void from_json(const nlohmann::json& j, TypeKind& v);
inline void to_json(nlohmann::json& j, const TypeKind& v);
inline void from_json(const nlohmann::json& j, Instruction& v);
inline void to_json(nlohmann::json& j, const Instruction& v);
inline void from_json(const nlohmann::json& j, SourcePosition& v);
inline void to_json(nlohmann::json& j, const SourcePosition& v);
inline void from_json(const nlohmann::json& j, SourceRange& v);
inline void to_json(nlohmann::json& j, const SourceRange& v);
inline void from_json(const nlohmann::json& j, ExprPathSegment& v);
inline void to_json(nlohmann::json& j, const ExprPathSegment& v);
inline void from_json(const nlohmann::json& j, ExprInfo& v);
inline void to_json(nlohmann::json& j, const ExprInfo& v);
inline void from_json(const nlohmann::json& j, EmitInstr& v);
inline void to_json(nlohmann::json& j, const EmitInstr& v);
inline void from_json(const nlohmann::json& j, EmitExprInstr& v);
inline void to_json(nlohmann::json& j, const EmitExprInstr& v);
inline void from_json(const nlohmann::json& j, BeginCapturedBlockInstr& v);
inline void to_json(nlohmann::json& j, const BeginCapturedBlockInstr& v);
inline void from_json(const nlohmann::json& j, ForInstr& v);
inline void to_json(nlohmann::json& j, const ForInstr& v);
inline void from_json(const nlohmann::json& j, CaseBinding& v);
inline void to_json(nlohmann::json& j, const CaseBinding& v);
inline void from_json(const nlohmann::json& j, CaseInstr& v);
inline void to_json(nlohmann::json& j, const CaseInstr& v);
inline void from_json(const nlohmann::json& j, IfInstr& v);
inline void to_json(nlohmann::json& j, const IfInstr& v);
inline void from_json(const nlohmann::json& j, SwitchInstr& v);
inline void to_json(nlohmann::json& j, const SwitchInstr& v);
inline void from_json(const nlohmann::json& j, CallInstr& v);
inline void to_json(nlohmann::json& j, const CallInstr& v);
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

inline void from_json(const nlohmann::json& j, IfConditionKind& v)
{
    if (j.contains("Bool")) v.value.emplace<0>();

    else if (j.contains("Presence")) v.value.emplace<1>();
}
inline void to_json(nlohmann::json& j, const IfConditionKind& v)
{
    const char* _tags[] = {"Bool", "Presence"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, TypeKind& v)
{
    if (j.contains("Str")) v.value.emplace<0>();

    else if (j.contains("Int")) v.value.emplace<1>();

    else if (j.contains("Bool")) v.value.emplace<2>();

    else if (j.contains("Named")) v.value.emplace<3>(j["Named"].get<std::string>());

    else if (j.contains("List")) v.value.emplace<4>(std::make_unique<TypeKind>(j["List"].get<TypeKind>()));

    else if (j.contains("Optional")) v.value.emplace<5>(std::make_unique<TypeKind>(j["Optional"].get<TypeKind>()));
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

    else if (j.contains("EmitExpr")) v.value.emplace<1>(j["EmitExpr"].get<EmitExprInstr>());

    else if (j.contains("AlignCell")) v.value.emplace<2>();

    else if (j.contains("For")) v.value.emplace<3>(std::make_unique<ForInstr>(j["For"].get<ForInstr>()));

    else if (j.contains("If")) v.value.emplace<4>(std::make_unique<IfInstr>(j["If"].get<IfInstr>()));

    else if (j.contains("Switch")) v.value.emplace<5>(std::make_unique<SwitchInstr>(j["Switch"].get<SwitchInstr>()));

    else if (j.contains("Call")) v.value.emplace<6>(j["Call"].get<CallInstr>());

    else if (j.contains("BeginCapturedBlock")) v.value.emplace<7>(j["BeginCapturedBlock"].get<BeginCapturedBlockInstr>());

    else if (j.contains("EmitCapturedBlock")) v.value.emplace<8>();
}
inline void to_json(nlohmann::json& j, const Instruction& v)
{
    const char* _tags[] = {"Emit", "EmitExpr", "AlignCell", "For", "If", "Switch", "Call", "BeginCapturedBlock", "EmitCapturedBlock"};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
inline void from_json(const nlohmann::json& j, SourcePosition& v)
{
    v.line = j.at("line").get<int>();
    v.character = j.at("character").get<int>();
}
inline void to_json(nlohmann::json& j, const SourcePosition& v)
{
    j = nlohmann::json{};
    j["line"] = v.line;
    j["character"] = v.character;
}
inline void from_json(const nlohmann::json& j, SourceRange& v)
{
    v.start = j.at("start").get<SourcePosition>();
    v.end = j.at("end").get<SourcePosition>();
}
inline void to_json(nlohmann::json& j, const SourceRange& v)
{
    j = nlohmann::json{};
    j["start"] = v.start;
    j["end"] = v.end;
}
inline void from_json(const nlohmann::json& j, ExprPathSegment& v)
{
    v.field = j.at("field").get<std::string>();
}
inline void to_json(nlohmann::json& j, const ExprPathSegment& v)
{
    j = nlohmann::json{};
    j["field"] = v.field;
}
inline void from_json(const nlohmann::json& j, ExprInfo& v)
{
    v.bindingName = j.at("bindingName").get<std::string>();
    v.segments = j.at("segments").get<std::vector<ExprPathSegment>>();
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
    v.isRecursive = j.at("isRecursive").get<bool>();
    v.isOptional = j.at("isOptional").get<bool>();
}
inline void to_json(nlohmann::json& j, const ExprInfo& v)
{
    j = nlohmann::json{};
    j["bindingName"] = v.bindingName;
    j["segments"] = v.segments;
    j["type"] = *v.type;
    j["isRecursive"] = v.isRecursive;
    j["isOptional"] = v.isOptional;
}
inline void from_json(const nlohmann::json& j, EmitInstr& v)
{
    v.text = j.at("text").get<std::string>();
}
inline void to_json(nlohmann::json& j, const EmitInstr& v)
{
    j = nlohmann::json{};
    j["text"] = v.text;
}
inline void from_json(const nlohmann::json& j, EmitExprInstr& v)
{
    v.expr = j.at("expr").get<ExprInfo>();
    v.policy = j.at("policy").get<std::string>();
}
inline void to_json(nlohmann::json& j, const EmitExprInstr& v)
{
    j = nlohmann::json{};
    j["expr"] = v.expr;
    j["policy"] = v.policy;
}
inline void from_json(const nlohmann::json& j, BeginCapturedBlockInstr& v)
{
    if (j.contains("blockIndentInParentBlock") && !j.at("blockIndentInParentBlock").is_null()) v.blockIndentInParentBlock = j.at("blockIndentInParentBlock").get<int>();
}
inline void to_json(nlohmann::json& j, const BeginCapturedBlockInstr& v)
{
    j = nlohmann::json{};
    if (v.blockIndentInParentBlock.has_value()) j["blockIndentInParentBlock"] = *v.blockIndentInParentBlock;
}
inline void from_json(const nlohmann::json& j, ForInstr& v)
{
    v.varName = j.at("varName").get<std::string>();
    if (j.contains("enumeratorName") && !j.at("enumeratorName").is_null()) v.enumeratorName = j.at("enumeratorName").get<std::string>();
    v.collection = j.at("collection").get<ExprInfo>();
    v.elementType = std::make_unique<TypeKind>(j.at("elementType").get<TypeKind>());
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
    if (j.contains("sep") && !j.at("sep").is_null()) v.sep = j.at("sep").get<std::string>();
    if (j.contains("followedBy") && !j.at("followedBy").is_null()) v.followedBy = j.at("followedBy").get<std::string>();
    if (j.contains("precededBy") && !j.at("precededBy").is_null()) v.precededBy = j.at("precededBy").get<std::string>();
    v.policy = j.at("policy").get<std::string>();
    if (j.contains("alignSpec") && !j.at("alignSpec").is_null()) v.alignSpec = j.at("alignSpec").get<std::string>();
}
inline void to_json(nlohmann::json& j, const ForInstr& v)
{
    j = nlohmann::json{};
    j["varName"] = v.varName;
    if (v.enumeratorName.has_value()) j["enumeratorName"] = *v.enumeratorName;
    j["collection"] = v.collection;
    j["elementType"] = *v.elementType;
    j["body"] = *v.body;
    if (v.sep.has_value()) j["sep"] = *v.sep;
    if (v.followedBy.has_value()) j["followedBy"] = *v.followedBy;
    if (v.precededBy.has_value()) j["precededBy"] = *v.precededBy;
    j["policy"] = v.policy;
    if (v.alignSpec.has_value()) j["alignSpec"] = *v.alignSpec;
}
inline void from_json(const nlohmann::json& j, CaseBinding& v)
{
    v.name = j.at("name").get<std::string>();
    v.isRecursive = j.at("isRecursive").get<bool>();
}
inline void to_json(nlohmann::json& j, const CaseBinding& v)
{
    j = nlohmann::json{};
    j["name"] = v.name;
    j["isRecursive"] = v.isRecursive;
}
inline void from_json(const nlohmann::json& j, CaseInstr& v)
{
    v.tag = j.at("tag").get<std::string>();
    if (j.contains("bindingName") && !j.at("bindingName").is_null()) v.bindingName = j.at("bindingName").get<CaseBinding>();
    if (j.contains("payloadType") && !j.at("payloadType").is_null()) v.payloadType = std::make_unique<TypeKind>(j.at("payloadType").get<TypeKind>());
    v.payloadIsRecursive = j.at("payloadIsRecursive").get<bool>();
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
    v.variantIndex = j.at("variantIndex").get<int>();
}
inline void to_json(nlohmann::json& j, const CaseInstr& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    if (v.bindingName.has_value()) j["bindingName"] = *v.bindingName;
    if (v.payloadType) j["payloadType"] = *v.payloadType;
    j["payloadIsRecursive"] = v.payloadIsRecursive;
    j["body"] = *v.body;
    j["variantIndex"] = v.variantIndex;
}
inline void from_json(const nlohmann::json& j, IfInstr& v)
{
    v.condExpr = j.at("condExpr").get<ExprInfo>();
    v.conditionKind = j.at("conditionKind").get<IfConditionKind>();
    v.negated = j.at("negated").get<bool>();
    v.thenBody = std::make_unique<std::vector<Instruction>>(j.at("thenBody").get<std::vector<Instruction>>());
    v.elseBody = std::make_unique<std::vector<Instruction>>(j.at("elseBody").get<std::vector<Instruction>>());
}
inline void to_json(nlohmann::json& j, const IfInstr& v)
{
    j = nlohmann::json{};
    j["condExpr"] = v.condExpr;
    j["conditionKind"] = v.conditionKind;
    j["negated"] = v.negated;
    j["thenBody"] = *v.thenBody;
    j["elseBody"] = *v.elseBody;
}
inline void from_json(const nlohmann::json& j, SwitchInstr& v)
{
    v.expr = j.at("expr").get<ExprInfo>();
    v.cases = std::make_unique<std::vector<CaseInstr>>(j.at("cases").get<std::vector<CaseInstr>>());
    v.policy = j.at("policy").get<std::string>();
}
inline void to_json(nlohmann::json& j, const SwitchInstr& v)
{
    j = nlohmann::json{};
    j["expr"] = v.expr;
    j["cases"] = *v.cases;
    j["policy"] = v.policy;
}
inline void from_json(const nlohmann::json& j, CallInstr& v)
{
    v.functionName = j.at("functionName").get<std::string>();
    v.functionIndex = j.at("functionIndex").get<int>();
    v.arguments = j.at("arguments").get<std::vector<ExprInfo>>();
}
inline void to_json(nlohmann::json& j, const CallInstr& v)
{
    j = nlohmann::json{};
    j["functionName"] = v.functionName;
    j["functionIndex"] = v.functionIndex;
    j["arguments"] = v.arguments;
}
inline void from_json(const nlohmann::json& j, ParamDef& v)
{
    v.name = j.at("name").get<std::string>();
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
    v.name = j.at("name").get<std::string>();
    v.params = j.at("params").get<std::vector<ParamDef>>();
    v.body = std::make_unique<std::vector<Instruction>>(j.at("body").get<std::vector<Instruction>>());
    v.policy = j.at("policy").get<std::string>();
    v.doc = j.at("doc").get<std::string>();
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
    v.name = j.at("name").get<std::string>();
    v.type = std::make_unique<TypeKind>(j.at("type").get<TypeKind>());
    v.recursive = j.at("recursive").get<bool>();
    v.doc = j.at("doc").get<std::string>();
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
    v.name = j.at("name").get<std::string>();
    v.fields = j.at("fields").get<std::vector<FieldDef>>();
    v.doc = j.at("doc").get<std::string>();
    if (j.contains("sourceRange") && !j.at("sourceRange").is_null()) v.sourceRange = j.at("sourceRange").get<SourceRange>();
    v.rawTypedefs = j.at("rawTypedefs").get<std::string>();
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
    v.tag = j.at("tag").get<std::string>();
    if (j.contains("payload") && !j.at("payload").is_null()) v.payload = std::make_unique<TypeKind>(j.at("payload").get<TypeKind>());
    v.recursive = j.at("recursive").get<bool>();
    v.doc = j.at("doc").get<std::string>();
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
    v.name = j.at("name").get<std::string>();
    v.variants = j.at("variants").get<std::vector<VariantDef>>();
    v.doc = j.at("doc").get<std::string>();
    if (j.contains("sourceRange") && !j.at("sourceRange").is_null()) v.sourceRange = j.at("sourceRange").get<SourceRange>();
    v.rawTypedefs = j.at("rawTypedefs").get<std::string>();
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
    v.find = j.at("find").get<std::string>();
    v.replace = j.at("replace").get<std::string>();
}
inline void to_json(nlohmann::json& j, const PolicyReplacement& v)
{
    j = nlohmann::json{};
    j["find"] = v.find;
    j["replace"] = v.replace;
}
inline void from_json(const nlohmann::json& j, PolicyRequire& v)
{
    v.regex = j.at("regex").get<std::string>();
    v.replace = j.at("replace").get<std::string>();
    if (j.contains("compiledReplace") && !j.at("compiledReplace").is_null()) v.compiledReplace = j.at("compiledReplace").get<std::string>();
}
inline void to_json(nlohmann::json& j, const PolicyRequire& v)
{
    j = nlohmann::json{};
    j["regex"] = v.regex;
    j["replace"] = v.replace;
    if (v.compiledReplace.has_value()) j["compiledReplace"] = *v.compiledReplace;
}
inline void from_json(const nlohmann::json& j, PolicyOutputFilter& v)
{
    v.regex = j.at("regex").get<std::string>();
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
    v.regex = j.at("regex").get<std::string>();
    v.message = j.at("message").get<std::string>();
}
inline void to_json(nlohmann::json& j, const PolicyRejectIf& v)
{
    j = nlohmann::json{};
    j["regex"] = v.regex;
    j["message"] = v.message;
}
inline void from_json(const nlohmann::json& j, PolicyDef& v)
{
    v.tag = j.at("tag").get<std::string>();
    v.identifier = j.at("identifier").get<std::string>();
    if (j.contains("length") && !j.at("length").is_null()) v.length = j.at("length").get<PolicyLength>();
    if (j.contains("rejectIf") && !j.at("rejectIf").is_null()) v.rejectIf = j.at("rejectIf").get<PolicyRejectIf>();
    v.require = j.at("require").get<std::vector<PolicyRequire>>();
    v.replacements = j.at("replacements").get<std::vector<PolicyReplacement>>();
    v.outputFilter = j.at("outputFilter").get<std::vector<PolicyOutputFilter>>();
}
inline void to_json(nlohmann::json& j, const PolicyDef& v)
{
    j = nlohmann::json{};
    j["tag"] = v.tag;
    j["identifier"] = v.identifier;
    if (v.length.has_value()) j["length"] = *v.length;
    if (v.rejectIf.has_value()) j["rejectIf"] = *v.rejectIf;
    j["require"] = v.require;
    j["replacements"] = v.replacements;
    j["outputFilter"] = v.outputFilter;
}
inline void from_json(const nlohmann::json& j, IR& v)
{
    v.versionMajor = j.at("versionMajor").get<int>();
    v.versionMinor = j.at("versionMinor").get<int>();
    v.versionPatch = j.at("versionPatch").get<int>();
    v.structs = j.at("structs").get<std::vector<StructDef>>();
    v.enums = j.at("enums").get<std::vector<EnumDef>>();
    v.functions = j.at("functions").get<std::vector<FunctionDef>>();
    v.policies = j.at("policies").get<std::vector<PolicyDef>>();
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
