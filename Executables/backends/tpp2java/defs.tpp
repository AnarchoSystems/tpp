// ── Meta-template: embeds types + template content as constexpr strings ──

template render_templates(types: string, template: string)
#pragma once

constexpr char types_content[] = @types@;
constexpr char template_content[] = @template@;
END

// ── Type name helpers (inline — used in expressions) ─────────────────────

template java_type(t: TypeKind)
@switch t@@case Str@String@end case@@case Int@int@end case@@case Bool@boolean@end case@@case Named(name)@@name@@end case@@case List(elemType)@java.util.List<@java_boxed_type(elemType)@>@end case@@case Optional(innerType)@@java_type(innerType)@@end case@@end switch@
END

template java_boxed_type(t: TypeKind)
@switch t@@case Str@String@end case@@case Int@Integer@end case@@case Bool@Boolean@end case@@case Named(name)@@name@@end case@@case List(elemType)@java.util.List<@java_boxed_type(elemType)@>@end case@@case Optional(innerType)@@java_boxed_type(innerType)@@end case@@end switch@
END

// ── Array element getters (inline — used in expressions) ─────────────────

template java_arr_elem(t: TypeKind)
@switch t@@case Str@_arr.getString(_i)@end case@@case Int@_arr.getInt(_i)@end case@@case Bool@_arr.getBoolean(_i)@end case@@case Named(name)@@name@.fromJson(_arr.getJSONObject(_i))@end case@@case List(inner)@null@end case@@case Optional(inner)@null@end case@@end switch@
END

template java_inner_arr_elem(t: TypeKind)
@switch t@@case Str@_inner.getString(_j)@end case@@case Int@_inner.getInt(_j)@end case@@case Bool@_inner.getBoolean(_j)@end case@@case Named(name)@@name@.fromJson(_inner.getJSONObject(_j))@end case@@case List(inner)@null@end case@@case Optional(inner)@null@end case@@end switch@
END

// ── List loop body (block-style — called on its own line) ────────────────

template java_list_body(name: string, elemType: TypeKind)
@switch elemType@
@case Str@
v.@name@.add(_arr.getString(_i));
@end case@
@case Int@
v.@name@.add(_arr.getInt(_i));
@end case@
@case Bool@
v.@name@.add(_arr.getBoolean(_i));
@end case@
@case Named(n)@
v.@name@.add(@n@.fromJson(_arr.getJSONObject(_i)));
@end case@
@case List(inner)@
org.json.JSONArray _inner = _arr.getJSONArray(_i);
java.util.List<@java_boxed_type(inner)@> _row = new java.util.ArrayList<>();
for (int _j = 0; _j < _inner.length(); _j++) {
    _row.add(@java_inner_arr_elem(inner)@);
}
v.@name@.add(_row);
@end case@
@case Optional(inner)@
v.@name@.add(null);
@end case@
@end switch@
END

// ── Field fromJson / toJson helpers (block-style) ────────────────────────

template java_read_field(name: string, type: TypeKind)
@switch type@
@case Str@
v.@name@ = j.getString("@name@");
@end case@
@case Int@
v.@name@ = j.getInt("@name@");
@end case@
@case Bool@
v.@name@ = j.getBoolean("@name@");
@end case@
@case Named(typeName)@
v.@name@ = @typeName@.fromJson(j.getJSONObject("@name@"));
@end case@
@case List(elemType)@
{
    org.json.JSONArray _arr = j.getJSONArray("@name@");
    v.@name@ = new java.util.ArrayList<>();
    for (int _i = 0; _i < _arr.length(); _i++) {
        @java_list_body(name, elemType)@
    }
}
@end case@
@case Optional(inner)@
if (j.has("@name@") && !j.isNull("@name@")) {
    @java_read_field(name, inner)@
}
@end case@
@end switch@
END

template java_write_field(name: string, type: TypeKind)
@switch type@
@case Str@
j.put("@name@", @name@);
@end case@
@case Int@
j.put("@name@", @name@);
@end case@
@case Bool@
j.put("@name@", @name@);
@end case@
@case Named(typeName)@
j.put("@name@", @name@.toJson());
@end case@
@case List(elemType)@
j.put("@name@", @name@);
@end case@
@case Optional(inner)@
if (@name@ != null) j.put("@name@", @name@);
@end case@
@end switch@
END

