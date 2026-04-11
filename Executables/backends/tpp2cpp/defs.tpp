template render_templates(types: string, template: string)
#pragma once

constexpr char types_content[] = @types@;
constexpr char template_content[] =  @template@;
END

template render_cpp_types(input: CppTypesInput)
#pragma once
@for inc in input.includes@
#include "@inc@"
@end for@
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>
@if input.namespaceName@
namespace @input.namespaceName@ {
@end if@
template<typename _TppT>
inline nlohmann::json _tpp_j(const _TppT& v) { return nlohmann::json(v); }
template<typename _TppT>
inline nlohmann::json _tpp_j(const std::unique_ptr<_TppT>& v) { return v ? nlohmann::json(*v) : nlohmann::json(); }

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
struct @s.name@;
@end case@
@case Enum(e)@
struct @e.name@;
@end case@
@end switch@
@end for@

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
@if s.doc@
/// @s.doc@
@end if@
struct @s.name@
{
    @for field in s.fields@
    @if field.doc@
    /// @field.doc@
    @end if@
    @field.type@ @field.name@;
    @end for@
    static std::string tpp_typedefs() noexcept
    {
        return @s.definition@;
    }
};
@end case@
@case Enum(e)@
@if e.doc@
/// @e.doc@
@end if@
@for variant in e.variants@
@if not variant.payloadType@
@if variant.doc@
/// @variant.doc@
@end if@
struct @e.name@_@variant.tag@
{
    friend void to_json(nlohmann::json& j, const @e.name@_@variant.tag@&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, @e.name@_@variant.tag@&) {}
};
@end if@
@end for@
struct @e.name@
{
    using Value = std::variant<@for variant in e.variants | sep=", "@@if not variant.payloadType@@e.name@_@variant.tag@@else@@if variant.isRecursivePayload@std::unique_ptr<@variant.payloadType@>@else@@variant.payloadType@@end if@@end if@@end for@>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return @e.definition@;
    }
};
@end case@
@end switch@
@end for@

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
inline void from_json(const nlohmann::json& j, @s.name@& v);
inline void to_json(nlohmann::json& j, const @s.name@& v);
@end case@
@case Enum(e)@
inline void from_json(const nlohmann::json& j, @e.name@& v);
inline void to_json(nlohmann::json& j, const @e.name@& v);
@end case@
@end switch@
@end for@

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
inline void from_json(const nlohmann::json& j, @s.name@& v)
{
    @for field in s.fields@
    @if field.recursiveInnerType@
    @if field.isOptional@
    if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = std::make_unique<@field.recursiveInnerType@>(j.at("@field.name@").get<@field.recursiveInnerType@>());
    @else@
    v.@field.name@ = std::make_unique<@field.recursiveInnerType@>(j.at("@field.name@").get<@field.recursiveInnerType@>());
    @end if@
    @else@
    @if field.innerType@
    if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = j.at("@field.name@").get<@field.innerType@>();
    @else@
    j.at("@field.name@").get_to(v.@field.name@);
    @end if@
    @end if@
    @end for@
}
inline void to_json(nlohmann::json& j, const @s.name@& v)
{
    j = nlohmann::json{};
    @for field in s.fields@
    @if field.recursiveInnerType@
    @if field.isOptional@
    if (v.@field.name@) j["@field.name@"] = *v.@field.name@;
    @else@
    j["@field.name@"] = *v.@field.name@;
    @end if@
    @else@
    @if field.isOptional@
    if (v.@field.name@.has_value()) j["@field.name@"] = *v.@field.name@;
    @else@
    j["@field.name@"] = v.@field.name@;
    @end if@
    @end if@
    @end for@
}
@end case@
@case Enum(e)@
inline void from_json(const nlohmann::json& j, @e.name@& v)
{
    @for variant in e.variants | sep="\n    else "@
    if (j.contains("@variant.tag@")) @if not variant.payloadType@v.value.emplace<@variant.index@>();@else@@if variant.isRecursivePayload@v.value.emplace<@variant.index@>(std::make_unique<@variant.payloadType@>(j["@variant.tag@"].get<@variant.payloadType@>()));@else@v.value.emplace<@variant.index@>(j["@variant.tag@"].get<@variant.payloadType@>());@end if@@end if@
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
@end case@
@end switch@
@end for@
@if input.namespaceName@
} // namespace @input.namespaceName@
@end if@
END

