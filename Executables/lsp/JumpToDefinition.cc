#include "JumpToDefinition.h"
#include <tpp/Tooling.h>
#include <cctype>

namespace tpp
{

// ── Compiler type aliases (these live in tpp::compiler after the IR migration) ─
using TypeRef          = compiler::TypeRef;
using NamedType        = compiler::NamedType;
using ListType         = compiler::ListType;
using OptionalType     = compiler::OptionalType;
using SemanticModel    = compiler::SemanticModel;
using TemplateFunction = compiler::TemplateFunction;
using Expression       = compiler::Expression;
using Variable         = compiler::Variable;
using FieldAccess      = compiler::FieldAccess;
using ASTNode          = compiler::ASTNode;
using InterpolationNode = compiler::InterpolationNode;
using FunctionCallNode  = compiler::FunctionCallNode;
using ForNode           = compiler::ForNode;
using IfNode            = compiler::IfNode;
using SwitchNode        = compiler::SwitchNode;
using RenderViaNode     = compiler::RenderViaNode;

// ── Range helpers ─────────────────────────────────────────────────────────────

static bool rangeContains(const Range &r, int line, int character)
{
    if (line < r.start.line || line > r.end.line) return false;
    if (line == r.start.line && character < r.start.character) return false;
    if (line == r.end.line   && character > r.end.character)   return false;
    return true;
}

static nlohmann::json locationJson(const std::string &uri, const Range &r)
{
    return {
        {"uri", uri},
        {"range", {
            {"start", {{"line", r.start.line}, {"character", r.start.character}}},
            {"end",   {{"line", r.end.line},   {"character", r.end.character}}}
        }}
    };
}

// ── Convert an Expression back to source text (for argument positioning) ─────
static std::string exprToText(const Expression &e)
{
    if (auto *v = std::get_if<Variable>(&e))
        return v->name;
    if (auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&e))
        return *fa ? exprToText((*fa)->base) + "." + (*fa)->field : std::string{};
    return {};
}

// ── Word at (line, character) in source text ──────────────────────────────────
// Extracts the identifier token (letters, digits, underscore) surrounding
// the given column on the given line.
static std::string wordAtPos(const std::string &source, int line, int character)
{
    int curLine = 0;
    size_t pos = 0;
    while (pos < source.size() && curLine < line)
    {
        if (source[pos++] == '\n') ++curLine;
    }
    if (curLine != line) return {};

    size_t lineEnd = source.find('\n', pos);
    if (lineEnd == std::string::npos) lineEnd = source.size();
    const size_t lineLen = lineEnd - pos;

    if (character < 0 || (size_t)character >= lineLen) return {};
    const char *ln = source.data() + pos;

    auto isIdChar = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
    if (!isIdChar(ln[character])) return {};

    size_t start = character;
    while (start > 0 && isIdChar(ln[start - 1])) --start;
    size_t end = character;
    while (end < lineLen && isIdChar(ln[end])) ++end;

    return std::string(ln + start, end - start);
}

static std::string textForSingleLineRange(const std::string &source, const Range &range)
{
    if (range.start.line != range.end.line)
        return {};

    int curLine = 0;
    size_t pos = 0;
    while (pos < source.size() && curLine < range.start.line)
    {
        if (source[pos++] == '\n') ++curLine;
    }
    if (curLine != range.start.line) return {};

    size_t lineEnd = source.find('\n', pos);
    if (lineEnd == std::string::npos) lineEnd = source.size();
    const size_t lineLen = lineEnd - pos;

    const int start = std::clamp(range.start.character, 0, static_cast<int>(lineLen));
    const int end = std::clamp(range.end.character, start, static_cast<int>(lineLen));
    if (end <= start)
        return {};

    return source.substr(pos + static_cast<size_t>(start), static_cast<size_t>(end - start));
}

static nlohmann::json resolveTypeName(const std::string &name,
                                      const SemanticModel &model,
                                      const TppProject &project);

static nlohmann::json resolveTypeIdentifierInSource(const std::string &src,
                                                    int line,
                                                    int character,
                                                    const SemanticModel &model,
                                                    const TppProject &project,
                                                    const std::vector<TypeSourceSemanticSpan> *typeSpans = nullptr)
{
    if (typeSpans)
    {
        for (const auto &span : *typeSpans)
        {
            if (span.kind != TypeSourceSemanticKind::Type)
                continue;
            if (!rangeContains(span.range, line, character))
                continue;

            const std::string name = textForSingleLineRange(src, span.range);
            if (!name.empty())
                return resolveTypeName(name, model, project);
            break;
        }
    }

    auto tokens = tokenizeTypeSource(src);
    for (const auto &tok : tokens)
    {
        if (tok.kind == TypeSourceTokenKind::Eof) break;
        if (tok.kind == TypeSourceTokenKind::Ident &&
            tok.range.start.line == line &&
            tok.range.start.character <= character &&
            character < tok.range.end.character)
        {
            return resolveTypeName(tok.text, model, project);
        }
    }
    return nullptr;
}