// ── Variant fromJson helpers (block-style) ───────────────────────────────

template java_variant_list_add(elemType: TypeKind)
@switch elemType@
@case Str@
_list.add(_arr.getString(_i));
@end case@
@case Int@
_list.add(_arr.getInt(_i));
@end case@
@case Bool@
_list.add(_arr.getBoolean(_i));
@end case@
@case Named(n)@
_list.add(@n@.fromJson(_arr.getJSONObject(_i)));
@end case@
@case List(inner)@
{
    org.json.JSONArray _inner = _arr.getJSONArray(_i);
    java.util.ArrayList<@java_boxed_type(inner)@> _row = new java.util.ArrayList<>();
    for (int _j = 0; _j < _inner.length(); _j++) {
        _row.add(@java_inner_arr_elem(inner)@);
    }
    _list.add(_row);
}
@end case@
@case Optional(inner)@
_list.add(null);
@end case@
@end switch@
END

template java_variant_from_json(v: VariantInfo, enumName: string)
@if v.payload@
@switch v.payload@
@case Str@
return new @enumName@("@v.tag@", j.getString("@v.tag@"));
@end case@
@case Int@
return new @enumName@("@v.tag@", j.getInt("@v.tag@"));
@end case@
@case Bool@
return new @enumName@("@v.tag@", j.getBoolean("@v.tag@"));
@end case@
@case Named(typeName)@
return new @enumName@("@v.tag@", @typeName@.fromJson(j.getJSONObject("@v.tag@")));
@end case@
@case List(elemType)@
{
    org.json.JSONArray _arr = j.getJSONArray("@v.tag@");
    java.util.ArrayList<@java_boxed_type(elemType)@> _list = new java.util.ArrayList<>();
    for (int _i = 0; _i < _arr.length(); _i++) {
        @java_variant_list_add(elemType)@
    }
    return new @enumName@("@v.tag@", _list);
}
@end case@
@case Optional(inner)@
return new @enumName@("@v.tag@", null);
@end case@
@end switch@
@end if@
END

// ── Main codegen template ────────────────────────────────────────────────

template render_java_source(input: CodegenInput)
// Generated by tpp2java — do not edit.
@for e in input.enums@

class @e.name@ {
    public String _tag;
    public Object _payload;

    @e.name@(String tag, Object payload) { _tag = tag; _payload = payload; }
    @for v in e.variants@
    public boolean is@v.tag@() { return "@v.tag@".equals(_tag); }
    @end for@
    @for v in e.variants@
    @if v.payload@
    \@SuppressWarnings("unchecked")
    public @java_type(v.payload)@ get@v.tag@() { return (@java_boxed_type(v.payload)@) _payload; }
    @end if@
    @end for@

    public static @e.name@ fromJson(org.json.JSONObject j) {
        @for v in e.variants@
        if (j.has("@v.tag@")) {
            @if v.payload@
            @java_variant_from_json(v, e.name)@
            @else@
            return new @e.name@("@v.tag@", null);
            @end if@
        }
        @end for@
        throw new RuntimeException("Unknown variant of @e.name@");
    }

    public org.json.JSONObject toJson() {
        org.json.JSONObject j = new org.json.JSONObject();
        j.put(_tag, _payload != null ? _payload : org.json.JSONObject.NULL);
        return j;
    }
}
@end for@
@for s in input.structs@

class @s.name@ {
    @for field in s.fields@
    public @java_type(field.type)@ @field.name@;
    @end for@

    public static @s.name@ fromJson(org.json.JSONObject j) {
        @s.name@ v = new @s.name@();
        @for field in s.fields@
        @java_read_field(field.name, field.type)@
        @end for@
        return v;
    }

    public org.json.JSONObject toJson() {
        org.json.JSONObject j = new org.json.JSONObject();
        @for field in s.fields@
        @java_write_field(field.name, field.type)@
        @end for@
        return j;
    }
}
@end for@

END

// ═══════════════════════════════════════════════════════════════════════════════
// Instruction IR → Java rendering functions
// ═══════════════════════════════════════════════════════════════════════════════