template render_cpp_functions(input: CppFunctionsInput)
#pragma once
#include <tpp/ArgType.h>
@for inc in input.includes@
#include "@inc@"
@end for@
#include <string>
@if input.namespaceName@
namespace @input.namespaceName@ {
@end if@
@for function in input.functions@
@if function.doc@
/// @function.doc@
@end if@
std::string @input.functionPrefix@@function.name@(@for param in function.params | sep=", "@typename tpp::ArgType<@param.type@>::type @param.name@@end for@);
@end for@
@if input.namespaceName@
} // namespace @input.namespaceName@
@end if@
END

template render_cpp_implementation(input: CppFunctionsInput)
@for inc in input.includes@
#include "@inc@"
@end for@
#include <tpp/Compiler.h>
#include <nlohmann/json.hpp>
@if input.namespaceName@
namespace @input.namespaceName@ {
@end if@
static const tpp::IR& _getIR()
{
    static const tpp::IR co =
        nlohmann::json::parse(@input.iRepJson@).get<tpp::IR>();
    return co;
}
@for function in input.functions@
std::string @input.functionPrefix@@function.name@(@for param in function.params | sep=", "@typename tpp::ArgType<@param.type@>::type @param.name@@end for@)
{
    const auto& co = _getIR();
    tpp::FunctionSymbol fs;
    std::string error;
    if (!co.get_function("@function.name@", fs, error)) return {};
    nlohmann::json _tpp_input = nlohmann::json::array({@for param in function.params | sep=", "@nlohmann::json(@param.name@)@end for@});
    std::string output;
    if (!fs.render(_tpp_input, output, error)) return {};
    return output;
}
@end for@
@if input.namespaceName@
} // namespace @input.namespaceName@
@end if@
END

// ═══════════════════════════════════════════════════════════════════════════════
// Instruction IR → C++ rendering functions (native code generation)
// ═══════════════════════════════════════════════════════════════════════════════

// ── Expression → C++ String expression ───────────────────────────────────────

template cpp_optional_to_str(path: string, inner: TypeKind)
@switch inner@
@case Str@
(@path@.has_value() ? *@path@ : std::string{})@end case@
@case Int@
(@path@.has_value() ? std::to_string(*@path@) : std::string{})@end case@
@case Bool@
(@path@.has_value() ? (*@path@ ? "true" : "false") : std::string{})@end case@
@case Named(n)@
(@path@.has_value() ? std::to_string(*@path@) : std::string{})@end case@
@case List(e)@
(@path@.has_value() ? std::to_string(*@path@) : std::string{})@end case@
@case Optional(i)@
(@path@.has_value() ? std::to_string(*@path@) : std::string{})@end case@
@end switch@
END

template cpp_expr_to_str(expr: ExprInfo)
@switch expr.type@
@case Str@
@expr.path@@end case@
@case Int@
std::to_string(@expr.path@)@end case@
@case Bool@
(@expr.path@ ? "true" : "false")@end case@
@case Named(n)@
std::to_string(@expr.path@)@end case@
@case List(e)@
std::to_string(@expr.path@)@end case@
@case Optional(inner)@
@cpp_optional_to_str(expr.path, inner)@@end case@
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
@c.sb@ += @c.functionName@(@c.argsStr@);
END

// ── Recursive instruction dispatcher (block-style switch — avoids bug) ───────

template emit_instr(instr: Instruction)
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
@case RenderVia(r)@
@emit_render_via(r)@
@end case@
@end switch@
END

// ── For loop ─────────────────────────────────────────────────────────────────

template cpp_type(t: TypeKind)
@switch t@@case Str@std::string@end case@@case Int@int@end case@@case Bool@bool@end case@@case Named(n)@@n@@end case@@case List(e)@std::vector<@cpp_type(e)@>@end case@@case Optional(inner)@std::optional<@cpp_type(inner)@>@end case@@end switch@
END

