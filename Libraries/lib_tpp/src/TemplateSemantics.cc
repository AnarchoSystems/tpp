#include <tpp/TemplateParser.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace tpp::compiler
{

namespace
{
    std::string trim(const std::string &s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    bool isIdentStart(char c)
    {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    bool isIdentChar(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    bool isIdentifier(const std::string &s)
    {
        if (s.empty() || !isIdentStart(s[0]))
            return false;
        for (size_t i = 1; i < s.size(); ++i)
        {
            if (!isIdentChar(s[i]))
                return false;
        }
        return true;
    }

    bool isPathExpressionText(const std::string &s)
    {
        std::string t = trim(s);
        if (t.empty())
            return false;

        size_t start = 0;
        while (start <= t.size())
        {
            size_t dot = t.find('.', start);
            std::string part = (dot == std::string::npos) ? t.substr(start) : t.substr(start, dot - start);
            if (!isIdentifier(part))
                return false;
            if (dot == std::string::npos)
                break;
            start = dot + 1;
        }

        return true;
    }

    bool isOptionalType(const TypeRef &t)
    {
        return std::holds_alternative<std::shared_ptr<OptionalType>>(t);
    }

    TypeRef unwrapOptionalType(const TypeRef &t)
    {
        if (auto opt = std::get_if<std::shared_ptr<OptionalType>>(&t))
            return (*opt)->innerType;
        return t;
    }

    std::string expressionToPath(const Expression &expr)
    {
        return std::visit([&](auto &&arg) -> std::string
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Variable>)
            {
                return arg.name;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<FieldAccess>>)
            {
                std::string base = expressionToPath(arg->base);
                if (base.empty())
                    return arg->field;
                return base + "." + arg->field;
            }
            return "";
        }, expr);
    }

    enum class ScopeKind
    {
        Root,
        For,
        If,
        Switch,
        Case
    };

    struct ValidationFrame
    {
        ScopeKind kind;
        std::map<std::string, TypeRef> vars;
        std::set<std::string> narrowedOptionals;
        std::set<std::string> absentOptionals;
        std::string optionalGuardPath;
        bool optionalGuardNegated = false;
        std::optional<TypeRef> switchExprType;
        bool switchCheckExhaustive = false;
        bool switchHasDefault = false;
        std::set<std::string> switchCaseTags;
        std::optional<std::string> alignSpec;
        int alignCellCount = 0;
        int openLineIndex = -1;
        LineSeg openSeg;
    };

    const EnumDef *findNearestSwitchEnum(const std::vector<ValidationFrame> &frames,
                                         const SemanticModel &types)
    {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it)
        {
            if (it->kind != ScopeKind::Switch || !it->switchExprType.has_value())
                continue;
            return types.resolve_enum(it->switchExprType.value());
        }
        return nullptr;
    }

    bool hasWholeEnumRenderViaOverload(const std::vector<const TemplateFunction *> &overloads,
                                       const TypeRef &enumType)
    {
        return overloads.size() == 1 && overloads[0]->params.size() == 1 && overloads[0]->params[0].type == enumType;
    }

    bool hasDirectRenderViaOverload(const std::vector<const TemplateFunction *> &overloads,
                                    const TypeRef &argType)
    {
        for (const auto *overload : overloads)
        {
            if (overload->params.size() == 1 && overload->params[0].type == argType)
                return true;
        }
        return false;
    }

    bool renderViaVariantCovered(const std::vector<const TemplateFunction *> &overloads,
                                 const TypeRef &enumType,
                                 const VariantDef &variant)
    {
        if (hasWholeEnumRenderViaOverload(overloads, enumType))
            return true;

        for (const auto *overload : overloads)
        {
            if (!variant.payload.has_value())
            {
                if (overload->params.empty())
                    return true;
                continue;
            }

            if (overload->params.size() == 1 && overload->params[0].type == variant.payload.value())
                return true;
        }

        return false;
    }

    std::string typeRefToString(const TypeRef &type)
    {
        return std::visit([&](const auto &arg) -> std::string
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, StringType>)
                return "string";
            else if constexpr (std::is_same_v<T, IntType>)
                return "int";
            else if constexpr (std::is_same_v<T, BoolType>)
                return "bool";
            else if constexpr (std::is_same_v<T, NamedType>)
                return arg.name;
            else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
                return "list<" + typeRefToString(arg->elementType) + ">";
            else
                return "optional<" + typeRefToString(arg->innerType) + ">";
        }, type);
    }

    bool lookupVariableType(const std::string &name,
                            const std::vector<ValidationFrame> &frames,
                            TypeRef &outType)
    {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it)
        {
            auto vt = it->vars.find(name);
            if (vt != it->vars.end())
            {
                outType = vt->second;
                return true;
            }
        }
        return false;
    }

    std::set<std::string> collectActiveOptionalNarrowing(const std::vector<ValidationFrame> &frames)
    {
        std::set<std::string> result;
        for (const auto &frame : frames)
            result.insert(frame.narrowedOptionals.begin(), frame.narrowedOptionals.end());
        return result;
    }

    std::set<std::string> collectActiveAbsentOptionals(const std::vector<ValidationFrame> &frames)
    {
        std::set<std::string> result;
        for (const auto &frame : frames)
            result.insert(frame.absentOptionals.begin(), frame.absentOptionals.end());
        return result;
    }

    bool validatePathExpressionType(const std::string &path,
                                    const std::vector<ValidationFrame> &frames,
                                    const SemanticModel &types,
                                    const std::set<std::string> &activeNarrowing,
                                    const std::set<std::string> &activeAbsent,
                                    bool allowOptionalTerminal,
                                    TypeRef &outType,
                                    std::string &error)
    {
        if (!isPathExpressionText(path))
        {
            error = "expression must be a variable or member path";
            return false;
        }

        std::vector<std::string> parts;
        size_t start = 0;
        while (start <= path.size())
        {
            size_t dot = path.find('.', start);
            if (dot == std::string::npos)
            {
                parts.push_back(path.substr(start));
                break;
            }
            parts.push_back(path.substr(start, dot - start));
            start = dot + 1;
        }

        TypeRef current;
        if (!lookupVariableType(parts[0], frames, current))
        {
            error = "unknown variable '" + parts[0] + "'";
            return false;
        }

        std::string currentPath = parts[0];
        for (size_t i = 1; i < parts.size(); ++i)
        {
            if (isOptionalType(current))
            {
                if (activeAbsent.count(currentPath) != 0)
                {
                    error = "optional value '" + currentPath + "' does not exist in this branch";
                    return false;
                }
                if (activeNarrowing.count(currentPath) == 0)
                {
                    error = "optional value '" + currentPath + "' must be guarded by '@if " + currentPath + "@'";
                    return false;
                }
                current = unwrapOptionalType(current);
            }

            const StructDef *sd = types.resolve_struct(current);
            if (!sd)
            {
                if (auto named = std::get_if<NamedType>(&current))
                {
                    if (!types.has_named_type(named->name))
                    {
                        error = "undefined type '" + named->name + "'";
                        return false;
                    }
                }
                error = "cannot access member '" + parts[i] + "' on non-struct value '" + currentPath + "'";
                return false;
            }

            bool found = false;
            for (const auto &fd : sd->fields)
            {
                if (fd.name == parts[i])
                {
                    current = fd.type;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                error = "Reference to undeclared member '" + parts[i] + "' of struct '" + sd->name + "'";
                return false;
            }

            currentPath += "." + parts[i];
        }

        if (isOptionalType(current) && !allowOptionalTerminal)
        {
            if (activeAbsent.count(currentPath) != 0)
            {
                error = "optional value '" + currentPath + "' does not exist in this branch";
                return false;
            }
            if (activeNarrowing.count(currentPath) == 0)
            {
                error = "optional value '" + currentPath + "' must be guarded by '@if " + currentPath + "@'";
                return false;
            }
            current = unwrapOptionalType(current);
        }

        outType = current;
        return true;
    }

    void addTemplateDiagnostic(std::vector<Diagnostic> &diags,
                               size_t bodyLine,
                               size_t lineIndex,
                               const LineSeg &seg,
                               const std::string &message,
                               int relStart = 0,
                               int relEnd = 0)
    {
        int start = seg.startCol - 1;
        int end = seg.endCol + 1;
        if (relEnd > relStart)
        {
            start = seg.startCol + relStart;
            end = seg.startCol + relEnd;
        }
        if (end < start)
            end = start;

        Diagnostic d;
        d.range = {{(int)(bodyLine + lineIndex), start}, {(int)(bodyLine + lineIndex), end}};
        d.message = message;
        d.severity = DiagnosticSeverity::Error;
        diags.push_back(std::move(d));
    }
}