// ── Expression → Java String expression ──────────────────────────────────────

template java_optional_to_str(path: string, inner: TypeKind)
@switch inner@@case Str@(@path@ != null ? @path@ : "")@end case@@case Int@(@path@ != null ? Integer.toString(@path@) : "")@end case@@case Bool@(@path@ != null ? Boolean.toString(@path@) : "")@end case@@case Named(n)@(@path@ != null ? String.valueOf(@path@) : "")@end case@@case List(e)@(@path@ != null ? String.valueOf(@path@) : "")@end case@@case Optional(i)@(@path@ != null ? String.valueOf(@path@) : "")@end case@@end switch@
END

template java_expr_to_str(expr: ExprInfo)
@switch expr.type@@case Str@@expr.path@@end case@@case Int@Integer.toString(@expr.path@)@end case@@case Bool@Boolean.toString(@expr.path@)@end case@@case Named(n)@String.valueOf(@expr.path@)@end case@@case List(e)@String.valueOf(@expr.path@)@end case@@case Optional(inner)@@java_optional_to_str(expr.path, inner)@@end case@@end switch@
END

// ── Leaf instruction templates ───────────────────────────────────────────────

template emit_emit(e: EmitData)
@e.sb@.append(@e.textLit@);
END

template emit_emit_expr(e: EmitExprData)
@if e.staticPolicyId@
{ String _pv = @java_expr_to_str(e.expr)@; _pv = TppPolicy.@e.staticPolicyId@.apply(_pv); _tppAppendValue(@e.sb@, _pv); }
@else@
@if e.useRuntimePolicy@
{ String _pv = @java_expr_to_str(e.expr)@; _pv = _policy.apply(_pv); _tppAppendValue(@e.sb@, _pv); }
@else@
_tppAppendValue(@e.sb@, @java_expr_to_str(e.expr)@);
@end if@
@end if@
END

template emit_call(c: CallData)
@if c.policyArg@
@c.sb@.append(@c.functionName@(@for arg in c.args | sep=", " followedBy=", "@@arg.path@@end for@@java_policy_ref(c.policyArg)@));
@else@
@c.sb@.append(@c.functionName@(@for arg in c.args | sep=", "@@arg.path@@end for@));
@end if@
END

template java_policy_ref(ref: PolicyRef)
@switch ref@@case Named(tag)@TppPolicy.@tag@@end case@@case Pure@TppPolicy.pure@end case@@case Runtime@_policy@end case@@end switch@
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
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collPath@.size(); _i@f.scopeId@++) {
    @java_type(f.elemType)@ @f.varName@ = @f.collPath@.get(_i@f.scopeId@);
    @if f.hasEnum@
    int @f.enumeratorName@ = _i@f.scopeId@;
    @end if@
    @if f.hasPreceded@
    @f.sb@.append(@f.precededByLit@);
    @end if@
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    @if f.hasSep@
    if (_i@f.scopeId@ + 1 < @f.collPath@.size()) @f.sb@.append(@f.sepLit@);
    @if f.hasFollowed@
    else if (!@f.collPath@.isEmpty()) @f.sb@.append(@f.followedByLit@);
    @end if@
    @else@
    @if f.hasFollowed@
    if (_i@f.scopeId@ + 1 >= @f.collPath@.size() && !@f.collPath@.isEmpty()) @f.sb@.append(@f.followedByLit@);
    @end if@
    @end if@
}
END

template emit_for_block_sep(f: ForData)
boolean _stripped@f.scopeId@ = false;
if (!_iter@f.scopeId@.isEmpty() && _iter@f.scopeId@.charAt(_iter@f.scopeId@.length()-1) == '\n') {
    long _nlCount = _iter@f.scopeId@.chars().filter(c -> c == '\n').count();
    if (_nlCount == 1) { _iter@f.scopeId@ = _iter@f.scopeId@.substring(0, _iter@f.scopeId@.length()-1); _stripped@f.scopeId@ = true; }
}
@if f.hasPreceded@
@f.sb@.append(@f.precededByLit@);
@end if@
@f.sb@.append(_iter@f.scopeId@);
if (_i@f.scopeId@ + 1 < @f.collPath@.size()) {
    @f.sb@.append(@f.sepLit@);
} else {
    @if f.hasFollowed@
    if (!@f.collPath@.isEmpty()) @f.sb@.append(@f.followedByLit@);
    @end if@
    if (_stripped@f.scopeId@) @f.sb@.append("\n");
}
END

