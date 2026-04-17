template cpp_ir_type(t: TypeKind)
@switch t@@case Str@std::string@end case@@case Int@int@end case@@case Bool@bool@end case@@case Named(n)@@n@@end case@@case List(e)@std::vector<@cpp_ir_type(e)@>@end case@@case Optional(inner)@std::optional<@cpp_ir_type(inner)@>@end case@@end switch@
END

template cpp_ir_inner_type(t: TypeKind)
@switch t@@case Optional(inner)@@cpp_ir_type(inner)@@end case@@case Str@std::string@end case@@case Int@int@end case@@case Bool@bool@end case@@case Named(n)@@n@@end case@@case List(e)@std::vector<@cpp_ir_type(e)@>@end case@@end switch@
END

template cpp_struct_field_decl(field: FieldDef)
@if field.recursive@
std::unique_ptr<@cpp_ir_inner_type(field.type)@> @field.name@;
@else@
@cpp_ir_type(field.type)@ @field.name@;
@end if@
END

template cpp_read_field(field: FieldDef)
@if field.recursive@
@switch field.type@
@case Optional(inner)@
if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = std::make_unique<@cpp_ir_type(inner)@>(j.at("@field.name@").get<@cpp_ir_type(inner)@>());
@end case@
@case Str@
v.@field.name@ = std::make_unique<std::string>(j.at("@field.name@").get<std::string>());
@end case@
@case Int@
v.@field.name@ = std::make_unique<int>(j.at("@field.name@").get<int>());
@end case@
@case Bool@
v.@field.name@ = std::make_unique<bool>(j.at("@field.name@").get<bool>());
@end case@
@case Named(n)@
v.@field.name@ = std::make_unique<@n@>(j.at("@field.name@").get<@n@>());
@end case@
@case List(e)@
v.@field.name@ = std::make_unique<std::vector<@cpp_ir_type(e)@>>(j.at("@field.name@").get<std::vector<@cpp_ir_type(e)@>>());
@end case@
@end switch@
@else@
@switch field.type@
@case Optional(inner)@
if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = j.at("@field.name@").get<@cpp_ir_type(inner)@>();
@end case@
@case Str@
v.@field.name@ = j.at("@field.name@").get<std::string>();
@end case@
@case Int@
v.@field.name@ = j.at("@field.name@").get<int>();
@end case@
@case Bool@
v.@field.name@ = j.at("@field.name@").get<bool>();
@end case@
@case Named(n)@
v.@field.name@ = j.at("@field.name@").get<@n@>();
@end case@
@case List(e)@
v.@field.name@ = j.at("@field.name@").get<std::vector<@cpp_ir_type(e)@>>();
@end case@
@end switch@
@end if@
END

template cpp_write_field(field: FieldDef)
@if field.recursive@
@switch field.type@
@case Optional(inner)@
if (v.@field.name@) j["@field.name@"] = *v.@field.name@;
@end case@
@case Str@
j["@field.name@"] = *v.@field.name@;
@end case@
@case Int@
j["@field.name@"] = *v.@field.name@;
@end case@
@case Bool@
j["@field.name@"] = *v.@field.name@;
@end case@
@case Named(n)@
j["@field.name@"] = *v.@field.name@;
@end case@
@case List(e)@
j["@field.name@"] = *v.@field.name@;
@end case@
@end switch@
@else@
@switch field.type@
@case Optional(inner)@
if (v.@field.name@.has_value()) j["@field.name@"] = *v.@field.name@;
@end case@
@case Str@
j["@field.name@"] = v.@field.name@;
@end case@
@case Int@
j["@field.name@"] = v.@field.name@;
@end case@
@case Bool@
j["@field.name@"] = v.@field.name@;
@end case@
@case Named(n)@
j["@field.name@"] = v.@field.name@;
@end case@
@case List(e)@
j["@field.name@"] = v.@field.name@;
@end case@
@end switch@
@end if@
END

