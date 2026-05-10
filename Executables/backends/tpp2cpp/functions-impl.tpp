// ═══════════════════════════════════════════════════════════════════════════════
// Instruction IR → C++ rendering functions (native code generation)
// ═══════════════════════════════════════════════════════════════════════════════
// ── Expression → C++ String expression ───────────────────────────────────────

template cpp_value_path(value: RenderValueRef)
@if value.isRecursive@
*@value.path@
@else@
@if value.isOptional@
*@value.path@
@else@
@value.path@
@end if@
@end if@
END

template cpp_optional_to_str(expr: RenderExprInfo, inner: RenderTypeKind)
@switch inner@
@case Str@
@if expr.ref.isRecursive@(@expr.ref.path@ ? *@expr.ref.path@ : std::string{})@else@(@expr.ref.path@.has_value() ? *@expr.ref.path@ : std::string{})@end if@
@end case@
@case Int@
@if expr.ref.isRecursive@(@expr.ref.path@ ? std::to_string(*@expr.ref.path@) : std::string{})@else@(@expr.ref.path@.has_value() ? std::to_string(*@expr.ref.path@) : std::string{})@end if@
@end case@
@case Bool@
@if expr.ref.isRecursive@(@expr.ref.path@ ? (*@expr.ref.path@ ? "true" : "false") : std::string{})@else@(@expr.ref.path@.has_value() ? (*@expr.ref.path@ ? "true" : "false") : std::string{})@end if@
@end case@
@case Named(n)@
@if expr.ref.isRecursive@(@expr.ref.path@ ? std::to_string(*@expr.ref.path@) : std::string{})@else@(@expr.ref.path@.has_value() ? std::to_string(*@expr.ref.path@) : std::string{})@end if@
@end case@
@case List(e)@
@if expr.ref.isRecursive@(@expr.ref.path@ ? std::to_string(*@expr.ref.path@) : std::string{})@else@(@expr.ref.path@.has_value() ? std::to_string(*@expr.ref.path@) : std::string{})@end if@
@end case@
@case Optional(i)@
@if expr.ref.isRecursive@(@expr.ref.path@ ? std::to_string(*@expr.ref.path@) : std::string{})@else@(@expr.ref.path@.has_value() ? std::to_string(*@expr.ref.path@) : std::string{})@end if@
@end case@
@end switch@
END

template cpp_expr_to_str(expr: RenderExprInfo)
@switch expr.type@
@case Str@
@cpp_value_path(expr.ref)@@end case@
@case Int@
std::to_string(@cpp_value_path(expr.ref)@)@end case@
@case Bool@
(@cpp_value_path(expr.ref)@ ? "true" : "false")@end case@
@case Named(n)@
std::to_string(@cpp_value_path(expr.ref)@)@end case@
@case List(e)@
std::to_string(@cpp_value_path(expr.ref)@)@end case@
@case Optional(inner)@
@cpp_optional_to_str(expr, inner)@@end case@
@end switch@
END

// ── Leaf instruction templates ───────────────────────────────────────────────

template emit_emit(e: EmitData)
{
    if (!_writer.emit(@e.textLit@))
        throw std::runtime_error("tpp render error: " + _writer.error());
}
END

template emit_emit_expr(e: EmitExprData)
{
@if e.staticPolicyId@
    if (!_writer.emitValue(@cpp_expr_to_str(e.expr)@, _tppPolicy_@e.staticPolicyId@))
        throw std::runtime_error("tpp render error: " + _writer.error());
@else@
@if e.useRuntimePolicy@
    if (!_writer.emitValue(@cpp_expr_to_str(e.expr)@, _policy))
        throw std::runtime_error("tpp render error: " + _writer.error());
@else@
    if (!_writer.emitValue(@cpp_expr_to_str(e.expr)@))
        throw std::runtime_error("tpp render error: " + _writer.error());
@end if@
@end if@
}
END

template emit_call(c: CallData)
@if c.policyArg@
if (!_writer.emit(@c.functionName@(@for arg in c.args | sep=", " followedBy=", "@@cpp_call_arg(arg)@@end for@@cpp_policy_ref(c.policyArg)@)))
    throw std::runtime_error("tpp render error: " + _writer.error());
