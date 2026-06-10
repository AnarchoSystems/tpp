// ── Type name helpers (inline — used in expressions) ─────────────────────

template java_ir_type(t: RenderTypeKind)
@switch t@@case Str@String@end case@@case Int@int@end case@@case Bool@boolean@end case@@case Named(name)@@name@@end case@@case List(elemType)@java.util.List<@java_ir_boxed_type(elemType)@>@end case@@case Optional(innerType)@@java_ir_type(innerType)@@end case@@end switch@
END

template java_ir_boxed_type(t: RenderTypeKind)
@switch t@@case Str@String@end case@@case Int@Integer@end case@@case Bool@Boolean@end case@@case Named(name)@@name@@end case@@case List(elemType)@java.util.List<@java_ir_boxed_type(elemType)@>@end case@@case Optional(innerType)@@java_ir_boxed_type(innerType)@@end case@@end switch@
END

template java_inner_arr_elem(t: RenderTypeKind)
@switch t@@case Str@_inner.getString(_j)@end case@@case Int@_inner.getInt(_j)@end case@@case Bool@_inner.getBoolean(_j)@end case@@case Named(name)@@name@.fromJson(_inner.getJSONObject(_j))@end case@@case List(inner)@null@end case@@case Optional(inner)@null@end case@@end switch@
END

// ── List loop body (block-style — called on its own line) ────────────────

template java_list_body(name: string, elemType: RenderTypeKind)
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
java.util.List<@java_ir_boxed_type(inner)@> _row = new java.util.ArrayList<>();
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

template java_read_field(name: string, type: RenderTypeKind)
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

template java_write_field(name: string, type: RenderTypeKind)
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

template java_variant_list_add(elemType: RenderTypeKind)
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
    java.util.ArrayList<@java_ir_boxed_type(inner)@> _row = new java.util.ArrayList<>();
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

template java_variant_from_json(v: SourceVariantDef, enumName: string)
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
    java.util.ArrayList<@java_ir_boxed_type(elemType)@> _list = new java.util.ArrayList<>();
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