template render_cpp_enum(e: EnumDef)
@for variant in e.variants@
@if not variant.payload@
struct @e.name@_@variant.tag@
{
    friend void to_json(nlohmann::json& j, const @e.name@_@variant.tag@&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, @e.name@_@variant.tag@&) {}
};
@end if@
@end for@
struct @e.name@
{
    using Value = std::variant<@for variant in e.variants | enumerator=variantIndex sep=", "@@if not variant.payload@@e.name@_@variant.tag@@else@@if variant.recursive@std::unique_ptr<@cpp_ir_type(variant.payload)@>@else@@cpp_ir_type(variant.payload)@@end if@@end if@@end for@>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return @e.rawTypedefs@;
    }
};
END

template render_cpp_struct(s: StructDef)
struct @s.name@
{
    @for field in s.fields@
    @cpp_struct_field_decl(field)@
    @end for@
    static std::string tpp_typedefs() noexcept
    {
        return @s.rawTypedefs@;
    }
};
END

template render_cpp_types(preStructEnums: list<EnumDef>, structs: list<StructDef>, postStructEnums: list<EnumDef>, includes: list<string>, namespaceName: optional<string>)
#pragma once
@for inc in includes@
#include "@inc@"
@end for@
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>
@if namespaceName@
namespace @namespaceName@ {
@end if@
template<typename _TppT>
inline nlohmann::json _tpp_j(const _TppT& v) { return nlohmann::json(v); }
template<typename _TppT>
inline nlohmann::json _tpp_j(const std::unique_ptr<_TppT>& v) { return v ? nlohmann::json(*v) : nlohmann::json(); }

@for e in preStructEnums@
struct @e.name@;
@end for@
@for s in structs@
struct @s.name@;
@end for@
@for e in postStructEnums@
struct @e.name@;
@end for@

@for e in preStructEnums@
@render_cpp_enum(e)@
@end for@
@for s in structs@
@render_cpp_struct(s)@
@end for@
@for e in postStructEnums@
@render_cpp_enum(e)@
@end for@

@for e in preStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v);
inline void to_json(nlohmann::json& j, const @e.name@& v);
@end for@
@for e in postStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v);
inline void to_json(nlohmann::json& j, const @e.name@& v);
@end for@
@for s in structs@
inline void from_json(const nlohmann::json& j, @s.name@& v);
inline void to_json(nlohmann::json& j, const @s.name@& v);
@end for@

@for e in preStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v)
{
    @for variant in e.variants | enumerator=variantIndex sep="\n    else "@
    if (j.contains("@variant.tag@")) @if not variant.payload@v.value.emplace<@variantIndex@>();@else@@if variant.recursive@v.value.emplace<@variantIndex@>(std::make_unique<@cpp_ir_type(variant.payload)@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>()));@else@v.value.emplace<@variantIndex@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>());@end if@@end if@
    @end for@
}
inline void to_json(nlohmann::json& j, const @e.name@& v)
{
    const char* _tags[] = {@for variant in e.variants | sep=", "@"@variant.tag@"@end for@};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
@end for@
@for e in postStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v)
{
    @for variant in e.variants | enumerator=variantIndex sep="\n    else "@
    if (j.contains("@variant.tag@")) @if not variant.payload@v.value.emplace<@variantIndex@>();@else@@if variant.recursive@v.value.emplace<@variantIndex@>(std::make_unique<@cpp_ir_type(variant.payload)@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>()));@else@v.value.emplace<@variantIndex@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>());@end if@@end if@
    @end for@
}
inline void to_json(nlohmann::json& j, const @e.name@& v)
{
    const char* _tags[] = {@for variant in e.variants | sep=", "@"@variant.tag@"@end for@};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
@end for@
@for s in structs@
inline void from_json(const nlohmann::json& j, @s.name@& v)
{
    @for field in s.fields@
    @cpp_read_field(field)@
    @end for@
}
inline void to_json(nlohmann::json& j, const @s.name@& v)
{
    j = nlohmann::json{};
    @for field in s.fields@
    @cpp_write_field(field)@
    @end for@
}
@end for@
@if namespaceName@
} // namespace @namespaceName@
@end if@
END

