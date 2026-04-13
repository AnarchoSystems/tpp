#include <tpp/IRBuilder.h>
#include "tpp/Version.h"
#include <cassert>

namespace tpp
{

// ═══════════════════════════════════════════════════════════════════════════
// compiler::Range → tpp::SourceRange (optional)
// ═══════════════════════════════════════════════════════════════════════════

static std::optional<SourceRange> convert_range(const Range &r, bool include)
{
    if (!include) return std::nullopt;
    SourceRange sr;
    sr.start.line = r.start.line;
    sr.start.character = r.start.character;
    sr.end.line = r.end.line;
    sr.end.character = r.end.character;
    return sr;
}

// ═══════════════════════════════════════════════════════════════════════════
// compiler::TypeRef → tpp::TypeKind
// ═══════════════════════════════════════════════════════════════════════════

static TypeKind convert_type(const compiler::TypeRef &t)
{
    return std::visit([](auto &&arg) -> TypeKind
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, compiler::StringType>)
            return TypeKind{TypeKind::Value{std::in_place_index<0>}};
        else if constexpr (std::is_same_v<T, compiler::IntType>)
            return TypeKind{TypeKind::Value{std::in_place_index<1>}};
        else if constexpr (std::is_same_v<T, compiler::BoolType>)
            return TypeKind{TypeKind::Value{std::in_place_index<2>}};
        else if constexpr (std::is_same_v<T, compiler::NamedType>)
            return TypeKind{TypeKind::Value{std::in_place_index<3>, arg.name}};
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::ListType>>)
            return TypeKind{TypeKind::Value{std::in_place_index<4>,
                std::make_unique<TypeKind>(convert_type(arg->elementType))}};
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::OptionalType>>)
            return TypeKind{TypeKind::Value{std::in_place_index<5>,
                std::make_unique<TypeKind>(convert_type(arg->innerType))}};
        else
            return TypeKind{TypeKind::Value{std::in_place_index<0>}};
    }, t);
}

// ═══════════════════════════════════════════════════════════════════════════
// compiler::InstrExpr → tpp::ExprInfo
// ═══════════════════════════════════════════════════════════════════════════