template emit_for_block(f: ForData)
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collPath@.size(); _i@f.scopeId@++) {
    @java_type(f.elemType)@ @f.varName@ = @f.collPath@.get(_i@f.scopeId@);
    @if f.hasEnum@
    int @f.enumeratorName@ = _i@f.scopeId@;
    @end if@
    StringBuilder _blk@f.scopeId@ = new StringBuilder();
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    String _iter@f.scopeId@ = _tppBlockIndent(_blk@f.scopeId@.toString(), @f.insertCol@);
    @if f.hasSep@
    @emit_for_block_sep(f)@
    @else@
    @if f.hasPreceded@
    @f.sb@.append(@f.precededByLit@);
    @end if@
    @f.sb@.append(_iter@f.scopeId@);
    @if f.hasFollowed@
    if (_i@f.scopeId@ + 1 >= @f.collPath@.size() && !@f.collPath@.isEmpty()) @f.sb@.append(@f.followedByLit@);
    @end if@
    @end if@
}
END

// ── Aligned for ──────────────────────────────────────────────────────────────

template emit_align_cell(scopeId: int, cell: AlignCellInfo)
{
    StringBuilder _cell@scopeId@_@cell.cellIndex@ = new StringBuilder();
    @for instr in cell.body@
    @emit_instr(instr)@
    @end for@
    _row@scopeId@[@cell.cellIndex@] = _cell@scopeId@_@cell.cellIndex@.toString();
}
END