template render_cpp_functions(functions: list<FunctionDef>, includes: list<string>, namespaceName: optional<string>, functionPrefix: string)
#pragma once
#include <tpp/ArgType.h>
@for inc in includes@
#include "@inc@"
@end for@
#include <string>
@if namespaceName@
namespace @namespaceName@ {
@end if@
@for function in functions@
std::string @functionPrefix@@function.name@(@for param in function.params | sep=", "@typename tpp::ArgType<@cpp_ir_type(param.type)@>::type @param.name@@end for@);
@end for@
@if namespaceName@
} // namespace @namespaceName@
@end if@
END

// ═══════════════════════════════════════════════════════════════════════════════
// Instruction IR → C++ rendering functions (native code generation)
// ═══════════════════════════════════════════════════════════════════════════════

// ── Expression → C++ String expression ───────────────────────────────────────

template cpp_value_path(path: string, isRecursive: bool, isOptional: bool)
@if isRecursive@*@path@@else@@if isOptional@*@path@@else@@path@@end if@@end if@
END

template cpp_optional_to_str(expr: RenderExprInfo, inner: RenderTypeKind)
@switch inner@
@case Str@
@if expr.isRecursive@(@expr.path@ ? *@expr.path@ : std::string{})@else@(@expr.path@.has_value() ? *@expr.path@ : std::string{})@end if@@end case@
@case Int@
@if expr.isRecursive@(@expr.path@ ? std::to_string(*@expr.path@) : std::string{})@else@(@expr.path@.has_value() ? std::to_string(*@expr.path@) : std::string{})@end if@@end case@
@case Bool@
@if expr.isRecursive@(@expr.path@ ? (*@expr.path@ ? "true" : "false") : std::string{})@else@(@expr.path@.has_value() ? (*@expr.path@ ? "true" : "false") : std::string{})@end if@@end case@
@case Named(n)@
@if expr.isRecursive@(@expr.path@ ? std::to_string(*@expr.path@) : std::string{})@else@(@expr.path@.has_value() ? std::to_string(*@expr.path@) : std::string{})@end if@@end case@
@case List(e)@
@if expr.isRecursive@(@expr.path@ ? std::to_string(*@expr.path@) : std::string{})@else@(@expr.path@.has_value() ? std::to_string(*@expr.path@) : std::string{})@end if@@end case@
@case Optional(i)@
@if expr.isRecursive@(@expr.path@ ? std::to_string(*@expr.path@) : std::string{})@else@(@expr.path@.has_value() ? std::to_string(*@expr.path@) : std::string{})@end if@@end case@
@end switch@
END

template cpp_expr_to_str(expr: RenderExprInfo)
@switch expr.type@
@case Str@
@cpp_value_path(expr.path, expr.isRecursive, expr.isOptional)@@end case@
@case Int@
std::to_string(@cpp_value_path(expr.path, expr.isRecursive, expr.isOptional)@)@end case@
@case Bool@
(@cpp_value_path(expr.path, expr.isRecursive, expr.isOptional)@ ? "true" : "false")@end case@
@case Named(n)@
std::to_string(@cpp_value_path(expr.path, expr.isRecursive, expr.isOptional)@)@end case@
@case List(e)@
std::to_string(@cpp_value_path(expr.path, expr.isRecursive, expr.isOptional)@)@end case@
@case Optional(inner)@
@cpp_optional_to_str(expr, inner)@@end case@
@end switch@
END

// ── Leaf instruction templates ───────────────────────────────────────────────

template emit_emit(e: EmitData)
@e.sb@ += @e.textLit@;
END