// ── Extract the top-level named type from a TypeRef ───────────────────────────
static std::string typeRefNamedType(const TypeRef &type)
{
    return std::visit([](auto &&arg) -> std::string
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NamedType>)
            return arg.name;
        else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
            return arg ? typeRefNamedType(arg->elementType) : std::string{};
        else if constexpr (std::is_same_v<T, std::shared_ptr<OptionalType>>)
            return arg ? typeRefNamedType(arg->innerType) : std::string{};
        else
            return {};
    }, type);
}

// ── Resolve a type name to its Location in the types file ─────────────────────
static nlohmann::json resolveTypeName(const std::string &name,
                                       const SemanticModel &model,
                                       const TppProject &project)
{
    const Range *range = nullptr;
    if (const auto *sd = model.find_struct(name))
        range = &sd->sourceRange;
    else if (const auto *ed = model.find_enum(name))
        range = &ed->sourceRange;
    else
        return nullptr;

    for (const auto &pUri : project.uris())
    {
        if (project.isTypeUri(pUri))
            return locationJson(pUri, *range);
    }
    return nullptr;
}

// ── Scope: bindings in scope at the cursor position ───────────────────────────
struct TemplateScope
{
    // For-loop variable bindings: varName → location of the @for@ directive
    std::map<std::string, std::pair<std::string /*uri*/, Range>> forBindings;
    // Template parameter types: paramName → type name string
    std::map<std::string, std::string> paramTypes;
    // Full TypeRef for each scoped variable (params + for-loop vars)
    std::map<std::string, TypeRef> varTypes;
};

// ── Resolve the TypeRef of an expression given the scope ─────────────────────
// Returns nullopt if type cannot be determined.
static std::optional<TypeRef> exprTypeRef(const Expression &expr,
                                           const TemplateScope &scope,
                                           const SemanticModel &model)
{
    if (auto *v = std::get_if<Variable>(&expr))
    {
        auto it = scope.varTypes.find(v->name);
        if (it != scope.varTypes.end()) return it->second;
        return {};
    }
    if (auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&expr))
    {
        if (!*fa) return {};
        auto baseType = exprTypeRef((*fa)->base, scope, model);
        if (!baseType) return {};
        // Unwrap list/optional to get the named struct
        auto unwrap = [](const TypeRef &t) -> TypeRef {
            if (auto *lst = std::get_if<std::shared_ptr<ListType>>(&t))
                if (*lst) return (*lst)->elementType;
            if (auto *opt = std::get_if<std::shared_ptr<OptionalType>>(&t))
                if (*opt) return (*opt)->innerType;
            return t;
        };
        TypeRef inner = unwrap(*baseType);
        if (const auto *field = model.find_field(inner, (*fa)->field))
            return field->type;
    }
    return {};
}

// ── Resolve jump to a named field inside a container type ────────────────────
static nlohmann::json resolveFieldLocation(const TypeRef &containerType,
                                            const std::string &fieldName,
                                            const SemanticModel &model,
                                            const TppProject &project)
{
    // Unwrap list/optional to get the named struct
    auto unwrap = [](const TypeRef &t) -> TypeRef {
        if (auto *lst = std::get_if<std::shared_ptr<ListType>>(&t))
            if (*lst) return (*lst)->elementType;
        if (auto *opt = std::get_if<std::shared_ptr<OptionalType>>(&t))
            if (*opt) return (*opt)->innerType;
        return t;
    };
    TypeRef inner = unwrap(containerType);
    if (const auto *field = model.find_field(inner, fieldName))
        for (const auto &pUri : project.uris())
            if (project.isTypeUri(pUri))
                return locationJson(pUri, field->sourceRange);
    return nullptr;
}


// ── Resolve an expression root to a jump target ──────────────────────────────
// Given an expression and the current scope, return the definition location:
//  - If root var is a for-loop binding → jump to the @for@ directive
//  - If root var is a template param  → jump to the param's type definition
//  - Otherwise return null.
static nlohmann::json resolveExpr(const Expression &expr,
                                   const TemplateScope &scope,
                                   const SemanticModel &model,
                                   const TppProject &project,
                                   const std::string &uri)
{
    // Walk to root variable
    const Expression *cur = &expr;
    while (true)
    {
        if (auto *v = std::get_if<Variable>(cur))
        {
            // 1. For-loop binding → jump to @for@ directive
            auto fIt = scope.forBindings.find(v->name);
            if (fIt != scope.forBindings.end())
                return locationJson(fIt->second.first, fIt->second.second);

            // 2. Template param → resolve the param's type
            auto pIt = scope.paramTypes.find(v->name);
            if (pIt != scope.paramTypes.end())
                return resolveTypeName(pIt->second, model, project);

            return nullptr;
        }
        else if (auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(cur))
        {
            if (!*fa) return nullptr;
            cur = &(*fa)->base;
        }
        else
        {
            return nullptr;
        }
    }
}