template emit_aligned_for(f: ForData)
java.util.List<String[]> _rows@f.scopeId@ = new java.util.ArrayList<>();
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collPath@.size(); _i@f.scopeId@++) {
    @java_type(f.elemType)@ @f.varName@ = @f.collPath@.get(_i@f.scopeId@);
    @if f.hasEnum@
    int @f.enumeratorName@ = _i@f.scopeId@;
    @end if@
    String[] _row@f.scopeId@ = new String[@f.numCols@];
    @for cell in f.cells@
    @emit_align_cell(f.scopeId, cell)@
    @end for@
    _rows@f.scopeId@.add(_row@f.scopeId@);
}
int[] _cw@f.scopeId@ = new int[@f.numCols@];
for (String[] _r : _rows@f.scopeId@) for (int _c = 0; _c < _r.length; _c++) _cw@f.scopeId@[_c] = Math.max(_cw@f.scopeId@[_c], _r[_c].length());
char[] _spec@f.scopeId@ = new char[@f.numCols@];
java.util.Arrays.fill(_spec@f.scopeId@, 'l');
@if f.singleAlignChar@
@for ch in f.alignSpecChars@
java.util.Arrays.fill(_spec@f.scopeId@, '@ch@');
@end for@
@else@
@for ch in f.alignSpecChars | enumerator=ci@
_spec@f.scopeId@[@ci@] = '@ch@';
@end for@
@end if@
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < _rows@f.scopeId@.size(); _i@f.scopeId@++) {
    String[] _r = _rows@f.scopeId@.get(_i@f.scopeId@);
    StringBuilder _line@f.scopeId@ = new StringBuilder();
    for (int _c = 0; _c < _r.length; _c++) {
        if (_c + 1 < @f.numCols@) {
            _line@f.scopeId@.append(_tppPadCell(_r[_c], _cw@f.scopeId@[_c], _spec@f.scopeId@[_c]));
        } else {
            char _sp = _spec@f.scopeId@[_c]; int _pd = _cw@f.scopeId@[_c] - _r[_c].length();
            if (_pd > 0 && _sp != 'l') { int _left = (_sp == 'c') ? _pd / 2 : _pd; _line@f.scopeId@.append(" ".repeat(_left)); }
            _line@f.scopeId@.append(_r[_c]);
        }
    }
    @if f.hasPreceded@
    @f.sb@.append(@f.precededByLit@);
    @end if@
    @f.sb@.append(_line@f.scopeId@);
    @if f.hasSep@
    if (_i@f.scopeId@ + 1 < _rows@f.scopeId@.size()) @f.sb@.append(@f.sepLit@);
    @if f.hasFollowed@
    else if (!_rows@f.scopeId@.isEmpty()) @f.sb@.append(@f.followedByLit@);
    @end if@
    @else@
    @if f.hasFollowed@
    if (_i@f.scopeId@ + 1 >= _rows@f.scopeId@.size() && !_rows@f.scopeId@.isEmpty()) @f.sb@.append(@f.followedByLit@);
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
@if i.condIsBool@
@if i.isNegated@
if (!@i.condPath@) {
@else@
if (@i.condPath@) {
@end if@
@else@
@if i.isNegated@
if (@i.condPath@ == null) {
@else@
if (@i.condPath@ != null) {
@end if@
@end if@
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
@if i.condIsBool@
@if i.isNegated@
if (!@i.condPath@) {
@else@
if (@i.condPath@) {
@end if@
@else@
@if i.isNegated@
if (@i.condPath@ == null) {
@else@
if (@i.condPath@ != null) {
@end if@
@end if@
    StringBuilder _blk@i.thenScopeId@ = new StringBuilder();
    @for instr in i.thenBody@
    @emit_instr(instr)@
    @end for@
    @i.sb@.append(_tppBlockIndent(_blk@i.thenScopeId@.toString(), @i.insertCol@));
@if i.hasElse@
} else {
    StringBuilder _blk@i.elseScopeId@ = new StringBuilder();
    @for instr in i.elseBody@
    @emit_instr(instr)@
    @end for@
    @i.sb@.append(_tppBlockIndent(_blk@i.elseScopeId@.toString(), @i.insertCol@));
@end if@
}
END

// ── Switch / Case ────────────────────────────────────────────────────────────

template emit_switch_case_block(s: SwitchData, c: CaseData)
StringBuilder _blk@c.scopeId@ = new StringBuilder();
@for instr in c.body@
@emit_instr(instr)@
@end for@
@s.sb@.append(_tppBlockIndent(_blk@c.scopeId@.toString(), @s.insertCol@));
END

template emit_switch(s: SwitchData)
@for c in s.cases@
@if c.isFirst@
if (@s.exprPath@._tag.equals(@c.tagLit@)) {
@else@
} else if (@s.exprPath@._tag.equals(@c.tagLit@)) {
@end if@
    @if c.payloadType@
    @java_type(c.payloadType)@ @c.bindingName@ = @s.exprPath@.get@c.tag@();
    @end if@
    @if s.isBlock@
    @emit_switch_case_block(s, c)@
    @else@
    @for instr in c.body@
    @emit_instr(instr)@
    @end for@
    @end if@
@end for@
}
END

// ── Render Via ───────────────────────────────────────────────────────────────

template emit_render_via_dispatch(r: RenderViaData)
String _res@r.scopeId@ = "";
@for ovl in r.overloads@
@if ovl.isFirst@
@if r.policyArg@
if (_item@r.scopeId@._tag.equals(@ovl.tagLit@)) {
    _res@r.scopeId@ = @r.functionName@(_item@r.scopeId@.get@ovl.tag@(), @java_policy_ref(r.policyArg)@);
@else@
if (_item@r.scopeId@._tag.equals(@ovl.tagLit@)) {
    _res@r.scopeId@ = @r.functionName@(_item@r.scopeId@.get@ovl.tag@());
@end if@
@else@
@if r.policyArg@
} else if (_item@r.scopeId@._tag.equals(@ovl.tagLit@)) {
    _res@r.scopeId@ = @r.functionName@(_item@r.scopeId@.get@ovl.tag@(), @java_policy_ref(r.policyArg)@);
@else@
} else if (_item@r.scopeId@._tag.equals(@ovl.tagLit@)) {
    _res@r.scopeId@ = @r.functionName@(_item@r.scopeId@.get@ovl.tag@());
@end if@
@end if@
@end for@
}
END

template emit_render_via(r: RenderViaData)
for (int _i@r.scopeId@ = 0; _i@r.scopeId@ < @r.collPath@.size(); _i@r.scopeId@++) {
    @java_type(r.elemType)@ _item@r.scopeId@ = @r.collPath@.get(_i@r.scopeId@);
    @if r.isSingleOverload@
@if r.policyArg@
    String _res@r.scopeId@ = @r.functionName@(_item@r.scopeId@, @java_policy_ref(r.policyArg)@);
@else@
    String _res@r.scopeId@ = @r.functionName@(_item@r.scopeId@);
@end if@
    @else@
    @emit_render_via_dispatch(r)@
    @end if@
    @if r.hasPreceded@
    @r.sb@.append(@r.precededByLit@);
    @end if@
    @r.sb@.append(_res@r.scopeId@);
    @if r.hasSep@
    if (_i@r.scopeId@ + 1 < @r.collPath@.size()) @r.sb@.append(@r.sepLit@);
    @if r.hasFollowed@
    else if (!@r.collPath@.isEmpty()) @r.sb@.append(@r.followedByLit@);
    @end if@
    @else@
    @if r.hasFollowed@
    if (_i@r.scopeId@ + 1 >= @r.collPath@.size() && !@r.collPath@.isEmpty()) @r.sb@.append(@r.followedByLit@);
    @end if@
    @end if@
}
END

// ── Policy helpers ───────────────────────────────────────────────────────────

template emit_policies(ctx: RenderFunctionsInput)
static class TppPolicy {
    final String tag;
    Integer minLength, maxLength;
    static class RejectRule { java.util.regex.Pattern pattern; String message; }
    RejectRule rejectIf;
    static class RequireStep { java.util.regex.Pattern pattern; String replace; }
    java.util.List<RequireStep> require = new java.util.ArrayList<>();
    String[][] replacements;
    java.util.regex.Pattern[] outputFilter;
    TppPolicy(String tag) { this.tag = tag; }
    String apply(String value) throws Exception {
        String v = value;
        if (minLength != null && v.length() < minLength)
            throw new Exception("[policy " + tag + "] value is below minimum length of " + minLength);
        if (maxLength != null && v.length() > maxLength)
            throw new Exception("[policy " + tag + "] value exceeds maximum length of " + maxLength);
        if (rejectIf != null && rejectIf.pattern.matcher(v).find())
            throw new Exception("[policy " + tag + "] " + rejectIf.message);
        for (RequireStep step : require) {
            if (!step.pattern.matcher(v).find())
                throw new Exception("[policy " + tag + "] value does not match required pattern");
            if (step.replace != null)
                v = step.pattern.matcher(v).replaceAll(step.replace);
        }
        if (replacements != null) {
            for (String[] r : replacements)
                v = v.replace(r[0], r[1]);
        }
        if (outputFilter != null) {
            for (java.util.regex.Pattern p : outputFilter) {
                if (!p.matcher(v).matches())
                    throw new Exception("[policy " + tag + "] output does not match required filter");
            }
        }
        return v;
    }
@for pol in ctx.policies@

    static final TppPolicy @pol.identifier@ = new TppPolicy(@pol.tagLit@);
    static {
        @if pol.hasLength@
        @pol.identifier@.minLength = @pol.minVal@;
        @pol.identifier@.maxLength = @pol.maxVal@;
        @end if@
        @if pol.hasRejectIf@
        { TppPolicy.RejectRule rr = new TppPolicy.RejectRule(); rr.pattern = java.util.regex.Pattern.compile(@pol.rejectIfRegexLit@); rr.message = @pol.rejectMsgLit@; @pol.identifier@.rejectIf = rr; }
        @end if@
        @if pol.hasRequire@
        @for r in pol.require@
        { TppPolicy.RequireStep rs = new TppPolicy.RequireStep(); rs.pattern = java.util.regex.Pattern.compile(@r.regexLit@); @if r.hasReplace@rs.replace = @r.replaceLit@; @end if@@pol.identifier@.require.add(rs); }
        @end for@
        @end if@
        @if pol.hasReplacements@
        @pol.identifier@.replacements = new String[][]{@for r in pol.replacements | sep=", "@{@r.findLit@, @r.replaceLit@}@end for@};
        @end if@
        @if pol.hasOutputFilter@
        @pol.identifier@.outputFilter = new java.util.regex.Pattern[]{@for f in pol.outputFilter | sep=", "@java.util.regex.Pattern.compile(@f.regexLit@)@end for@};
        @end if@
    }
@end for@

    static final TppPolicy pure = new TppPolicy("");
}
END

// ── Runtime helpers (shared across generated files) ──────────────────────────

template emit_runtime_helpers(ctx: RenderFunctionsInput)
static void _tppAppendValue(StringBuilder sb, String value) {
    if (value.indexOf('\n') < 0) { sb.append(value); return; }
    int lastNl = -1;
    for (int i = sb.length() - 1; i >= 0; i--) { if (sb.charAt(i) == '\n') { lastNl = i; break; } }
    int col = (lastNl < 0) ? sb.length() : sb.length() - lastNl - 1;
    if (col <= 0) { sb.append(value); return; }
    String pad = " ".repeat(col);
    int start = 0;
    while (true) {
        int end = value.indexOf('\n', start);
        if (start > 0) sb.append(pad);
        if (end < 0) { sb.append(value.substring(start)); break; }
        sb.append(value, start, end);
        sb.append('\n');
        start = end + 1;
    }
}
static String _tppBlockIndent(String raw, int insertCol) {
    if (raw.isEmpty()) return "";
    String[] parts = raw.split("\n", -1);
    boolean trailingNl = raw.endsWith("\n");
    int lineCount = trailingNl ? parts.length - 1 : parts.length;
    String zeroMarker = "";
    for (int i = 0; i < lineCount; i++) {
        if (!parts[i].trim().isEmpty()) {
            int ws = 0;
            while (ws < parts[i].length() && (parts[i].charAt(ws) == ' ' || parts[i].charAt(ws) == '\t')) ws++;
            zeroMarker = parts[i].substring(0, ws);
            break;
        }
    }
    String indent = insertCol > 0 ? " ".repeat(insertCol) : "";
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < lineCount; i++) {
        String l = parts[i];
        if (!zeroMarker.isEmpty() && l.startsWith(zeroMarker)) l = l.substring(zeroMarker.length());
        if (!l.isEmpty()) result.append(indent).append(l);
        if (i + 1 < lineCount || trailingNl) result.append('\n');
    }
    return result.toString();
}
static String _tppPadCell(String s, int width, char spec) {
    if (s.length() >= width) return s;
    int pad = width - s.length();
    if (spec == 'r') return " ".repeat(pad) + s;
    if (spec == 'c') { int left = pad / 2; return " ".repeat(left) + s + " ".repeat(pad - left); }
    return s + " ".repeat(pad);
}
END

// ── Standalone runtime class ─────────────────────────────────────────────────

template render_java_runtime(ctx: RenderFunctionsInput)
// Generated by tpp2java — runtime helpers.

class TppRuntime {
    @emit_runtime_helpers(ctx)@
@if ctx.hasPolicies@
    @emit_policies(ctx)@
@end if@
}
END

// ── Main entry point: Functions class ────────────────────────────────────────

template render_java_functions(ctx: RenderFunctionsInput)

class @if ctx.namespaceName@@ctx.namespaceName@@else@Functions@end if@@if ctx.externalRuntime@ extends TppRuntime@end if@ {
@if not ctx.externalRuntime@
    @emit_runtime_helpers(ctx)@
@if ctx.hasPolicies@
    @emit_policies(ctx)@
@end if@
@end if@
@for fn in ctx.functions@

@if ctx.hasPolicies@
    static String @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@@java_type(param.type)@ @param.name@@end for@) throws Exception { return @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", " followedBy=", "@@param.name@@end for@TppPolicy.pure); }

    private static String @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", " followedBy=", "@@java_type(param.type)@ @param.name@@end for@TppPolicy _policy) throws Exception {
@else@
    static String @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@@java_type(param.type)@ @param.name@@end for@) {
@end if@
        StringBuilder _sb = new StringBuilder();
        @for instr in fn.body@
        @emit_instr(instr)@
        @end for@
        if (_sb.length() > 0 && _sb.charAt(_sb.length() - 1) == '\n')
            _sb.deleteCharAt(_sb.length() - 1);
        return _sb.toString();
    }
@end for@
}
END