template emit_emit_expr(e: EmitExprData)
@if e.staticPolicyId@
{ std::string _pv = @cpp_expr_to_str(e.expr)@; _pv = TppPolicy::@e.staticPolicyId@.apply(_pv); _tppAppendValue(@e.sb@, _pv); }
@else@
@if e.useRuntimePolicy@
{ std::string _pv = @cpp_expr_to_str(e.expr)@; _pv = _policy.apply(_pv); _tppAppendValue(@e.sb@, _pv); }
@else@
_tppAppendValue(@e.sb@, @cpp_expr_to_str(e.expr)@);
@end if@
@end if@
END

template emit_call(c: CallData)
@if c.policyArg@
@c.sb@ += @c.functionName@(@for arg in c.args | sep=", " followedBy=", "@@cpp_call_arg(arg)@@end for@@cpp_policy_ref(c.policyArg)@);
@else@
@c.sb@ += @c.functionName@(@for arg in c.args | sep=", "@@cpp_call_arg(arg)@@end for@);
@end if@
END

template cpp_call_arg(arg: CallArgInfo)
@cpp_value_path(arg.path, arg.isRecursive, arg.isOptional)@
END

template cpp_policy_ref(ref: PolicyRef)
@switch ref@@case Named(tag)@TppPolicy::@tag@@end case@@case Pure@TppPolicy::pure@end case@@case Runtime@_policy@end case@@end switch@
END

// ── Recursive instruction dispatcher (block-style switch — avoids bug) ───────

template emit_instr(instr: RenderInstruction)
@switch instr@
@case Emit(e)@
@emit_emit(e)@
@end case@
@case EmitExpr(e)@
@emit_emit_expr(e)@
@end case@
@case AlignCell@
@end case@
@case For(f)@
@emit_for(f)@
@end case@
@case If(i)@
@emit_if(i)@
@end case@
@case Switch(s)@
@emit_switch(s)@
@end case@
@case Call(c)@
@emit_call(c)@
@end case@
@end switch@
END

// ── For loop ─────────────────────────────────────────────────────────────────

template cpp_type(t: RenderTypeKind)
@switch t@@case Str@std::string@end case@@case Int@int@end case@@case Bool@bool@end case@@case Named(n)@@n@@end case@@case List(e)@std::vector<@cpp_type(e)@>@end case@@case Optional(inner)@std::optional<@cpp_type(inner)@>@end case@@end switch@
END

template emit_for(f: ForData)
@if f.cells@
@emit_aligned_for(f)@
@else@
@if f.isBlock@
@emit_for_block(f)@
@else@
@emit_for_inline(f)@
@end if@
@end if@
END