void validateTemplateSemantics(const TemplateFunction &f,
                               const std::vector<TemplateLine> &templateLines,
                               size_t bodyStartLine,
                               const SemanticModel &types,
                               const PolicyRegistry &policies,
                               std::vector<Diagnostic> &diags,
                               const std::vector<TemplateFunction> &allFunctions,
                               int headerLine,
                               const std::string &headerText)
{
    auto validatePolicyTag = [&](const std::string &tag,
                                 size_t lineIndex,
                                 const LineSeg &seg)
    {
        if (tag.empty() || tag == "none" || policies.find(tag) != nullptr)
            return;

        const std::string needle = "policy=\"" + tag + "\"";
        size_t p = seg.text.find(needle);
        int start = 0;
        int end = 0;
        if (p != std::string::npos)
        {
            start = (int)p + 8;
            end = start + (int)tag.size();
        }
        addTemplateDiagnostic(diags, bodyStartLine, lineIndex, seg,
                              "unknown policy '" + tag + "'", start, end);
    };

    std::set<std::string> undefinedTypeVars;
    for (const auto &param : f.params)
    {
        if (auto undef = types.find_undefined_named_type(param.type))
        {
            int col = 0;
            if (!headerText.empty())
            {
                size_t p = headerText.find(*undef);
                if (p != std::string::npos)
                    col = (int)p;
            }

            Diagnostic d;
            d.range = {{headerLine, col}, {headerLine, col + (int)undef->size()}};
            d.message = "undefined type '" + *undef + "'";
            d.severity = DiagnosticSeverity::Error;
            diags.push_back(std::move(d));
            undefinedTypeVars.insert(param.name);
        }
    }

    if (!f.policy.empty() && f.policy != "none" && policies.find(f.policy) == nullptr)
    {
        int col = 0;
        if (!headerText.empty())
        {
            std::string needle = "policy=\"" + f.policy + "\"";
            size_t p = headerText.find(needle);
            if (p != std::string::npos)
                col = (int)p + 8;
        }

        Diagnostic d;
        d.range = {{headerLine, col}, {headerLine, col + (int)f.policy.size()}};
        d.message = "unknown policy '" + f.policy + "'";
        d.severity = DiagnosticSeverity::Error;
        diags.push_back(std::move(d));
    }

    std::vector<ValidationFrame> frames;
    ValidationFrame root{ScopeKind::Root};
    for (const auto &param : f.params)
        root.vars[param.name] = param.type;
    frames.push_back(std::move(root));

    auto popTo = [&](ScopeKind kind)
    {
        while (frames.size() > 1)
        {
            ScopeKind top = frames.back().kind;
            frames.pop_back();
            if (top == kind)
                break;
        }
    };

    for (size_t li = 0; li < templateLines.size(); ++li)
    {
        const auto &line = templateLines[li];
        for (const auto &seg : line.segments)
        {
            if (!seg.isDirective)
                continue;

            if (auto *e = std::get_if<ErrorDirective>(&seg.info))
            {
                addTemplateDiagnostic(diags, bodyStartLine, li, seg, e->message, e->start, e->end);
                continue;
            }

            auto activeNarrowing = collectActiveOptionalNarrowing(frames);
            auto activeAbsent = collectActiveAbsentOptionals(frames);
            std::string err;
            TypeRef exprType;

            if (auto *d = std::get_if<ExprDirective>(&seg.info))
            {
                validatePolicyTag(d->policy, li, seg);
                std::string path = expressionToPath(d->expr);
                if (undefinedTypeVars.count(path.substr(0, path.find('.'))) == 0)
                {
                    if (!validatePathExpressionType(path, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                }
            }
            else if (auto *d = std::get_if<FunctionCallDirective>(&seg.info))
            {
                for (const auto &argExpr : d->args)
                {
                    std::string path = expressionToPath(argExpr);
                    if (undefinedTypeVars.count(path.substr(0, path.find('.'))) != 0)
                        continue;
                    if (!validatePathExpressionType(path, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                        break;
                    }
                }
            }
            else if (auto *d = std::get_if<ForDirective>(&seg.info))
            {
                validatePolicyTag(d->policy, li, seg);
                for (const auto &[msg, s, e] : d->parseErrors)
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, msg, s, e);
                if (!validatePathExpressionType(d->collectionText, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                    continue;
                }
                if (!std::holds_alternative<std::shared_ptr<ListType>>(exprType))
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, "for collection must be a list");
                    continue;
                }

                auto listType = std::get<std::shared_ptr<ListType>>(exprType);
                ValidationFrame frame{ScopeKind::For};
                frame.vars[d->var] = listType->elementType;
                if (!d->enumerator.empty())
                    frame.vars[d->enumerator] = IntType{};
                frame.alignSpec = d->alignSpec;
                frame.openLineIndex = (int)li;
                frame.openSeg = seg;
                frames.push_back(std::move(frame));
            }
            else if (std::holds_alternative<EndForDirective>(seg.info))
            {
                for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                {
                    if (it->kind == ScopeKind::For)
                    {
                        if (it->alignSpec.has_value() && it->alignSpec->size() > 1)
                        {
                            size_t numCols = (size_t)it->alignCellCount + 1;
                            if (it->alignSpec->size() != numCols)
                            {
                                addTemplateDiagnostic(diags, bodyStartLine,
                                    it->openLineIndex, it->openSeg,
                                    "align spec has " + std::to_string(it->alignSpec->size()) +
                                    " column(s) but the loop body has " + std::to_string(numCols));
                            }
                        }
                        break;
                    }
                }
                popTo(ScopeKind::For);
            }
            else if (std::holds_alternative<AlignmentCellDirective>(seg.info))
            {
                bool inAlignFor = false;
                for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                {
                    if (it->kind == ScopeKind::For)
                    {
                        inAlignFor = it->alignSpec.has_value();
                        break;
                    }
                }
                if (!inAlignFor)
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg,
                        "@&@ is only allowed inside a for loop with the 'align' option");
                }
                else
                {
                    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                    {
                        if (it->kind == ScopeKind::For && it->alignSpec.has_value())
                        {
                            if (it->alignSpec->size() > 1 &&
                                it->alignCellCount >= (int)it->alignSpec->size() - 1)
                            {
                                addTemplateDiagnostic(diags, bodyStartLine, li, seg,
                                    "@&@ exceeds align spec — spec only covers " +
                                    std::to_string(it->alignSpec->size()) + " column(s)");
                            }
                            ++it->alignCellCount;
                            break;
                        }
                    }
                }
            }
            else if (std::holds_alternative<CommentDirective>(seg.info) ||
                     std::holds_alternative<EndCommentDirective>(seg.info))
            {
            }
            else if (auto *d = std::get_if<IfDirective>(&seg.info))
            {
                if (!validatePathExpressionType(d->condText, frames, types, activeNarrowing, activeAbsent, true, exprType, err))
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                    continue;
                }
                ValidationFrame frame{ScopeKind::If};
                if (d->negated)
                {
                    if (isOptionalType(exprType))
                    {
                        frame.optionalGuardPath = d->condText;
                        frame.optionalGuardNegated = true;
                        frame.absentOptionals.insert(d->condText);
                    }
                    else if (!std::holds_alternative<BoolType>(exprType))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "'not' is only allowed for bool or optional expressions");
                    }
                }
                else
                {
                    if (isOptionalType(exprType))
                    {
                        frame.optionalGuardPath = d->condText;
                        frame.optionalGuardNegated = false;
                        if (activeAbsent.count(d->condText) != 0)
                        {
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg,
                                "cannot guard optional value '" + d->condText + "' because it does not exist in this branch");
                        }
                        frame.narrowedOptionals.insert(d->condText);
                    }
                    else if (!std::holds_alternative<BoolType>(exprType))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "if condition must be bool or optional");
                    }
                }
                frame.openLineIndex = (int)li;
                frame.openSeg = seg;
                frames.push_back(std::move(frame));
            }
            else if (std::holds_alternative<ElseDirective>(seg.info))
            {
                for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                {
                    if (it->kind == ScopeKind::If)
                    {
                        it->narrowedOptionals.clear();
                        it->absentOptionals.clear();
                        if (!it->optionalGuardPath.empty())
                        {
                            if (it->optionalGuardNegated)
                                it->narrowedOptionals.insert(it->optionalGuardPath);
                            else
                                it->absentOptionals.insert(it->optionalGuardPath);
                        }
                        break;
                    }
                }
            }
            else if (std::holds_alternative<DefaultDirective>(seg.info))
            {
                ValidationFrame frame{ScopeKind::Case};
                bool foundSwitch = false;
                bool duplicateDefault = false;
                for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                {
                    if (it->kind != ScopeKind::Switch)
                        continue;
                    foundSwitch = true;
                    if (it->switchHasDefault)
                        duplicateDefault = true;
                    it->switchHasDefault = true;
                    break;
                }
                if (!foundSwitch)
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, "default directive must be inside a switch");
                else if (duplicateDefault)
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, "switch may contain at most one default branch");
                frame.openLineIndex = (int)li;
                frame.openSeg = seg;
                frames.push_back(std::move(frame));
            }
            else if (std::holds_alternative<EndIfDirective>(seg.info))
            {
                popTo(ScopeKind::If);
            }
            else if (auto *d = std::get_if<SwitchDirective>(&seg.info))
            {
                validatePolicyTag(d->policy, li, seg);
                std::string switchPath = expressionToPath(d->expr);
                if (!validatePathExpressionType(switchPath, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);

                ValidationFrame frame{ScopeKind::Switch};
                frame.switchExprType = exprType;
                frame.switchCheckExhaustive = d->checkExhaustive;
                frame.openLineIndex = (int)li;
                frame.openSeg = seg;
                frames.push_back(std::move(frame));
            }
            else if (std::holds_alternative<EndSwitchDirective>(seg.info))
            {
                for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                {
                    if (it->kind != ScopeKind::Switch)
                        continue;
                    if (it->switchCheckExhaustive)
                    {
                        const EnumDef *switchEnum = nullptr;
                        if (it->switchExprType.has_value())
                            switchEnum = types.resolve_enum(it->switchExprType.value());
                        if (switchEnum)
                        {
                            std::vector<std::string> missing;
                            for (const auto &variant : switchEnum->variants)
                            {
                                if (it->switchCaseTags.count(variant.tag) == 0 && !it->switchHasDefault)
                                    missing.push_back(variant.tag);
                            }
                            if (!missing.empty())
                            {
                                std::string missingText;
                                for (size_t i = 0; i < missing.size(); ++i)
                                {
                                    if (i > 0)
                                        missingText += ", ";
                                    missingText += missing[i];
                                }
                                addTemplateDiagnostic(diags, bodyStartLine, li, seg,
                                    "switch is missing cases for enum '" + switchEnum->name + "': " + missingText);
                            }
                        }
                    }
                    break;
                }
                popTo(ScopeKind::Switch);
            }
            else if (auto *d = std::get_if<CaseDirective>(&seg.info))
            {
                ValidationFrame frame{ScopeKind::Case};
                const EnumDef *switchEnum = findNearestSwitchEnum(frames, types);
                const VariantDef *variant = types.find_variant(switchEnum, d->tag);
                for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                {
                    if (it->kind == ScopeKind::Switch)
                    {
                        if (it->switchHasDefault)
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, "default branch must be the last branch in a switch");
                        auto [_, inserted] = it->switchCaseTags.insert(d->tag);
                        if (!inserted)
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, "duplicate case tag '" + d->tag + "'");
                        break;
                    }
                }
                if (!switchEnum)
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, "case directive must be inside a switch over an enum");
                }
                else if (!variant)
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, "unknown case tag '" + d->tag + "' for enum '" + switchEnum->name + "'");
                }
                else if (!d->binding.empty())
                {
                    if (!variant->payload.has_value())
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "case tag '" + d->tag + "' has no payload to bind");
                    else
                        frame.vars[d->binding] = variant->payload.value();
                }
                frame.openLineIndex = (int)li;
                frame.openSeg = seg;
                frames.push_back(std::move(frame));
            }
            else if (std::holds_alternative<EndCaseDirective>(seg.info))
            {
                popTo(ScopeKind::Case);
            }
            else if (auto *d = std::get_if<RenderDirective>(&seg.info))
            {
                for (const auto &[msg, s, e] : d->parseErrors)
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, msg, s, e);
                validatePolicyTag(d->policy, li, seg);

                const EnumDef *switchEnum = findNearestSwitchEnum(frames, types);
                const VariantDef *variant = types.find_variant(switchEnum, d->exprText);
                if (switchEnum && variant)
                {
                    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                    {
                        if (it->kind != ScopeKind::Switch)
                            continue;
                        if (it->switchHasDefault)
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, "default branch must be the last branch in a switch");
                        auto [_, inserted] = it->switchCaseTags.insert(d->exprText);
                        if (!inserted)
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, "duplicate case tag '" + d->exprText + "'");
                        break;
                    }
                    continue;
                }

                if (!validatePathExpressionType(d->exprText, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                    continue;
                }

                TypeRef renderElemType = exprType;
                if (auto listType = std::get_if<std::shared_ptr<ListType>>(&exprType))
                    renderElemType = (*listType)->elementType;
                else if (!types.resolve_enum(exprType))
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, "render expression must be a list or enum");
                    continue;
                }

                const EnumDef *renderEnum = types.resolve_enum(renderElemType);
                auto overloads = SemanticModel::find_template_overloads(allFunctions, d->func);
                if (!renderEnum)
                {
                    if (!hasDirectRenderViaOverload(overloads, renderElemType))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg,
                            "render-via function '" + d->func + "' must have an overload taking '" + typeRefToString(renderElemType) + "'");
                    }
                    continue;
                }

                std::vector<std::string> missing;
                for (const auto &renderVariant : renderEnum->variants)
                {
                    if (!renderViaVariantCovered(overloads, renderElemType, renderVariant))
                        missing.push_back(renderVariant.tag);
                }

                if (!missing.empty())
                {
                    std::string missingText;
                    for (size_t i = 0; i < missing.size(); ++i)
                    {
                        if (i > 0)
                            missingText += ", ";
                        missingText += missing[i];
                    }
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg,
                        "render-via function '" + d->func + "' is missing overloads for enum '" + renderEnum->name + "': " + missingText);
                }
            }
        }
    }

    for (size_t i = 1; i < frames.size(); ++i)
    {
        const auto &frame = frames[i];
        if (frame.openLineIndex < 0)
            continue;

        std::string msg;
        switch (frame.kind)
        {
        case ScopeKind::For:
            msg = "missing @end for@";
            break;
        case ScopeKind::Switch:
            msg = "missing @end switch@";
            break;
        case ScopeKind::Case:
            msg = "missing @end case@";
            break;
        case ScopeKind::If:
            msg = "missing @end if@";
            break;
        default:
            break;
        }
        if (!msg.empty())
            addTemplateDiagnostic(diags, bodyStartLine, frame.openLineIndex, frame.openSeg, msg);
    }
}

} // namespace tpp::compiler