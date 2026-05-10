// Shared render-model definitions for all tpp code generation backends.
// These types are not semantic IR; they are the execution-oriented adapter
// used only for lowering template instruction trees into backend emitters.

enum RenderTypeKind
{
    Str,
    Int,
    Bool,
    Named(string),
    List(RenderTypeKind),
    Optional(RenderTypeKind)
}

struct RenderValueRef
{
    path : string;
    isRecursive : bool;
    isOptional : bool;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Instruction IR types — for rendering function generation
// ═══════════════════════════════════════════════════════════════════════════════

struct RenderExprInfo
{
    ref : RenderValueRef;
    type : RenderTypeKind;
}

struct EmitData
{
    textLit : string;
}

struct EmitExprData
{
    expr : RenderExprInfo;
    staticPolicyId : optional<string>;
    useRuntimePolicy : bool;
}

struct BeginCapturedBlockData
{
    blockIndentInParentBlock : optional<int>;
}

struct AlignCellInfo
{
    cellIndex : int;
    body : list<RenderInstruction>;
}

struct ForData
{
    scopeId : int;
    collection : RenderValueRef;
    elemType : RenderTypeKind;
    varName : string;
    enumeratorName : optional<string>;
    body : optional<list<RenderInstruction>>;
    sepLit : optional<string>;
    followedByLit : optional<string>;
    precededByLit : optional<string>;
    capturesBody : bool;
    bodyBlockIndentInParentBlock : optional<int>;
    cells : optional<list<AlignCellInfo>>;
    numCols : int;
    alignSpecChars : list<string>;
}

struct CaseData
{
    tag : string;
    tagLit : string;
    bindingName : optional<string>;
    payloadType : optional<RenderTypeKind>;
    body : optional<list<RenderInstruction>>;
    variantIndex : int;
    isRecursivePayload : bool;
}

struct IfData
{
    condPath : string;
    condIsBool : bool;
    isNegated : bool;
    thenBody : list<RenderInstruction>;
    elseBody : optional<list<RenderInstruction>>;
}

struct SwitchData
{
    expr : RenderValueRef;
    cases : list<CaseData>;
}

enum PolicyRef
{
    Named(string),
    Pure,
    Runtime
}

struct CallData
{
    functionName : string;
    args : list<RenderValueRef>;
    policyArg : optional<PolicyRef>;
    needsTry : bool;
}

enum RenderInstruction
{
    Emit(EmitData),
    EmitExpr(EmitExprData),
    AlignCell,
    BeginCapturedBlock(BeginCapturedBlockData),
    EmitCapturedBlock,
    For(ForData),
    If(IfData),
    Switch(SwitchData),
    Call(CallData)
}

// ═══════════════════════════════════════════════════════════════════════════════
// Policy types
// ═══════════════════════════════════════════════════════════════════════════════

struct PolicyReplacementInfo
{
    findLit : string;
    replaceLit : string;
}

struct PolicyRequireInfo
{
    regexLit : string;
    replaceLit : optional<string>;
}

struct PolicyOutputFilterInfo
{
    regexLit : string;
}

struct PolicyInfo
{
    tagLit : string;
    identifier : string;
    minVal : optional<string>;
    maxVal : optional<string>;
    rejectIfRegexLit : optional<string>;
    rejectMsgLit : optional<string>;
    require : optional<list<PolicyRequireInfo>>;
    replacements : optional<list<PolicyReplacementInfo>>;
    outputFilter : optional<list<PolicyOutputFilterInfo>>;
}

struct ParamInfo
{
    name : string;
    type : RenderTypeKind;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Declaration-source context (shared across source-generating backends)
// ═══════════════════════════════════════════════════════════════════════════════

struct SourceFieldDef
{
    name : string;
    type : RenderTypeKind;
    recursive : bool;
    docComment : optional<string>;
}

struct SourceStructDef
{
    name : string;
    fields : list<SourceFieldDef>;
    docComment : optional<string>;
}

struct SourceVariantDef
{
    tag : string;
    payload : optional<RenderTypeKind>;
    docComment : optional<string>;
}

struct SourceEnumDef
{
    name : string;
    variants : list<SourceVariantDef>;
    docComment : optional<string>;
}

struct SourceInput
{
    enums : list<SourceEnumDef>;
    structs : list<SourceStructDef>;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Function rendering context (shared across all backends)
// ═══════════════════════════════════════════════════════════════════════════════

struct RenderFunctionDef
{
    name : string;
    params : list<ParamInfo>;
    body : list<RenderInstruction>;
    doc : optional<string>;
}

struct RenderFunctionsInput
{
    functions : list<RenderFunctionDef>;
    policies : optional<list<PolicyInfo>>;
    functionPrefix : string;
    namespaceName : optional<string>;
    needsStatic : bool;
    includes : optional<list<string>>;
}