template emit_for_inline(f: ForData)
@if f.body@
for (size_t _i@f.scopeId@ = 0; _i@f.scopeId@ < (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).size(); _i@f.scopeId@++) {
    [[maybe_unused]] const auto& @f.varName@ = (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@)[_i@f.scopeId@];
    @if f.enumeratorName@
    int @f.enumeratorName@ = (int)_i@f.scopeId@;
    @end if@
    @if f.precededByLit@
    @f.sb@ += @f.precededByLit@;
    @end if@
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    @if f.sepLit@
    if (_i@f.scopeId@ + 1 < (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).size()) @f.sb@ += @f.sepLit@;
    @if f.followedByLit@
    else if (!(@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @else@
    @if f.followedByLit@
    if (_i@f.scopeId@ + 1 >= (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).size() && !(@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @end if@
}
@end if@
END

template emit_for_block_sep(f: ForData)
@if f.sepLit@
bool _stripped@f.scopeId@ = false;
if (!_iter@f.scopeId@.empty() && _iter@f.scopeId@.back() == '\n') {
    if (std::count(_iter@f.scopeId@.begin(), _iter@f.scopeId@.end(), '\n') == 1) { _iter@f.scopeId@.pop_back(); _stripped@f.scopeId@ = true; }
}
@if f.precededByLit@
@f.sb@ += @f.precededByLit@;
@end if@
@f.sb@ += _iter@f.scopeId@;
if (_i@f.scopeId@ + 1 < (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).size()) {
    @f.sb@ += @f.sepLit@;
} else {
    @if f.followedByLit@
    if (!(@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    if (_stripped@f.scopeId@) @f.sb@ += "\n";
}
@end if@
END

template emit_for_block(f: ForData)
@if f.body@
for (size_t _i@f.scopeId@ = 0; _i@f.scopeId@ < (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).size(); _i@f.scopeId@++) {
    [[maybe_unused]] const auto& @f.varName@ = (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@)[_i@f.scopeId@];
    @if f.enumeratorName@
    int @f.enumeratorName@ = (int)_i@f.scopeId@;
    @end if@
    std::string _blk@f.scopeId@;
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    std::string _iter@f.scopeId@ = _tppBlockIndent(_blk@f.scopeId@, @f.insertCol@);
    @if f.sepLit@
    @emit_for_block_sep(f)@
    @else@
    @if f.precededByLit@
    @f.sb@ += @f.precededByLit@;
    @end if@
    @f.sb@ += _iter@f.scopeId@;
    @if f.followedByLit@
    if (_i@f.scopeId@ + 1 >= (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).size() && !(@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @end if@
}
@end if@
END

// ── Aligned for ──────────────────────────────────────────────────────────────

template emit_align_cell(scopeId: int, cell: AlignCellInfo)
{
    std::string _cell@scopeId@_@cell.cellIndex@;
    @for instr in cell.body@
    @emit_instr(instr)@
    @end for@
    _row@scopeId@[@cell.cellIndex@] = _cell@scopeId@_@cell.cellIndex@;
}
END

template emit_aligned_for(f: ForData)
@if f.cells@
std::vector<std::vector<std::string>> _rows@f.scopeId@;
for (size_t _i@f.scopeId@ = 0; _i@f.scopeId@ < (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@).size(); _i@f.scopeId@++) {
    [[maybe_unused]] const auto& @f.varName@ = (@cpp_value_path(f.collPath, f.collIsRecursive, f.collIsOptional)@)[_i@f.scopeId@];
    @if f.enumeratorName@
    int @f.enumeratorName@ = (int)_i@f.scopeId@;
    @end if@
    std::vector<std::string> _row@f.scopeId@(@f.numCols@);
    @for cell in f.cells@
    @emit_align_cell(f.scopeId, cell)@
    @end for@
    _rows@f.scopeId@.push_back(_row@f.scopeId@);
}
std::vector<int> _cw@f.scopeId@(@f.numCols@, 0);
for (auto& _r : _rows@f.scopeId@) for (int _c = 0; _c < (int)_r.size(); _c++) _cw@f.scopeId@[_c] = std::max(_cw@f.scopeId@[_c], (int)_r[_c].size());
std::vector<char> _spec@f.scopeId@(@f.numCols@, 'l');
@if f.singleAlignChar@
@for ch in f.alignSpecChars@
std::fill(_spec@f.scopeId@.begin(), _spec@f.scopeId@.end(), '@ch@');
@end for@
@else@
@for ch in f.alignSpecChars | enumerator=ci@
_spec@f.scopeId@[@ci@] = '@ch@';
@end for@
@end if@
for (size_t _i@f.scopeId@ = 0; _i@f.scopeId@ < _rows@f.scopeId@.size(); _i@f.scopeId@++) {
    auto& _r = _rows@f.scopeId@[_i@f.scopeId@];
    std::string _line@f.scopeId@;
    for (int _c = 0; _c < (int)_r.size(); _c++) {
        if (_c + 1 < @f.numCols@) {
            _line@f.scopeId@ += _tppPadCell(_r[_c], _cw@f.scopeId@[_c], _spec@f.scopeId@[_c]);
        } else {
            char _sp = _spec@f.scopeId@[_c]; int _pd = _cw@f.scopeId@[_c] - (int)_r[_c].size();
            if (_pd > 0 && _sp != 'l') { int _left = (_sp == 'c') ? _pd / 2 : _pd; _line@f.scopeId@ += std::string(_left, ' '); }
            _line@f.scopeId@ += _r[_c];
        }
    }
    @if f.precededByLit@
    @f.sb@ += @f.precededByLit@;
    @end if@
    @f.sb@ += _line@f.scopeId@;
    @if f.sepLit@
    if (_i@f.scopeId@ + 1 < _rows@f.scopeId@.size()) @f.sb@ += @f.sepLit@;
    @if f.followedByLit@
    else if (!_rows@f.scopeId@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @else@
    @if f.followedByLit@
    if (_i@f.scopeId@ + 1 >= _rows@f.scopeId@.size() && !_rows@f.scopeId@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @end if@
}
@end if@
END

// ── If / Else ────────────────────────────────────────────────────────────────

template emit_if(i: IfData)
@if i.isBlock@
@emit_if_block(i)@
@else@
@emit_if_inline(i)@
@end if@
END

template emit_if_inline(i: IfData)
@if i.isNegated@
if (!@i.condPath@) {
@else@
if (@i.condPath@) {
@end if@
    @for instr in i.thenBody@
    @emit_instr(instr)@
    @end for@
@if i.elseBody@
} else {
    @for instr in i.elseBody@
    @emit_instr(instr)@
    @end for@
@end if@
}
END

template emit_if_block(i: IfData)
@if i.isNegated@
if (!@i.condPath@) {
@else@
if (@i.condPath@) {
@end if@
    std::string _blk@i.thenScopeId@;
    @for instr in i.thenBody@
    @emit_instr(instr)@
    @end for@
    @i.sb@ += _tppBlockIndent(_blk@i.thenScopeId@, @i.insertCol@);
@if i.elseBody@
} else {
    std::string _blk@i.elseScopeId@;
    @for instr in i.elseBody@
    @emit_instr(instr)@
    @end for@
    @i.sb@ += _tppBlockIndent(_blk@i.elseScopeId@, @i.insertCol@);
@end if@
}
END

// ── Switch / Case ────────────────────────────────────────────────────────────

template emit_switch_case_block(s: SwitchData, c: CaseData)
std::string _blk@c.scopeId@;
@if c.body@
@for instr in c.body@
@emit_instr(instr)@
@end for@
@end if@
@s.sb@ += _tppBlockIndent(_blk@c.scopeId@, @s.insertCol@);
END

template emit_switch(s: SwitchData)
switch ((@cpp_value_path(s.exprPath, s.exprIsRecursive, s.exprIsOptional)@).value.index()) {
@for c in s.cases@
    case @c.variantIndex@: { // @c.tag@
        @if c.bindingName@
        @if c.isRecursivePayload@
        [[maybe_unused]] const auto& @c.bindingName@ = *std::get<@c.variantIndex@>((@cpp_value_path(s.exprPath, s.exprIsRecursive, s.exprIsOptional)@).value);
        @else@
        [[maybe_unused]] const auto& @c.bindingName@ = std::get<@c.variantIndex@>((@cpp_value_path(s.exprPath, s.exprIsRecursive, s.exprIsOptional)@).value);
        @end if@
        @end if@
        @if s.isBlock@
        @emit_switch_case_block(s, c)@
        @else@
        @if c.body@
        @for instr in c.body@
        @emit_instr(instr)@
        @end for@
        @end if@
        @end if@
        break;
    }
@end for@
}
END

// ── Policy helpers ───────────────────────────────────────────────────────────

template emit_policy_instance_decl(pol: PolicyInfo)
static const TppPolicy @pol.identifier@;
END

template emit_policy_instance_def(pol: PolicyInfo)
inline const TppPolicy TppPolicy::@pol.identifier@ = [] {
    TppPolicy p;
    p.tag = @pol.tagLit@;
    @if pol.minVal@
    p.minLength = @pol.minVal@;
    @end if@
    @if pol.maxVal@
    p.maxLength = @pol.maxVal@;
    @end if@
    @if pol.rejectIfRegexLit@
    @if pol.rejectMsgLit@
    p.rejectIf = TppPolicy::RejectRule{std::regex(@pol.rejectIfRegexLit@), @pol.rejectMsgLit@};
    @end if@
    @end if@
    @if pol.require@
    p.require = { @for r in pol.require | sep=", "@{std::regex(@r.regexLit@), @if r.replaceLit@std::optional<std::string>{@r.replaceLit@}@else@std::nullopt@end if@}@end for@ };
    @end if@
    @if pol.replacements@
    p.replacements = { @for r in pol.replacements | sep=", "@{@r.findLit@, @r.replaceLit@}@end for@ };
    @end if@
    @if pol.outputFilter@
    p.outputFilter = { @for f in pol.outputFilter | sep=", "@std::regex(@f.regexLit@)@end for@ };
    @end if@
    return p;
}();
END

template emit_policies(ctx: RenderFunctionsInput)
struct TppPolicy {
    std::string tag;
    std::optional<int> minLength, maxLength;
    struct RejectRule { std::regex pattern; std::string message; };
    std::optional<RejectRule> rejectIf;
    struct RequireStep { std::regex pattern; std::optional<std::string> replace; };
    std::vector<RequireStep> require;
    std::vector<std::pair<std::string, std::string>> replacements;
    std::vector<std::regex> outputFilter;
    std::string apply(const std::string& value) const {
        std::string v = value;
        if (minLength && (int)v.size() < *minLength)
            throw std::runtime_error("[policy " + tag + "] value is below minimum length of " + std::to_string(*minLength));
        if (maxLength && (int)v.size() > *maxLength)
            throw std::runtime_error("[policy " + tag + "] value exceeds maximum length of " + std::to_string(*maxLength));
        if (rejectIf && std::regex_search(v, rejectIf->pattern))
            throw std::runtime_error("[policy " + tag + "] " + rejectIf->message);
        for (const auto& step : require) {
            if (!std::regex_search(v, step.pattern))
                throw std::runtime_error("[policy " + tag + "] value does not match required pattern");
            if (step.replace) v = std::regex_replace(v, step.pattern, *step.replace);
        }
        for (const auto& [find, repl] : replacements) {
            std::string::size_type pos = 0;
            while ((pos = v.find(find, pos)) != std::string::npos) {
                v.replace(pos, find.size(), repl);
                pos += repl.size();
            }
        }
        for (const auto& p : outputFilter) {
            if (!std::regex_match(v, p))
                throw std::runtime_error("[policy " + tag + "] output does not match required filter");
        }
        return v;
    }
@if ctx.policies@
@for pol in ctx.policies@
    @emit_policy_instance_decl(pol)@
@end for@
@end if@
    static const TppPolicy pure;
};
@if ctx.policies@
@for pol in ctx.policies@

@emit_policy_instance_def(pol)@
@end for@
@end if@

inline const TppPolicy TppPolicy::pure{};
END

// ── Runtime helpers (shared across generated files) ──────────────────────────

template emit_runtime_helpers(ctx: RenderFunctionsInput)
[[maybe_unused]] static void _tppAppendValue(std::string& sb, const std::string& value) {
    if (value.find('\n') == std::string::npos) { sb += value; return; }
    auto lastNl = sb.rfind('\n');
    int col = (lastNl == std::string::npos) ? (int)sb.size() : (int)(sb.size() - lastNl - 1);
    if (col <= 0) { sb += value; return; }
    std::string pad(col, ' ');
    size_t start = 0;
    while (true) {
        auto end = value.find('\n', start);
        if (start > 0) sb += pad;
        if (end == std::string::npos) { sb += value.substr(start); break; }
        sb.append(value, start, end - start);
        sb += '\n';
        start = end + 1;
    }
}

[[maybe_unused]] static std::string _tppBlockIndent(const std::string& raw, int insertCol) {
    if (raw.empty()) return "";
    std::vector<std::string> parts;
    size_t pos = 0;
    while (pos < raw.size()) {
        auto nl = raw.find('\n', pos);
        if (nl == std::string::npos) { parts.push_back(raw.substr(pos)); break; }
        parts.push_back(raw.substr(pos, nl - pos));
        pos = nl + 1;
        if (pos == raw.size()) { parts.push_back(""); }
    }
    bool trailingNl = !raw.empty() && raw.back() == '\n';
    int lineCount = trailingNl ? (int)parts.size() - 1 : (int)parts.size();
    std::string zeroMarker;
    for (int i = 0; i < lineCount; i++) {
        size_t ws = parts[i].find_first_not_of(" \t");
        if (ws != std::string::npos) {
            zeroMarker = parts[i].substr(0, ws);
            break;
        }
    }
    std::string indent(insertCol, ' ');
    std::string result;
    for (int i = 0; i < lineCount; i++) {
        std::string l = parts[i];
        if (!zeroMarker.empty() && l.substr(0, zeroMarker.size()) == zeroMarker)
            l = l.substr(zeroMarker.size());
        if (!l.empty()) result += indent + l;
        if (i + 1 < lineCount || trailingNl) result += '\n';
    }
    return result;
}

[[maybe_unused]] static std::string _tppPadCell(const std::string& s, int width, char spec) {
    if ((int)s.size() >= width) return s;
    int pad = width - (int)s.size();
    if (spec == 'r') return std::string(pad, ' ') + s;
    if (spec == 'c') { int left = pad / 2; return std::string(left, ' ') + s + std::string(pad - left, ' '); }
    return s + std::string(pad, ' ');
}
END

// ── Standalone runtime header ────────────────────────────────────────────────

template render_cpp_runtime(ctx: RenderFunctionsInput)
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <regex>
#include <stdexcept>
@if ctx.namespaceName@
namespace @ctx.namespaceName@ {
@end if@

@emit_runtime_helpers(ctx)@
@if ctx.policies@

@emit_policies(ctx)@
@end if@
@if ctx.namespaceName@
} // namespace @ctx.namespaceName@
@end if@
END

// ── Main entry point: native C++ rendering functions ─────────────────────────

template render_cpp_native_implementation(ctx: RenderFunctionsInput)
@if ctx.includes@
@for inc in ctx.includes@
#include "@inc@"
@end for@
@end if@
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <algorithm>
@if ctx.policies@
#include <regex>
#include <stdexcept>
@end if@
@if ctx.namespaceName@
namespace @ctx.namespaceName@ {
@end if@
@if not ctx.externalRuntime@

@emit_runtime_helpers(ctx)@
@if ctx.policies@

@emit_policies(ctx)@
@end if@
@end if@
@for fn in ctx.functions@
@if ctx.policies@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", " followedBy=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@const TppPolicy& _policy);
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@);
@else@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@);
@end if@
@end for@
@for fn in ctx.functions@

@if ctx.policies@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", " followedBy=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@const TppPolicy& _policy) {
@else@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@) {
@end if@
    std::string _sb;
    @for instr in fn.body@
    @emit_instr(instr)@
    @end for@
    if (!_sb.empty() && _sb.back() == '\n')
        _sb.pop_back();
    return _sb;
}
@if ctx.policies@

@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@) {
    return @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", " followedBy=", "@@param.name@@end for@TppPolicy::pure);
}
@end if@
@end for@
@if ctx.namespaceName@
} // namespace @ctx.namespaceName@
@end if@
END