static ExprInfo convert_expr(const compiler::InstrExpr &e)
{
    ExprInfo out;
    out.path = e.path;
    out.type = std::make_unique<TypeKind>(convert_type(e.resolvedType));
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// compiler::Instruction → tpp::Instruction
// ═══════════════════════════════════════════════════════════════════════════

static std::vector<Instruction> convert_body(const std::vector<compiler::Instruction> &body);

static Instruction convert_instruction(const compiler::Instruction &instr)
{
    return std::visit([](auto &&arg) -> Instruction
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, compiler::EmitInstr>)
        {
            EmitInstr out;
            out.text = arg.text;
            return Instruction{Instruction::Value{std::in_place_index<0>, out}};
        }
        else if constexpr (std::is_same_v<T, compiler::EmitExprInstr>)
        {
            EmitExprInstr out;
            out.expr = convert_expr(arg.expr);
            out.policy = arg.policy;
            return Instruction{Instruction::Value{std::in_place_index<1>, std::move(out)}};
        }
        else if constexpr (std::is_same_v<T, compiler::AlignCellInstr>)
        {
            return Instruction{Instruction::Value{std::in_place_index<2>}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::ForInstr>>)
        {
            auto out = std::make_unique<ForInstr>();
            out->varName = arg->varName;
            out->enumeratorName = arg->enumeratorName;
            out->collection = convert_expr(arg->collection);
            out->body = std::make_unique<std::vector<Instruction>>(convert_body(arg->body));
            out->sep = arg->sep;
            out->followedBy = arg->followedBy;
            out->precededBy = arg->precededBy;
            out->isBlock = arg->isBlock;
            out->insertCol = arg->insertCol;
            out->policy = arg->policy;
            out->hasAlign = arg->hasAlign;
            out->alignSpec = arg->alignSpec;
            return Instruction{Instruction::Value{std::in_place_index<3>, std::move(out)}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::IfInstr>>)
        {
            auto out = std::make_unique<IfInstr>();
            out->condExpr = convert_expr(arg->condExpr);
            out->negated = arg->negated;
            out->thenBody = std::make_unique<std::vector<Instruction>>(convert_body(arg->thenBody));
            out->elseBody = std::make_unique<std::vector<Instruction>>(convert_body(arg->elseBody));
            out->isBlock = arg->isBlock;
            out->insertCol = arg->insertCol;
            return Instruction{Instruction::Value{std::in_place_index<4>, std::move(out)}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::SwitchInstr>>)
        {
            auto out = std::make_unique<SwitchInstr>();
            out->expr = convert_expr(arg->expr);
            auto cases = std::make_unique<std::vector<CaseInstr>>();
            for (const auto &c : arg->cases)
            {
                CaseInstr ci;
                ci.tag = c.tag;
                ci.bindingName = c.bindingName;
                if (c.payloadType.has_value())
                    ci.payloadType = std::make_unique<TypeKind>(convert_type(*c.payloadType));
                ci.body = std::make_unique<std::vector<Instruction>>(convert_body(c.body));
                if (c.tag.empty())
                    out->defaultCase = std::make_unique<CaseInstr>(std::move(ci));
                else
                    cases->push_back(std::move(ci));
            }
            out->cases = std::move(cases);
            out->isBlock = arg->isBlock;
            out->insertCol = arg->insertCol;
            out->policy = arg->policy;
            return Instruction{Instruction::Value{std::in_place_index<5>, std::move(out)}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::CallInstr>>)
        {
            CallInstr out;
            out.functionName = arg->functionName;
            for (const auto &a : arg->arguments)
                out.arguments.push_back(convert_expr(a));
            return Instruction{Instruction::Value{std::in_place_index<6>, std::move(out)}};
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::RenderViaInstr>>)
        {
            RenderViaInstr out;
            out.collection = convert_expr(arg->collection);
            out.functionName = arg->functionName;
            out.sep = arg->sep;
            out.followedBy = arg->followedBy;
            out.precededBy = arg->precededBy;
            out.isBlock = arg->isBlock;
            out.insertCol = arg->insertCol;
            out.policy = arg->policy;
            return Instruction{Instruction::Value{std::in_place_index<7>, std::move(out)}};
        }
        else
        {
            return Instruction{Instruction::Value{std::in_place_index<2>}};
        }
    }, instr);
}

static std::vector<Instruction> convert_body(const std::vector<compiler::Instruction> &body)
{
    std::vector<Instruction> out;
    out.reserve(body.size());
    for (const auto &instr : body)
        out.push_back(convert_instruction(instr));
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// compiler::InstructionFunction → tpp::FunctionDef
// ═══════════════════════════════════════════════════════════════════════════

static FunctionDef convert_function(const compiler::InstructionFunction &f, bool includeRanges)
{
    FunctionDef out;
    out.name = f.name;
    for (const auto &p : f.params)
    {
        ParamDef pd;
        pd.name = p.name;
        pd.type = std::make_unique<TypeKind>(convert_type(p.type));
        out.params.push_back(std::move(pd));
    }
    out.body = std::make_unique<std::vector<Instruction>>(convert_body(f.body));
    out.policy = f.policy;
    out.doc = f.doc;
    out.sourceRange = convert_range(f.sourceRange, includeRanges);
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// Schema types
// ═══════════════════════════════════════════════════════════════════════════

static StructDef convert_struct(const compiler::StructDef &s, bool includeRanges)
{
    StructDef out;
    out.name = s.name;
    for (const auto &f : s.fields)
    {
        FieldDef fd;
        fd.name = f.name;
        fd.type = std::make_unique<TypeKind>(convert_type(f.type));
        fd.recursive = f.recursive;
        fd.doc = f.doc;
        fd.sourceRange = convert_range(f.sourceRange, includeRanges);
        out.fields.push_back(std::move(fd));
    }
    out.doc = s.doc;
    out.sourceRange = convert_range(s.sourceRange, includeRanges);
    return out;
}

static EnumDef convert_enum(const compiler::EnumDef &e, bool includeRanges)
{
    EnumDef out;
    out.name = e.name;
    for (const auto &v : e.variants)
    {
        VariantDef vd;
        vd.tag = v.tag;
        if (v.payload.has_value())
            vd.payload = std::make_unique<TypeKind>(convert_type(*v.payload));
        vd.recursive = v.recursive;
        vd.doc = v.doc;
        vd.sourceRange = convert_range(v.sourceRange, includeRanges);
        out.variants.push_back(std::move(vd));
    }
    out.doc = e.doc;
    out.sourceRange = convert_range(e.sourceRange, includeRanges);
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// Policy types
// ═══════════════════════════════════════════════════════════════════════════

static PolicyDef convert_policy(const compiler::Policy &p)
{
    PolicyDef out;
    out.tag = p.tag;
    if (p.length.has_value())
    {
        PolicyLength pl;
        pl.min = p.length->min;
        pl.max = p.length->max;
        out.length = std::move(pl);
    }
    if (p.rejectIf.has_value())
    {
        PolicyRejectIf pr;
        pr.regex = p.rejectIf->regex;
        pr.message = p.rejectIf->message;
        out.rejectIf = std::move(pr);
    }
    for (const auto &r : p.require)
    {
        PolicyRequire rq;
        rq.regex = r.regex;
        rq.replace = r.replace;
        rq.compiledReplace = r.compiled_replace;
        out.require.push_back(std::move(rq));
    }
    for (const auto &r : p.replacements)
    {
        PolicyReplacement rp;
        rp.find = r.find;
        rp.replace = r.replace;
        out.replacements.push_back(std::move(rp));
    }
    for (const auto &f : p.outputFilter)
    {
        PolicyOutputFilter of;
        of.regex = f.regex;
        out.outputFilter.push_back(std::move(of));
    }
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// Raw typedef extraction
// ═══════════════════════════════════════════════════════════════════════════

// Extract a single type definition (struct or enum) from the raw typedefs string.
// Includes any preceding doc comments (lines starting with //).
static std::string extract_type_definition(const std::string &raw,
                                           const std::string &keyword,
                                           const std::string &name)
{
    std::string marker = keyword + " " + name;
    auto pos = raw.find(marker);
    if (pos == std::string::npos) return "";

    // Walk backward to include preceding doc-comment lines and blank lines
    auto start = pos;
    while (start > 0)
    {
        auto prev_end = start - 1;
        if (prev_end == std::string::npos || raw[prev_end] != '\n') break;
        auto prev_start = raw.rfind('\n', prev_end - 1);
        prev_start = (prev_start == std::string::npos) ? 0 : prev_start + 1;
        auto line = raw.substr(prev_start, prev_end - prev_start);
        auto first_non_ws = line.find_first_not_of(" \t");
        if (first_non_ws == std::string::npos)
        {
            start = prev_start;
            continue;
        }
        if (line.substr(first_non_ws, 2) == "//")
        {
            start = prev_start;
            continue;
        }
        break;
    }

    // Strip leading blank lines from extracted text
    while (start < pos && (raw[start] == '\n' || raw[start] == '\r'))
        ++start;

    // Find closing brace
    auto brace = raw.find('{', pos);
    if (brace == std::string::npos) return "";
    int depth = 0;
    auto end = brace;
    for (; end < raw.size(); ++end)
    {
        if (raw[end] == '{') ++depth;
        else if (raw[end] == '}') { --depth; if (depth == 0) { ++end; break; } }
    }

    return raw.substr(start, end - start);
}

// ═══════════════════════════════════════════════════════════════════════════
// Top-level builder
// ═══════════════════════════════════════════════════════════════════════════

IR build_ir(const compiler::TypeRegistry &types,
            const std::vector<compiler::InstructionFunction> &functions,
            const compiler::PolicyRegistry &policies,
            const std::string &rawTypedefs,
            bool includeSourceRanges)
{
    IR out;
    out.versionMajor = TPP_VERSION_MAJOR;
    out.versionMinor = TPP_VERSION_MINOR;
    out.versionPatch = TPP_VERSION_PATCH;

    for (const auto &s : types.structs)
    {
        auto sd = convert_struct(s, includeSourceRanges);
        sd.rawTypedefs = extract_type_definition(rawTypedefs, "struct", s.name);
        out.structs.push_back(std::move(sd));
    }
    for (const auto &e : types.enums)
    {
        auto ed = convert_enum(e, includeSourceRanges);
        ed.rawTypedefs = extract_type_definition(rawTypedefs, "enum", e.name);
        out.enums.push_back(std::move(ed));
    }
    for (const auto &f : functions)
        out.functions.push_back(convert_function(f, includeSourceRanges));
    for (const auto &[tag, policy] : policies.all())
        out.policies.push_back(convert_policy(policy));

    return out;
}

} // namespace tpp