// ── Resolve expression at cursor, honouring which segment of a.b.c is active ─
// exprStartChar: column of the first character of the expression (after the '@')
static nlohmann::json resolveExprAtCursor(const Expression &expr,
                                           int line, int character,
                                           int exprStartChar,
                                           const TemplateScope &scope,
                                           const SemanticModel &model,
                                           const TppProject &project,
                                           const std::string &uri);

// ── Walk AST nodes with scope to find the definition under (line, char) ────────
static nlohmann::json walkNodes(const std::vector<ASTNode> &nodes,
                                 int line, int character,
                                 TemplateScope scope,
                                 const SemanticModel &model,
                                 const TppProject &project,
                                 const std::string &uri);

static nlohmann::json walkNode(const ASTNode &node,
                                int line, int character,
                                TemplateScope &scope,
                                const SemanticModel &model,
                                const TppProject &project,
                                const std::string &uri)
{
    return std::visit([&](auto &&arg) -> nlohmann::json
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, InterpolationNode>)
        {
            if (rangeContains(arg.sourceRange, line, character))
            {
                const int exprStart = arg.sourceRange.start.character + 1; // skip '@'
                return resolveExprAtCursor(arg.expr, line, character, exprStart,
                                           scope, model, project, uri);
            }
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>)
        {
            if (rangeContains(arg->sourceRange, line, character))
            {
                const int sc      = arg->sourceRange.start.character;
                const int nameEnd = sc + 1 + (int)arg->functionName.size();
                if (character >= sc && character <= nameEnd)
                {
                    // Cursor on function name — jump to definition
                    for (const auto *func : project.compiler().semantic_model().find_template_overloads(arg->functionName))
                        for (const auto &pUri : project.uris())
                            if (!project.isTypeUri(pUri))
                                return locationJson(pUri, func->sourceRange);
                }
                else
                {
                    // Cursor on an argument — reconstruct positions and resolve
                    int argCol = sc + 1 + (int)arg->functionName.size() + 1;
                    bool firstArg = true;
                    for (const auto &argExpr : arg->arguments)
                    {
                        if (!firstArg) argCol += 2;
                        firstArg = false;
                        const std::string argText = exprToText(argExpr);
                        if (character >= argCol && character < argCol + (int)argText.size())
                            return resolveExprAtCursor(argExpr, line, character, argCol,
                                                        scope, model, project, uri);
                        argCol += (int)argText.size();
                    }
                }
            }
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>)
        {
            if (rangeContains(arg->sourceRange, line, character))
            {
                // Cursor is on the @for@ directive itself.
                // Resolve the collection expression (cursor may be on the collection var).
                return resolveExpr(arg->collectionExpr, scope, model, project, uri);
            }
            // Recurse into body with the loop variable added to scope.
            TemplateScope innerScope = scope;
            innerScope.forBindings[arg->varName] = {uri, arg->sourceRange};
            // Compute element TypeRef for the for-loop variable
            auto colType = exprTypeRef(arg->collectionExpr, scope, model);
            if (colType)
            {
                if (auto *lst = std::get_if<std::shared_ptr<ListType>>(&*colType))
                    { if (*lst) innerScope.varTypes[arg->varName] = (*lst)->elementType; }
                else
                    innerScope.varTypes[arg->varName] = *colType;
            }
            return walkNodes(arg->body, line, character, std::move(innerScope), model, project, uri);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
        {
            if (rangeContains(arg->sourceRange, line, character))
                return resolveExpr(arg->condExpr, scope, model, project, uri);
            auto r = walkNodes(arg->thenBody, line, character, scope, model, project, uri);
            if (!r.is_null()) return r;
            return walkNodes(arg->elseBody, line, character, scope, model, project, uri);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
        {
            if (rangeContains(arg->sourceRange, line, character))
                return resolveExpr(arg->expr, scope, model, project, uri);
            for (const auto &c : arg->cases)
            {
                auto r = walkNodes(c.body, line, character, scope, model, project, uri);
                if (!r.is_null()) return r;
            }
            if (arg->defaultCase)
                return walkNodes(arg->defaultCase->body, line, character, scope, model, project, uri);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
        {
            if (rangeContains(arg->sourceRange, line, character))
                return resolveExpr(arg->collectionExpr, scope, model, project, uri);
        }
        return nullptr;
    }, node);
}

static nlohmann::json walkNodes(const std::vector<ASTNode> &nodes,
                                 int line, int character,
                                 TemplateScope scope,
                                 const SemanticModel &model,
                                 const TppProject &project,
                                 const std::string &uri)
{
    for (const auto &n : nodes)
    {
        auto r = walkNode(n, line, character, scope, model, project, uri);
        if (!r.is_null()) return r;
    }
    return nullptr;
}

// ── Entry point ───────────────────────────────────────────────────────────────
nlohmann::json jumpToDefinition(const std::string &uri,
                                int line, int character,
                                const TppProject &project)
{
    const auto &model = project.compiler().semantic_model();
    // ── Types file ────────────────────────────────────────────────────────────
    if (project.isTypeUri(uri))
    {
        std::string src = project.getContent(uri);
        if (auto typeFileIndex = project.typeFileIndex(uri))
        {
            const auto &typeSourceFiles = model.type_source_files();
            if (*typeFileIndex < typeSourceFiles.size())
                return resolveTypeIdentifierInSource(src, line, character, model, project,
                                                   &typeSourceFiles[*typeFileIndex]);
        }
        return resolveTypeIdentifierInSource(src, line, character, model, project);
    }

    // ── Template file ─────────────────────────────────────────────────────────
    std::string src = project.getContent(uri);

    for (const auto &func : model.functions())
    {
        // ── Header line ───────────────────────────────────────────────────────
        if (line == func.sourceRange.start.line)
        {
            std::string word = wordAtPos(src, line, character);
            if (word.empty()) return nullptr;

            // Is the word a type name? → jump to type definition
            auto loc = resolveTypeName(word, model, project);
            if (!loc.is_null()) return loc;

            // Is the word a parameter name? → jump to the parameter's type
            for (const auto &p : func.params)
            {
                if (p.name == word)
                {
                    std::string typeName = typeRefNamedType(p.type);
                    if (!typeName.empty())
                        return resolveTypeName(typeName, model, project);
                }
            }
            return nullptr;
        }

        // ── Function body ─────────────────────────────────────────────────────
        // Check rough line range: body starts after header, ends somewhere before
        // the next function. Skip functions that clearly don't contain this line.
        // (func.sourceRange only covers the header; we rely on the walk returning
        //  null quickly for out-of-range directives.)
        TemplateScope scope;
        for (const auto &p : func.params)
        {
            std::string typeName = typeRefNamedType(p.type);
            if (!typeName.empty())
                scope.paramTypes[p.name] = typeName;
            scope.varTypes[p.name] = p.type;
        }

        auto result = walkNodes(func.body, line, character, scope, model, project, uri);
        if (!result.is_null()) return result;
    }

    return nullptr;
}

// ── resolveExprAtCursor implementation ───────────────────────────────────────
// Defined after walkNode/walkNodes since it only uses helpers already declared.
static nlohmann::json resolveExprAtCursor(const Expression &expr,
                                           int /*line*/, int character,
                                           int exprStartChar,
                                           const TemplateScope &scope,
                                           const SemanticModel &model,
                                           const TppProject &project,
                                           const std::string &uri)
{
    // Build segment chain from the field-access expression.
    // For "a.b.c": chain = [("a",&var), ("b",&fa1), ("c",&fa2)]
    std::vector<std::pair<std::string, const Expression *>> chain;
    std::function<void(const Expression &)> build = [&](const Expression &e) {
        if (auto *v = std::get_if<Variable>(&e))
            chain.push_back({v->name, &e});
        else if (auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&e))
            if (*fa) { build((*fa)->base); chain.push_back({(*fa)->field, &e}); }
    };
    build(expr);
    if (chain.empty()) return resolveExpr(expr, scope, model, project, uri);

    // Find which segment the cursor falls on.
    int col = exprStartChar;
    for (size_t i = 0; i < chain.size(); ++i)
    {
        int segEnd = col + (int)chain[i].first.size();
        if (character >= col && character < segEnd)
        {
            if (i == 0)
            {
                // Cursor on root variable — existing scope resolution
                return resolveExpr(*chain[i].second, scope, model, project, uri);
            }
            else
            {
                // Cursor on a field name — resolve the container type then jump to field
                const auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(chain[i].second);
                if (!fa || !*fa) return nullptr;
                auto containerType = exprTypeRef((*fa)->base, scope, model);
                if (!containerType) return nullptr;
                return resolveFieldLocation(*containerType, chain[i].first, model, project);
            }
        }
        col = segEnd + 1; // skip the '.'
    }
    // Cursor past the end (e.g. on closing '@') — fall back to root
    return resolveExpr(expr, scope, model, project, uri);
}

} // namespace tpp
