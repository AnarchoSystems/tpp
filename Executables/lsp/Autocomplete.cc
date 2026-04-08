#include "Autocomplete.h"
#include <tpp/CompilerOutput.h>
#include <tpp/Types.h>
#include <tpp/AST.h>
#include <map>
#include <string>

namespace tpp
{

// ── Scope: variable name → TypeRef at the cursor position ────────────────────

using Scope = std::map<std::string, TypeRef>;

// Returns the TypeRef for a named type, or nullopt if not found.
[[maybe_unused]] static std::optional<TypeRef> resolveNamed(const std::string &name, const TypeRegistry &reg)
{
    auto it = reg.nameIndex.find(name);
    if (it == reg.nameIndex.end()) return {};
    return NamedType{name};
}

// Collect all fields of a TypeRef if it's a named struct.
static std::vector<FieldDef> fieldsOf(const TypeRef &type, const TypeRegistry &reg)
{
    return std::visit([&](auto &&arg) -> std::vector<FieldDef>
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NamedType>)
        {
            auto it = reg.nameIndex.find(arg.name);
            if (it != reg.nameIndex.end() && it->second.kind == TypeKind::Struct)
                return reg.structs[it->second.index].fields;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
        {
            // list element fields (for for-loop var)
            return fieldsOf(arg->elementType, reg);
        }
        return {};
    }, type);
}

// Returns the element TypeRef for a list type, or nullopt.
static std::optional<TypeRef> elementType(const TypeRef &type)
{
    if (auto *lst = std::get_if<std::shared_ptr<ListType>>(&type))
        if (*lst) return (*lst)->elementType;
    return {};
}

// Walk the AST to collect the scope at (line, char) and return the scope + flag
// indicating whether cursor is immediately after a '.' in a variable expression.
// We also detect the ForNode varName type for inner scopes.
static bool walkForScope(const std::vector<ASTNode> &nodes, int line, int character,
                         Scope &scope, const TypeRegistry &reg)
{
    for (const auto &node : nodes)
    {
        bool found = std::visit([&](auto &&arg) -> bool
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>)
            {
                if (arg->sourceRange.start.line > line) return false;
                // Add the loop variable to scope with element type
                std::string colVar;
                if (auto *v = std::get_if<Variable>(&arg->collectionExpr))
                    colVar = v->name;
                auto it = scope.find(colVar);
                if (it != scope.end())
                {
                    auto et = elementType(it->second);
                    if (et) scope[arg->varName] = *et;
                }
                return walkForScope(arg->body, line, character, scope, reg);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
            {
                if (arg->sourceRange.start.line > line) return false;
                if (walkForScope(arg->thenBody, line, character, scope, reg)) return true;
                return walkForScope(arg->elseBody, line, character, scope, reg);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
            {
                if (arg->sourceRange.start.line > line) return false;
                for (const auto &c : arg->cases)
                    if (walkForScope(c.body, line, character, scope, reg)) return true;
            }
            return false;
        }, node);
        if (found) return true;
    }
    return false;
}

// ── Get the character immediately before the cursor on the same line ──────────
static char charBefore(const std::string &content, int line, int character)
{
    int curLine = 0, col = 0;
    for (size_t i = 0; i < content.size(); ++i)
    {
        if (curLine == line && col == character)
        {
            return i > 0 ? content[i - 1] : '\0';
        }
        if (content[i] == '\n') { ++curLine; col = 0; }
        else                    { ++col; }
    }
    return '\0';
}

// Get the identifier immediately before '.' on the same line
static std::string identBeforeDot(const std::string &content, int line, int character)
{
    // Find byte offset of (line, character - 1) which should be the '.'
    int curLine = 0, col = 0;
    size_t dotPos = std::string::npos;
    for (size_t i = 0; i < content.size(); ++i)
    {
        if (curLine == line && col == character - 1)
        {
            dotPos = i;
            break;
        }
        if (content[i] == '\n') { ++curLine; col = 0; }
        else                    { ++col; }
    }
    if (dotPos == std::string::npos || dotPos == 0 || content[dotPos] != '.') return {};

    // Scan backwards to collect the identifier before '.'
    size_t end = dotPos;
    size_t beg = dotPos;
    while (beg > 0 && (std::isalnum((unsigned char)content[beg - 1]) || content[beg - 1] == '_'))
        --beg;
    return content.substr(beg, end - beg);
}

// ── Make completion item ───────────────────────────────────────────────────────
static nlohmann::json makeItem(const std::string &label, int kind,
                               const std::string &detail = {}, int priority = 2)
{
    // sortText controls ordering: "0." (vars) < "1." (funcs) < "2." (types)
    nlohmann::json item = {{"label", label}, {"kind", kind},
                           {"sortText", std::to_string(priority) + "." + label}};
    if (!detail.empty()) item["detail"] = detail;
    return item;
}

// LSP CompletionItemKind values used here:
//   Field = 5, Variable = 6, Function = 3, Class = 7

// ── Entry point ───────────────────────────────────────────────────────────────
nlohmann::json computeCompletions(const std::string &uri,
                                  int line, int character,
                                  const TppProject &project)
{
    nlohmann::json result = nlohmann::json::array();
    const auto &output = project.output();
    const auto &reg = output.types;

    // Build initial scope from the function that contains (line, character)
    Scope scope;
    const TemplateFunction *enclosingFunc = nullptr;
    for (const auto &func : output.functions)
    {
        if (func.sourceRange.start.line <= line)
        {
            enclosingFunc = &func;
            scope.clear();
            for (const auto &param : func.params)
                scope[param.name] = param.type;
        }
    }

    if (enclosingFunc)
        walkForScope(enclosingFunc->body, line, character, scope, reg);

    // Detect context: after '.'?
    std::string content = project.getContent(uri);
    char prev = charBefore(content, line, character);

    if (prev == '.')
    {
        // Field completion: find the variable before '.'
        std::string varName = identBeforeDot(content, line, character);
        auto it = scope.find(varName);
        if (it != scope.end())
        {
            auto fields = fieldsOf(it->second, reg);
            for (const auto &f : fields)
                result.push_back(makeItem(f.name, 5 /*Field*/));
        }
        // Also try all struct fields as fallback
        if (result.empty())
        {
            for (const auto &sd : reg.structs)
                for (const auto &f : sd.fields)
                    result.push_back(makeItem(f.name, 5 /*Field*/, sd.name + "." + f.name));
        }
        return result;
    }

    // General completion: variables in scope (highest priority)
    for (const auto &[name, _] : scope)
        result.push_back(makeItem(name, 6 /*Variable*/, {}, 0));

    // Function names (medium priority)
    for (const auto &func : output.functions)
        result.push_back(makeItem(func.name, 3 /*Function*/, {}, 1));

    // Type names — only useful in .tpp.types files, not in templates
    if (project.isTypeUri(uri))
    {
        for (const auto &sd : reg.structs)
            result.push_back(makeItem(sd.name, 7 /*Class*/, {}, 2));
        for (const auto &ed : reg.enums)
            result.push_back(makeItem(ed.name, 13 /*Enum*/, {}, 2));
    }

    return result;
}

} // namespace tpp