template emit_for(f: ForData)
@if f.hasAlign@
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
for (size_t _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collPath@.size(); _i@f.scopeId@++) {
    [[maybe_unused]] const auto& @f.varName@ = @f.collPath@[_i@f.scopeId@];
    @if f.hasEnum@
    int @f.enumeratorName@ = (int)_i@f.scopeId@;
    @end if@
    @if f.hasPreceded@
    @f.sb@ += @f.precededByLit@;
    @end if@
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    @if f.hasSep@
    if (_i@f.scopeId@ + 1 < @f.collPath@.size()) @f.sb@ += @f.sepLit@;
    @if f.hasFollowed@
    else if (!@f.collPath@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @else@
    @if f.hasFollowed@
    if (_i@f.scopeId@ + 1 >= @f.collPath@.size() && !@f.collPath@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @end if@
}
END

template emit_for_block_sep(f: ForData)
bool _stripped@f.scopeId@ = false;
if (!_iter@f.scopeId@.empty() && _iter@f.scopeId@.back() == '\n') {
    if (std::count(_iter@f.scopeId@.begin(), _iter@f.scopeId@.end(), '\n') == 1) { _iter@f.scopeId@.pop_back(); _stripped@f.scopeId@ = true; }
}
@if f.hasPreceded@
@f.sb@ += @f.precededByLit@;
@end if@
@f.sb@ += _iter@f.scopeId@;
if (_i@f.scopeId@ + 1 < @f.collPath@.size()) {
    @f.sb@ += @f.sepLit@;
} else {
    @if f.hasFollowed@
    if (!@f.collPath@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    if (_stripped@f.scopeId@) @f.sb@ += "\n";
}
END

template emit_for_block(f: ForData)
for (size_t _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collPath@.size(); _i@f.scopeId@++) {
    [[maybe_unused]] const auto& @f.varName@ = @f.collPath@[_i@f.scopeId@];
    @if f.hasEnum@
    int @f.enumeratorName@ = (int)_i@f.scopeId@;
    @end if@
    std::string _blk@f.scopeId@;
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    std::string _iter@f.scopeId@ = _tppBlockIndent(_blk@f.scopeId@, @f.insertCol@);
    @if f.hasSep@
    @emit_for_block_sep(f)@
    @else@
    @if f.hasPreceded@
    @f.sb@ += @f.precededByLit@;
    @end if@
    @f.sb@ += _iter@f.scopeId@;
    @if f.hasFollowed@
    if (_i@f.scopeId@ + 1 >= @f.collPath@.size() && !@f.collPath@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @end if@
}
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
std::vector<std::vector<std::string>> _rows@f.scopeId@;
for (size_t _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collPath@.size(); _i@f.scopeId@++) {
    [[maybe_unused]] const auto& @f.varName@ = @f.collPath@[_i@f.scopeId@];
    @if f.hasEnum@
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
@for line in f.specSetupLines@
@line@
@end for@
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
    @if f.hasPreceded@
    @f.sb@ += @f.precededByLit@;
    @end if@
    @f.sb@ += _line@f.scopeId@;
    @if f.hasSep@
    if (_i@f.scopeId@ + 1 < _rows@f.scopeId@.size()) @f.sb@ += @f.sepLit@;
    @if f.hasFollowed@
    else if (!_rows@f.scopeId@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @else@
    @if f.hasFollowed@
    if (_i@f.scopeId@ + 1 >= _rows@f.scopeId@.size() && !_rows@f.scopeId@.empty()) @f.sb@ += @f.followedByLit@;
    @end if@
    @end if@
}
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
if (@i.condExprStr@) {
    @for instr in i.thenBody@
    @emit_instr(instr)@
    @end for@
@if i.hasElse@
} else {
    @for instr in i.elseBody@
    @emit_instr(instr)@
    @end for@
@end if@
}
END

template emit_if_block(i: IfData)
if (@i.condExprStr@) {
    std::string _blk@i.thenScopeId@;
    @for instr in i.thenBody@
    @emit_instr(instr)@
    @end for@
    @i.sb@ += _tppBlockIndent(_blk@i.thenScopeId@, @i.insertCol@);
@if i.hasElse@
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
@for instr in c.body@
@emit_instr(instr)@
@end for@
@s.sb@ += _tppBlockIndent(_blk@c.scopeId@, @s.insertCol@);
END

template emit_switch(s: SwitchData)
switch (@s.exprPath@.value.index()) {
@for c in s.cases@
    case @c.variantIndex@: { // @c.tag@
        @if c.hasBinding@
        @if c.isRecursivePayload@
        const auto& @c.bindingName@ = *std::get<@c.variantIndex@>(@s.exprPath@.value);
        @else@
        const auto& @c.bindingName@ = std::get<@c.variantIndex@>(@s.exprPath@.value);
        @end if@
        @end if@
        @if s.isBlock@
        @emit_switch_case_block(s, c)@
        @else@
        @for instr in c.body@
        @emit_instr(instr)@
        @end for@
        @end if@
        break;
    }
@end for@
}
END

// ── Render Via ───────────────────────────────────────────────────────────────

template emit_render_via_dispatch(r: RenderViaData)
std::string _res@r.scopeId@;
std::visit([&](const auto& _sv) {
@for ovl in r.overloads@
@if ovl.payloadType@
    if constexpr (std::is_same_v<std::decay_t<decltype(_sv)>, @cpp_type(ovl.payloadType)@>) {
        _res@r.scopeId@ = @r.functionName@(_sv@if r.policyArg@, @r.policyArg@@end if@);
    }
@end if@
@end for@
}, _item@r.scopeId@.value);
END

template emit_render_via(r: RenderViaData)
for (size_t _i@r.scopeId@ = 0; _i@r.scopeId@ < @r.collPath@.size(); _i@r.scopeId@++) {
    const auto& _item@r.scopeId@ = @r.collPath@[_i@r.scopeId@];
    @if r.isSingleOverload@
    std::string _res@r.scopeId@ = @r.functionName@(_item@r.scopeId@@if r.policyArg@, @r.policyArg@@end if@);
    @else@
    @emit_render_via_dispatch(r)@
    @end if@
    @if r.hasPreceded@
    @r.sb@ += @r.precededByLit@;
    @end if@
    @r.sb@ += _res@r.scopeId@;
    @if r.hasSep@
    if (_i@r.scopeId@ + 1 < @r.collPath@.size()) @r.sb@ += @r.sepLit@;
    @if r.hasFollowed@
    else if (!@r.collPath@.empty()) @r.sb@ += @r.followedByLit@;
    @end if@
    @else@
    @if r.hasFollowed@
    if (_i@r.scopeId@ + 1 >= @r.collPath@.size() && !@r.collPath@.empty()) @r.sb@ += @r.followedByLit@;
    @end if@
    @end if@
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
    @if pol.hasLength@
    p.minLength = @pol.minVal@;
    p.maxLength = @pol.maxVal@;
    @end if@
    @if pol.hasRejectIf@
    p.rejectIf = TppPolicy::RejectRule{std::regex(@pol.rejectIfRegexLit@), @pol.rejectMsgLit@};
    @end if@
    @if pol.hasRequire@
    p.require = { @for r in pol.require | sep=", "@{std::regex(@r.regexLit@), @if r.hasReplace@@r.replaceLit@@else@""@end if@, @if r.hasReplace@true@else@false@end if@}@end for@ };
    @end if@
    @if pol.hasReplacements@
    p.replacements = { @for r in pol.replacements | sep=", "@{@r.findLit@, @r.replaceLit@}@end for@ };
    @end if@
    @if pol.hasOutputFilter@
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
    struct RequireStep { std::regex pattern; std::string replace; bool hasReplace; };
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
            if (step.hasReplace) v = std::regex_replace(v, step.pattern, step.replace);
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
@for pol in ctx.policies@
    @emit_policy_instance_decl(pol)@
@end for@
    static const TppPolicy pure;
};
@for pol in ctx.policies@

@emit_policy_instance_def(pol)@
@end for@

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
@if ctx.hasPolicies@

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
@if ctx.hasPolicies@
#include <regex>
#include <stdexcept>
@end if@
@if ctx.namespaceName@
namespace @ctx.namespaceName@ {
@end if@
@if not ctx.externalRuntime@

@emit_runtime_helpers(ctx)@
@if ctx.hasPolicies@

@emit_policies(ctx)@
@end if@
@end if@
@for fn in ctx.functions@
@if ctx.hasPolicies@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@fn.paramsStrWithPolicy@);
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@fn.paramsStr@);
@else@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@fn.paramsStr@);
@end if@
@end for@
@for fn in ctx.functions@

@if ctx.hasPolicies@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@fn.paramsStrWithPolicy@) {
@else@
@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@fn.paramsStr@) {
@end if@
    std::string _sb;
    @for instr in fn.body@
    @emit_instr(instr)@
    @end for@
    if (!_sb.empty() && _sb.back() == '\n')
        _sb.pop_back();
    return _sb;
}
@if ctx.hasPolicies@

@ctx.staticModifier@std::string @ctx.functionPrefix@@fn.name@(@fn.paramsStr@) {
    return @ctx.functionPrefix@@fn.name@(@fn.argsPassStr@);
}
@end if@
@end for@
@if ctx.namespaceName@
} // namespace @ctx.namespaceName@
@end if@
END