template render_java_top_level_enum(e: SourceEnumDef)
@if e.docComment@@e.docComment@@end if@class @e.name@ {
    public String _tag;
    public Object _payload;

    @e.name@(String tag, Object payload) { _tag = tag; _payload = payload; }
    @for v in e.variants@
    @if not v.payload@
    @if v.docComment@@v.docComment@@end if@    public boolean is@v.tag@() { return "@v.tag@".equals(_tag); }
    @else@
    public boolean is@v.tag@() { return "@v.tag@".equals(_tag); }
    @if v.docComment@@v.docComment@@end if@    \@SuppressWarnings("unchecked")
    public @java_ir_type(v.payload)@ get@v.tag@() { return (@java_ir_boxed_type(v.payload)@) _payload; }
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
END

template render_java_nested_enum(e: SourceEnumDef)
@if e.docComment@@e.docComment@@end if@    static class @e.name@ {
        public String _tag;
        public Object _payload;

        @e.name@(String tag, Object payload) { _tag = tag; _payload = payload; }
        @for v in e.variants@
        @if not v.payload@
        @if v.docComment@@v.docComment@@end if@        public boolean is@v.tag@() { return "@v.tag@".equals(_tag); }
        @else@
        public boolean is@v.tag@() { return "@v.tag@".equals(_tag); }
        @if v.docComment@@v.docComment@@end if@        \@SuppressWarnings("unchecked")
        public @java_ir_type(v.payload)@ get@v.tag@() { return (@java_ir_boxed_type(v.payload)@) _payload; }
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
END

template render_java_top_level_struct(s: SourceStructDef)
@if s.docComment@@s.docComment@@end if@class @s.name@ {
    @for field in s.fields@
    @if field.docComment@@field.docComment@@end if@    public @java_ir_type(field.type)@ @field.name@;
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
END

template render_java_nested_struct(s: SourceStructDef)
@if s.docComment@@s.docComment@@end if@    static class @s.name@ {
        @for field in s.fields@
        @if field.docComment@@field.docComment@@end if@        public @java_ir_type(field.type)@ @field.name@;
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
END

// ── Main codegen template ────────────────────────────────────────────────

template render_java_source(input: SourceInput)
// Generated by tpp2java — do not edit.
@for e in input.enums@
@render_java_top_level_enum(e)@
@end for@
@for s in input.structs@
@render_java_top_level_struct(s)@
@end for@
END

template render_java_types_body(input: SourceInput)
@for e in input.enums@
@render_java_nested_enum(e)@
@end for@
@for s in input.structs@
@render_java_nested_struct(s)@
@end for@
END

// ═══════════════════════════════════════════════════════════════════════════════
// Instruction IR → Java rendering functions
// ═══════════════════════════════════════════════════════════════════════════════

// ── Expression → Java String expression ──────────────────────────────────────

template java_type(t: RenderTypeKind)
@switch t@@case Str@String@end case@@case Int@int@end case@@case Bool@boolean@end case@@case Named(name)@@name@@end case@@case List(elemType)@java.util.List<@java_type(elemType)@>@end case@@case Optional(innerType)@@java_type(innerType)@@end case@@end switch@
END

template java_optional_to_str(path: string, inner: RenderTypeKind)
@switch inner@@case Str@(@path@ != null ? @path@ : "")@end case@@case Int@(@path@ != null ? Integer.toString(@path@) : "")@end case@@case Bool@(@path@ != null ? Boolean.toString(@path@) : "")@end case@@case Named(n)@(@path@ != null ? String.valueOf(@path@) : "")@end case@@case List(e)@(@path@ != null ? String.valueOf(@path@) : "")@end case@@case Optional(i)@(@path@ != null ? String.valueOf(@path@) : "")@end case@@end switch@
END

template java_expr_to_str(expr: RenderExprInfo)
@switch expr.type@@case Str@@expr.ref.path@@end case@@case Int@Integer.toString(@expr.ref.path@)@end case@@case Bool@Boolean.toString(@expr.ref.path@)@end case@@case Named(n)@String.valueOf(@expr.ref.path@)@end case@@case List(e)@String.valueOf(@expr.ref.path@)@end case@@case Optional(inner)@@java_optional_to_str(expr.ref.path, inner)@@end case@@end switch@
END

// ── Leaf instruction templates ───────────────────────────────────────────────

template emit_emit(e: EmitData)
if (!_sb.emit(@e.textLit@))
    throw new RuntimeException("tpp render error: " + _sb.error());
END

template emit_emit_expr(e: EmitExprData)
@if e.staticPolicyId@
if (!_sb.emitValue(@java_expr_to_str(e.expr)@, @e.staticPolicyId@))
    throw new RuntimeException("tpp render error: " + _sb.error());
@else@
@if e.useRuntimePolicy@
if (!_sb.emitValue(@java_expr_to_str(e.expr)@, _policy))
    throw new RuntimeException("tpp render error: " + _sb.error());
@else@
if (!_sb.emitValue(@java_expr_to_str(e.expr)@))
    throw new RuntimeException("tpp render error: " + _sb.error());
@end if@
@end if@
END

template emit_call(c: CallData)
@if c.policyArg@
if (!_sb.emit(@c.functionName@(@for arg in c.args | sep=", " followedBy=", "@@arg.path@@end for@@java_policy_ref(c.policyArg)@)))
    throw new RuntimeException("tpp render error: " + _sb.error());
@else@
if (!_sb.emit(@c.functionName@(@for arg in c.args | sep=", "@@arg.path@@end for@)))
    throw new RuntimeException("tpp render error: " + _sb.error());
@end if@
END

template java_policy_ref(ref: PolicyRef)
@switch ref@@case Named(tag)@@tag@@end case@@case Pure@TppPolicy.pure@end case@@case Runtime@_policy@end case@@end switch@
END

// ── Recursive instruction dispatcher ─────────────────────────────────────────

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
if (!_sb.beginCapturedBlock(@p.blockIndentInParentBlock@))
@else@
if (!_sb.beginCapturedBlock())
@end if@
    throw new RuntimeException("tpp render error: " + _sb.error());
@end case@
@case EmitCapturedBlock@
if (!_sb.emitCapturedBlock())
    throw new RuntimeException("tpp render error: " + _sb.error());
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
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collection.path@.size(); _i@f.scopeId@++) {
    @java_type(f.elemType)@ @f.varName@ = @f.collection.path@.get(_i@f.scopeId@);
    @if f.enumeratorName@
    int @f.enumeratorName@ = _i@f.scopeId@;
    @end if@
    @if f.precededByLit@
    if (!_sb.emit(@f.precededByLit@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @end if@
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    @if f.sepLit@
    if (_i@f.scopeId@ + 1 < @f.collection.path@.size()) {
        if (!_sb.emit(@f.sepLit@))
            throw new RuntimeException("tpp render error: " + _sb.error());
    }
    @if f.followedByLit@
    else if (!@f.collection.path@.isEmpty() && !_sb.emit(@f.followedByLit@)) {
        throw new RuntimeException("tpp render error: " + _sb.error());
    }
    @end if@
    @else@
    @if f.followedByLit@
    if (_i@f.scopeId@ + 1 >= @f.collection.path@.size() && !@f.collection.path@.isEmpty() && !_sb.emit(@f.followedByLit@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @end if@
    @end if@
}
@end if@
END

template emit_for_block(f: ForData)
@if f.body@
@if f.bodyBlockIndentInParentBlock@
if (!_sb.beginCapturedBlock(@f.bodyBlockIndentInParentBlock@))
@else@
if (!_sb.beginCapturedBlock())
@end if@
    throw new RuntimeException("tpp render error: " + _sb.error());
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collection.path@.size(); _i@f.scopeId@++) {
    @java_type(f.elemType)@ @f.varName@ = @f.collection.path@.get(_i@f.scopeId@);
    @if f.enumeratorName@
    int @f.enumeratorName@ = _i@f.scopeId@;
    @end if@
    _sb.beginCapture();
    @for instr in f.body@
    @emit_instr(instr)@
    @end for@
    if (_sb.hasError()) {
        _sb.endCapture();
        throw new RuntimeException("tpp render error: " + _sb.error());
    }
    @if f.bodyBlockIndentInParentBlock@
    String _iterText@f.scopeId@ = _sb.endBlockCapture(@f.bodyBlockIndentInParentBlock@);
    @else@
    TppWriter.CaptureResult _iter@f.scopeId@ = _sb.endCaptureResult();
    String _iterText@f.scopeId@ = _iter@f.scopeId@.text;
    @end if@
    boolean _trimIterTrailingNewline@f.scopeId@ = false;
    if (_i@f.scopeId@ + 1 < @f.collection.path@.size()) {
        @if f.sepLit@
        _trimIterTrailingNewline@f.scopeId@ = !@f.sepLit@.isEmpty() && @f.sepLit@.chars().anyMatch(ch -> ch != '\n' && ch != '\r');
        @end if@
    } else {
        @if f.followedByLit@
        _trimIterTrailingNewline@f.scopeId@ = !@f.followedByLit@.isEmpty() && @f.followedByLit@.chars().anyMatch(ch -> ch != '\n' && ch != '\r');
        @end if@
    }
    if (_trimIterTrailingNewline@f.scopeId@ && _iterText@f.scopeId@.endsWith("\n"))
        _iterText@f.scopeId@ = _iterText@f.scopeId@.substring(0, _iterText@f.scopeId@.length() - 1);
    @if f.precededByLit@
    @if f.bodyBlockIndentInParentBlock@
    if (!_sb.emitWithIndentColumn(@f.precededByLit@, @f.bodyBlockIndentInParentBlock@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @else@
    if (!_sb.emit(@f.precededByLit@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @end if@
    @end if@
    if (!_sb.emit(_iterText@f.scopeId@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @if f.sepLit@
    if (_i@f.scopeId@ + 1 < @f.collection.path@.size()) {
        @if f.bodyBlockIndentInParentBlock@
        if (!_sb.emitWithIndentColumn(@f.sepLit@, @f.bodyBlockIndentInParentBlock@))
            throw new RuntimeException("tpp render error: " + _sb.error());
        @else@
        if (!_sb.emit(@f.sepLit@))
            throw new RuntimeException("tpp render error: " + _sb.error());
        @end if@
    } else {
        @if f.followedByLit@
        @if f.bodyBlockIndentInParentBlock@
        if (!@f.collection.path@.isEmpty() && !_sb.emitWithIndentColumn(@f.followedByLit@, @f.bodyBlockIndentInParentBlock@))
            throw new RuntimeException("tpp render error: " + _sb.error());
        @else@
        if (!@f.collection.path@.isEmpty() && !_sb.emit(@f.followedByLit@))
            throw new RuntimeException("tpp render error: " + _sb.error());
        @end if@
        @end if@
    }
    @else@
    @if f.followedByLit@
    @if f.bodyBlockIndentInParentBlock@
    if (_i@f.scopeId@ + 1 >= @f.collection.path@.size() && !@f.collection.path@.isEmpty() && !_sb.emitWithIndentColumn(@f.followedByLit@, @f.bodyBlockIndentInParentBlock@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @else@
    if (_i@f.scopeId@ + 1 >= @f.collection.path@.size() && !@f.collection.path@.isEmpty() && !_sb.emit(@f.followedByLit@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @end if@
    @end if@
    @end if@
}
if (!_sb.emitCapturedBlock())
    throw new RuntimeException("tpp render error: " + _sb.error());
@end if@
END

// ── Aligned for ──────────────────────────────────────────────────────────────

template emit_align_cell(scopeId: int, cell: AlignCellInfo)
_sb.beginCapture();
@for instr in cell.body@
@emit_instr(instr)@
@end for@
if (_sb.hasError()) {
    _sb.endCapture();
    throw new RuntimeException("tpp render error: " + _sb.error());
}
TppWriter.CaptureResult _cell@scopeId@_@cell.cellIndex@ = _sb.endCaptureResult();
if (TppWriter.containsLineBreak(_cell@scopeId@_@cell.cellIndex@.text))
    throw new RuntimeException("tpp render error: aligned cells must be single-line");
_row@scopeId@[@cell.cellIndex@] = _cell@scopeId@_@cell.cellIndex@.text;
END

template emit_aligned_for(f: ForData)
@if f.cells@
java.util.List<String[]> _rows@f.scopeId@ = new java.util.ArrayList<>();
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < @f.collection.path@.size(); _i@f.scopeId@++) {
    @java_type(f.elemType)@ @f.varName@ = @f.collection.path@.get(_i@f.scopeId@);
    @if f.enumeratorName@
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
char[] _chars@f.scopeId@ = new char[]{@for ch in f.alignSpecChars | sep=", "@'@ch@'@end for@};
if (_chars@f.scopeId@.length == 1) {
    java.util.Arrays.fill(_spec@f.scopeId@, _chars@f.scopeId@[0]);
} else {
    for (int _ci = 0; _ci < _chars@f.scopeId@.length && _ci < _spec@f.scopeId@.length; _ci++)
        _spec@f.scopeId@[_ci] = _chars@f.scopeId@[_ci];
}
for (int _i@f.scopeId@ = 0; _i@f.scopeId@ < _rows@f.scopeId@.size(); _i@f.scopeId@++) {
    String[] _r = _rows@f.scopeId@.get(_i@f.scopeId@);
    StringBuilder _line@f.scopeId@ = new StringBuilder();
    for (int _c = 0; _c < _r.length; _c++) {
        if (_c + 1 < @f.numCols@) {
            _line@f.scopeId@.append(TppWriter.padCell(_r[_c], _cw@f.scopeId@[_c], _spec@f.scopeId@[_c]));
        } else {
            char _sp = _spec@f.scopeId@[_c]; int _pd = _cw@f.scopeId@[_c] - _r[_c].length();
            if (_pd > 0 && _sp != 'l') { int _left = (_sp == 'c') ? _pd / 2 : _pd; _line@f.scopeId@.append(" ".repeat(_left)); }
            _line@f.scopeId@.append(_r[_c]);
        }
    }
    @if f.precededByLit@
    if (!_sb.emit(@f.precededByLit@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @end if@
    if (!_sb.emit(_line@f.scopeId@.toString()))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @if f.sepLit@
    if (_i@f.scopeId@ + 1 < _rows@f.scopeId@.size()) {
        if (!_sb.emit(@f.sepLit@))
            throw new RuntimeException("tpp render error: " + _sb.error());
    }
    @if f.followedByLit@
    else if (!_rows@f.scopeId@.isEmpty() && !_sb.emit(@f.followedByLit@)) {
        throw new RuntimeException("tpp render error: " + _sb.error());
    }
    @end if@
    @else@
    @if f.followedByLit@
    if (_i@f.scopeId@ + 1 >= _rows@f.scopeId@.size() && !_rows@f.scopeId@.isEmpty() && !_sb.emit(@f.followedByLit@))
        throw new RuntimeException("tpp render error: " + _sb.error());
    @end if@
    @end if@
}
@end if@
END

// ── If / Else ────────────────────────────────────────────────────────────────

template emit_if(i: IfData)
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
switch (@s.expr.path@._tag) {
@for c in s.cases@
case @c.tagLit@: {
    @if c.bindingName@
    @if c.payloadType@
    @java_type(c.payloadType)@ @c.bindingName@ = @s.expr.path@.get@c.tag@();
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

template emit_policy_instance(pol: PolicyInfo)
static final TppPolicy @pol.identifier@ = new TppPolicy(@pol.tagLit@);
static {
    @if pol.minVal@
    @pol.identifier@.minLength = @pol.minVal@;
    @end if@
    @if pol.maxVal@
    @pol.identifier@.maxLength = @pol.maxVal@;
    @end if@
    @if pol.rejectIfRegexLit@
    @if pol.rejectMsgLit@
    { TppPolicy.RejectRule rr = new TppPolicy.RejectRule(); rr.pattern = java.util.regex.Pattern.compile(@pol.rejectIfRegexLit@); rr.message = @pol.rejectMsgLit@; @pol.identifier@.rejectIf = rr; }
    @end if@
    @end if@
    @if pol.require@
    @for r in pol.require@
    { TppPolicy.RequireStep rs = new TppPolicy.RequireStep(); rs.pattern = java.util.regex.Pattern.compile(@r.regexLit@); @if r.replaceLit@rs.replace = @r.replaceLit@; @end if@@pol.identifier@.require.add(rs); }
    @end for@
    @end if@
    @if pol.replacements@
    @pol.identifier@.replacements = new String[][]{@for r in pol.replacements | sep=", "@{@r.findLit@, @r.replaceLit@}@end for@};
    @end if@
    @if pol.outputFilter@
    @pol.identifier@.outputFilter = new java.util.regex.Pattern[]{@for f in pol.outputFilter | sep=", "@java.util.regex.Pattern.compile(@f.regexLit@)@end for@};
    @end if@
}
END

template emit_policy_instances(ctx: RenderFunctionsInput)
@if ctx.policies@
@for pol in ctx.policies@
@emit_policy_instance(pol)@
@end for@
@end if@
END

template emit_java_function(fn: RenderFunctionDef, ctx: RenderFunctionsInput)
@if ctx.policies@
@if fn.doc@@fn.doc@@end if@    static String @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@@java_type(param.type)@ @param.name@@end for@) throws Exception { return @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", " followedBy=", "@@param.name@@end for@TppPolicy.pure); }

    private static String @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", " followedBy=", "@@java_type(param.type)@ @param.name@@end for@TppPolicy _policy) throws Exception {
@else@
@if fn.doc@@fn.doc@@end if@    static String @ctx.functionPrefix@@fn.name@(@for param in fn.params | sep=", "@@java_type(param.type)@ @param.name@@end for@) {
@end if@
        TppWriter _sb = new TppWriter();
        @for instr in fn.body@
        @emit_instr(instr)@
        @end for@
        return _sb.takeOutput(TppWriter.OutputPostProcessing.StripSingleTrailingNewline);
    }
END

// ── Main entry point: Functions class ────────────────────────────────────────

template render_java_functions(ctx: RenderFunctionsInput)

class @if ctx.namespaceName@@ctx.namespaceName@@else@Functions@end if@ extends TppRuntime {
    @emit_policy_instances(ctx)@
@for fn in ctx.functions@
@emit_java_function(fn, ctx)@
@end for@
}
END

template render_java_bundle(input: SourceInput, ctx: RenderFunctionsInput)
// Generated by tpp2java — namespaced test bundle.

class @if ctx.namespaceName@@ctx.namespaceName@@end if@ extends TppRuntime {
    @emit_policy_instances(ctx)@
    @render_java_types_body(input)@
@for fn in ctx.functions@
@emit_java_function(fn, ctx)@
@end for@
}
END