@else@
if (!_writer.emit(@c.functionName@(@for arg in c.args | sep=", "@@cpp_call_arg(arg)@@end for@)))
    throw std::runtime_error("tpp render error: " + _writer.error());
@end if@
END

template cpp_call_arg(arg: RenderValueRef)
@cpp_value_path(arg)@
END

template cpp_policy_ref(ref: PolicyRef)
@switch ref@@case Named(tag)@_tppPolicy_@tag@@end case@@case Pure@_tppPolicyPure@end case@@case Runtime@_policy@end case@@end switch@
END

template cpp_inline_loop_options(f: ForData)
tpp::Writer::NativeLoopOptions{
    @if f.precededByLit@std::optional<std::string_view>{@f.precededByLit@}@else@std::nullopt@end if@,
    @if f.sepLit@std::optional<std::string_view>{@f.sepLit@}@else@std::nullopt@end if@,
    @if f.followedByLit@std::optional<std::string_view>{@f.followedByLit@}@else@std::nullopt@end if@
}
END

template cpp_block_loop_options(f: ForData)
tpp::Writer::NativeLoopOptions{
    @if f.precededByLit@std::optional<std::string_view>{@f.precededByLit@}@else@std::nullopt@end if@,
    @if f.sepLit@std::optional<std::string_view>{@f.sepLit@}@else@std::nullopt@end if@,
    @if f.followedByLit@std::optional<std::string_view>{@f.followedByLit@}@else@std::nullopt@end if@
}
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
@case BeginCapturedBlock(p)@
@if p.blockIndentInParentBlock@
if (!_writer.beginCapturedBlock(@p.blockIndentInParentBlock@)) {
@else@
if (!_writer.beginCapturedBlock()) {
@end if@
    throw std::runtime_error("tpp render error: " + _writer.error());
}
@end case@
@case EmitCapturedBlock@
if (!_writer.emitCapturedBlock()) {
    throw std::runtime_error("tpp render error: " + _writer.error());
}
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
@if f.capturesBody@
@emit_for_block(f)@
@else@
@emit_for_inline(f)@
@end if@
@end if@
END

template emit_for_inline(f: ForData)
@if f.body@
if (!_writer.emitForEach(
    @cpp_value_path(f.collection)@,
    @cpp_inline_loop_options(f)@,
        [&](const auto& @f.varName@, @if f.enumeratorName@int @f.enumeratorName@@else@int@end if@) {
            @for instr in f.body@
            @emit_instr(instr)@
            @end for@
        }))
    throw std::runtime_error("tpp render error: " + _writer.error());
@end if@
END

template emit_for_block(f: ForData)
@if f.body@
@if f.bodyBlockIndentInParentBlock@
if (!_writer.emitCapturedBlockForEach(
    @cpp_value_path(f.collection)@,
    @f.bodyBlockIndentInParentBlock@,
    @cpp_block_loop_options(f)@,
        [&](const auto& @f.varName@, @if f.enumeratorName@int @f.enumeratorName@@else@int@end if@) {
            @for instr in f.body@
            @emit_instr(instr)@
            @end for@
        }))
    throw std::runtime_error("tpp render error: " + _writer.error());
@else@
if (!_writer.emitCapturedBlockForEach(@cpp_value_path(f.collection)@,
                                     @cpp_block_loop_options(f)@,
                                    [&](const auto& @f.varName@, @if f.enumeratorName@int @f.enumeratorName@@else@int@end if@) {
                                        @for instr in f.body@
                                        @emit_instr(instr)@
                                        @end for@
                                    }))
{
    throw std::runtime_error("tpp render error: " + _writer.error());
}
@end if@
@end if@
END

// ── Aligned for ──────────────────────────────────────────────────────────────

template emit_align_cell(cell: AlignCellInfo)
{
    if (!_writer.captureAlignedCell(@cell.cellIndex@, [&]() {
            @for instr in cell.body@
            @emit_instr(instr)@
            @end for@
        }))
    {
        throw std::runtime_error("tpp render error: " + _writer.error());
    }
}
END

template emit_aligned_for(f: ForData)
@if f.cells@
if (!_writer.emitAlignedForEach(
    @cpp_value_path(f.collection)@,
        @f.numCols@,
        std::vector<char>{ @for ch in f.alignSpecChars | sep=", "@'@ch@'@end for@ },
        std::vector<char>{ @for ch in f.alignSpecChars | sep=", "@'@ch@'@end for@ }.size() == 1,
    @cpp_inline_loop_options(f)@,
        [&](const auto& @f.varName@, @if f.enumeratorName@int @f.enumeratorName@@else@int@end if@) {
            @for cell in f.cells@
            @emit_align_cell(cell)@
            @end for@
        }))
    throw std::runtime_error("tpp render error: " + _writer.error());
@end if@
END

// ── If / Else ────────────────────────────────────────────────────────────────

template emit_if(i: IfData)
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

// ── Switch / Case ────────────────────────────────────────────────────────────

template emit_switch(s: SwitchData)
switch ((@cpp_value_path(s.expr)@).value.index()) {
@for c in s.cases@
    case @c.variantIndex@: { // @c.tag@
        @if c.bindingName@
        @if c.isRecursivePayload@
    [[maybe_unused]] const auto& @c.bindingName@ = *std::get<@c.variantIndex@>((@cpp_value_path(s.expr)@).value);
        @else@
    [[maybe_unused]] const auto& @c.bindingName@ = std::get<@c.variantIndex@>((@cpp_value_path(s.expr)@).value);
        @end if@
        @end if@
        @if c.body@
        @for instr in c.body@
        @emit_instr(instr)@
        @end for@
        @end if@
        break;
    }
@end for@
}
END

// ── Policy helpers ───────────────────────────────────────────────────────────

template emit_policy_instance_def(pol: PolicyInfo)
inline const tpp::TppPolicy _tppPolicy_@pol.identifier@ = [] {
    tpp::TppPolicy p;
    p.tag = @pol.tagLit@;
    @if pol.minVal@
    p.minLength = @pol.minVal@;
    @end if@
    @if pol.maxVal@
    p.maxLength = @pol.maxVal@;
    @end if@
    @if pol.rejectIfRegexLit@
    @if pol.rejectMsgLit@
    p.rejectIf = tpp::TppPolicy::RejectRule{std::regex(@pol.rejectIfRegexLit@), @pol.rejectMsgLit@};
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
@if ctx.policies@
@for pol in ctx.policies@
@emit_policy_instance_def(pol)@
@end for@
@end if@

inline const tpp::TppPolicy _tppPolicyPure{};
END

// ── Main entry point: native C++ rendering functions ─────────────────────────

template render_cpp_native_implementation(ctx: RenderFunctionsInput)
@if ctx.includes@
@for inc in ctx.includes@
#include "@inc@"
@end for@
@end if@
#include <tpp/ArgType.h>
#include <tpp/Policy.h>
#include <tpp/Writer.h>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <regex>
#include <stdexcept>
@if ctx.namespaceName@
namespace @ctx.namespaceName@ {
@end if@

@if ctx.policies@

@emit_policies(ctx)@
@end if@
@if ctx.policies@
@for fn in ctx.functions@
static std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@, const tpp::TppPolicy& _policy);
@end for@

@end if@
@for fn in ctx.functions@

@if ctx.policies@
static std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@, const tpp::TppPolicy& _policy) {
    tpp::Writer _writer;
    @for instr in fn.body@
    @emit_instr(instr)@
    @end for@
    return _writer.takeOutput(tpp::Writer::OutputPostProcessing::StripSingleTrailingNewline);
}

@if ctx.needsStatic@static @end if@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@) {
    return @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@@param.name@@end for@, _tppPolicyPure);
}
@else@
@if ctx.needsStatic@static @end if@std::string @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@typename tpp::ArgType<@cpp_type(param.type)@>::type @param.name@@end for@) {
    tpp::Writer _writer;
    @for instr in fn.body@
    @emit_instr(instr)@
    @end for@
    return _writer.takeOutput(tpp::Writer::OutputPostProcessing::StripSingleTrailingNewline);
}
@end if@
@end for@
@if ctx.namespaceName@
} // namespace @ctx.namespaceName@
@end if@
END
