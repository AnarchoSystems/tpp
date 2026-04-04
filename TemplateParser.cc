#include <tpp/Compiler.h>
#include <tpp/CompilerOutput.h>
#include <tpp/TemplateParser.h>
#include <tpp/Diagnostic.h>
#include <sstream>
#include <functional>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace tpp
{

    // ═══════════════════════════════════════════════════════════════════
    // Template Parser  (compile)
    // ═══════════════════════════════════════════════════════════════════

    std::vector<Segment> tokenize_template(const std::string &src)
    {
        std::vector<Segment> segs;
        size_t i = 0;
        while (i < src.size())
        {
            size_t at = src.find('@', i);
            if (at == std::string::npos)
            {
                segs.push_back({false, src.substr(i), (int)i, (int)src.size()});
                break;
            }
            if (at > i)
            {
                segs.push_back({false, src.substr(i, at - i), (int)i, (int)at});
            }
            size_t end = src.find('@', at + 1);
            if (end == std::string::npos)
            {
                segs.push_back({false, src.substr(at), (int)at, (int)src.size()});
                break;
            }
            if (end == at + 1)
            {
                // Empty @@ pair — skip it; re-examine 'end' as potential opener
                i = end;
                continue;
            }
            segs.push_back({true, src.substr(at + 1, end - at - 1), (int)(at + 1), (int)end});
            i = end + 1;
        }
        return segs;
    }

    static std::string trim(const std::string &s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static bool startsWith(const std::string &s, const std::string &prefix)
    {
        return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
    }

    Expression parseExpression(const std::string &s)
    {
        std::string trimmed = trim(s);
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i <= trimmed.size(); ++i)
        {
            if (i == trimmed.size() || trimmed[i] == '.')
            {
                parts.push_back(trimmed.substr(start, i - start));
                start = i + 1;
            }
        }
        Expression expr = Variable{parts[0]};
        for (size_t i = 1; i < parts.size(); ++i)
        {
            expr = std::make_shared<FieldAccess>(FieldAccess{expr, parts[i]});
        }
        return expr;
    }

    static std::string parseStringLiteral(const std::string &s, size_t &pos)
    {
        std::string result;
        if (pos >= s.size() || s[pos] != '"')
            return result;
        ++pos;
        while (pos < s.size() && s[pos] != '"')
        {
            if (s[pos] == '\\' && pos + 1 < s.size())
            {
                char next = s[pos + 1];
                if (next == '"')
                {
                    result += '"';
                    pos += 2;
                }
                else if (next == '\\')
                {
                    result += '\\';
                    pos += 2;
                }
                else if (next == 'n')
                {
                    result += '\n';
                    pos += 2;
                }
                else if (next == 't')
                {
                    result += '\t';
                    pos += 2;
                }
                else
                {
                    result += next;
                    pos += 2;
                }
            }
            else
            {
                result += s[pos];
                ++pos;
            }
        }
        if (pos < s.size())
            ++pos;
        return result;
    }

    static bool isIdentStart(char c)
    {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    static bool isIdentChar(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    static bool isIdentifier(const std::string &s)
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

    static bool isPathExpressionText(const std::string &s)
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

    struct ParseError { std::string message; int start = 0, end = 0; };

    // Parses key=value option pairs. Returns an error with position info, or nullopt on success.
    static std::optional<ParseError> parseDirectiveOptions(
        const std::string &optPart,
        const std::set<std::string> &allowedStringKeys,
        const std::set<std::string> &allowedIdentKeys,
        std::map<std::string, std::string> &outValues)
    {
        size_t p = 0;
        std::set<std::string> seen;
        while (p < optPart.size())
        {
            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;
            if (p >= optPart.size())
                break;

            size_t keyStart = p;
            while (p < optPart.size() && optPart[p] != '=' && !std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;
            std::string key = trim(optPart.substr(keyStart, p - keyStart));
            if (!isIdentifier(key))
                return ParseError{"invalid option key '" + key + "'", (int)keyStart, (int)p};

            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;
            if (p >= optPart.size() || optPart[p] != '=')
                return ParseError{"expected '=' after option key '" + key + "'", (int)keyStart, (int)p};
            ++p;
            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;

            if (seen.count(key))
                return ParseError{"duplicate option '" + key + "'", (int)keyStart, (int)p};
            seen.insert(key);

            if (allowedStringKeys.count(key) == 0 && allowedIdentKeys.count(key) == 0)
                return ParseError{"unknown option '" + key + "'", (int)keyStart, (int)p};

            std::string value;
            if (allowedStringKeys.count(key) != 0)
            {
                size_t before = p;
                value = parseStringLiteral(optPart, p);
                if (before == p)
                    return ParseError{"option '" + key + "' expects a quoted string value", (int)keyStart, (int)p};
            }
            else
            {
                size_t valueStart = p;
                while (p < optPart.size() && !std::isspace(static_cast<unsigned char>(optPart[p])))
                    ++p;
                value = trim(optPart.substr(valueStart, p - valueStart));
                if (!isIdentifier(value))
                    return ParseError{"option '" + key + "' expects an identifier value", (int)keyStart, (int)p};
            }

            outValues[key] = value;
        }
        return std::nullopt;
    }

    // Parses space-separated flag keywords. Returns an error with position info, or nullopt on success.
    static std::optional<ParseError> parseDirectiveFlags(
        const std::string &optPart,
        const std::set<std::string> &allowedFlags,
        std::set<std::string> &outFlags)
    {
        size_t p = 0;
        while (p < optPart.size())
        {
            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;
            if (p >= optPart.size())
                break;

            size_t start = p;
            while (p < optPart.size() && !std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;

            std::string flag = trim(optPart.substr(start, p - start));
            if (!isIdentifier(flag))
                return ParseError{"invalid option '" + flag + "'", (int)start, (int)p};
            if (allowedFlags.count(flag) == 0)
                return ParseError{"unknown option '" + flag + "'", (int)start, (int)p};
            if (outFlags.count(flag) != 0)
                return ParseError{"duplicate option '" + flag + "'", (int)start, (int)p};
            outFlags.insert(flag);
        }
        return std::nullopt;
    }

    DirectiveInfo classifyDirective(const std::string &s)
    {
        std::string t = trim(s);

        if (t == "else")       return ElseDirective{};
        if (t == "endif")      return EndIfDirective{};
        if (t == "end for")    return EndForDirective{};
        if (t == "end switch") return EndSwitchDirective{};

        if (startsWith(t, "end case"))
        {
            if (!trim(t.substr(8)).empty())
                return ErrorDirective{"unexpected trailing tokens in end case directive"};
            return EndCaseDirective{};
        }

        if (startsWith(t, "for "))
        {
            std::string mainPart = t;
            std::string optPart;
            size_t pipe = t.find('|');
            if (pipe != std::string::npos)
            {
                mainPart = trim(t.substr(0, pipe));
                optPart  = trim(t.substr(pipe + 1));
                if (optPart.empty())
                    return ErrorDirective{"for options list is empty after '|'"};
            }
            std::string rest = mainPart.substr(4);
            size_t inPos = rest.find(" in ");
            if (inPos == std::string::npos)
                return ErrorDirective{"invalid for directive; expected 'for <item> in <collection>'"};

            ForDirective d;
            d.var            = trim(rest.substr(0, inPos));
            d.collectionText = trim(rest.substr(inPos + 4));
            d.collection     = parseExpression(d.collectionText);

            if (!isIdentifier(d.var))
                return ErrorDirective{"invalid loop variable name '" + d.var + "'"};
            if (!isPathExpressionText(d.collectionText))
                return ErrorDirective{"for collection must be a variable or member path"};
            if (pipe == std::string::npos)
            {
                if (rest.find("sep=") != std::string::npos ||
                    rest.find("followedBy=") != std::string::npos ||
                    rest.find("precededBy=") != std::string::npos ||
                    rest.find("iteratorVar=") != std::string::npos)
                    return ErrorDirective{"for options require '|' before option list"};
            }
            if (!optPart.empty())
            {
                std::map<std::string, std::string> values;
                if (auto err = parseDirectiveOptions(optPart, {"sep", "followedBy", "precededBy"}, {"iteratorVar"}, values))
                    return ErrorDirective{err->message, err->start, err->end};
                d.sep         = values["sep"];
                d.followedBy  = values["followedBy"];
                d.precededBy  = values["precededBy"];
                d.iteratorVar = values["iteratorVar"];
            }
            return d;
        }

        if (startsWith(t, "if "))
        {
            std::string condText = trim(t.substr(3));
            IfDirective d;
            if (startsWith(condText, "not "))
            {
                d.negated = true;
                condText  = trim(condText.substr(4));
            }
            d.condText = condText;
            if (!isPathExpressionText(condText))
                return ErrorDirective{"if condition must be a variable or member path"};
            d.cond = parseExpression(condText);
            return d;
        }

        if (startsWith(t, "switch "))
        {
            std::string rest     = trim(t.substr(7));
            std::string exprText = rest;
            std::string optPart;
            size_t pipe = rest.find('|');
            if (pipe != std::string::npos)
            {
                exprText = trim(rest.substr(0, pipe));
                optPart  = trim(rest.substr(pipe + 1));
                if (optPart.empty())
                    return ErrorDirective{"switch options list is empty after '|'"};
            }
            if (exprText.empty())
                return ErrorDirective{"switch directive requires an expression"};
            if (!isPathExpressionText(exprText))
                return ErrorDirective{"switch expression must be a variable or member path"};
            SwitchDirective d;
            d.expr = parseExpression(exprText);
            if (!optPart.empty())
            {
                std::set<std::string> flags;
                if (auto err = parseDirectiveFlags(optPart, {"checkExhaustive"}, flags))
                    return ErrorDirective{err->message, err->start, err->end};
                d.checkExhaustive = flags.count("checkExhaustive") != 0;
            }
            return d;
        }

        if (startsWith(t, "case "))
        {
            std::string rest = trim(t.substr(5));
            if (rest.find('|') != std::string::npos)
                return ErrorDirective{"case directive does not accept options"};
            CaseDirective d;
            size_t paren = rest.find('(');
            if (paren != std::string::npos)
            {
                d.tag = trim(rest.substr(0, paren));
                size_t cparen = rest.find(')', paren);
                if (cparen == std::string::npos)
                    return ErrorDirective{"case binding is missing closing ')'"};
                d.binding = rest.substr(paren + 1, cparen - paren - 1);
                if (!trim(rest.substr(cparen + 1)).empty())
                    return ErrorDirective{"unexpected trailing tokens in case directive"};
                if (!trim(d.binding).empty() && !isIdentifier(trim(d.binding)))
                    return ErrorDirective{"invalid case binding variable name"};
            }
            else
            {
                d.tag = rest;
            }
            if (!isIdentifier(d.tag))
                return ErrorDirective{"invalid case tag '" + d.tag + "'"};
            d.binding = trim(d.binding);
            return d;
        }

        if (startsWith(t, "render "))
        {
            std::string rest     = t.substr(7);
            std::string mainPart = rest;
            std::string optPart;
            size_t pipe = rest.find('|');
            if (pipe != std::string::npos)
            {
                mainPart = trim(rest.substr(0, pipe));
                optPart  = trim(rest.substr(pipe + 1));
                if (optPart.empty())
                    return ErrorDirective{"render options list is empty after '|'"};
            }
            size_t viaPos = mainPart.find(" via ");
            if (viaPos == std::string::npos)
                return ErrorDirective{"invalid render directive; expected 'render <collection> via <function>'"};
            RenderDirective d;
            d.exprText = trim(mainPart.substr(0, viaPos));
            d.expr     = parseExpression(d.exprText);
            d.func     = trim(mainPart.substr(viaPos + 5));
            if (!isPathExpressionText(d.exprText))
                return ErrorDirective{"render expression must be a variable or member path"};
            if (!isIdentifier(d.func))
                return ErrorDirective{"invalid render function name '" + d.func + "'"};
            if (pipe == std::string::npos)
            {
                if (rest.find("sep=") != std::string::npos ||
                    rest.find("followedBy=") != std::string::npos ||
                    rest.find("precededBy=") != std::string::npos)
                    return ErrorDirective{"render options require '|' before option list"};
            }
            if (!optPart.empty())
            {
                std::map<std::string, std::string> values;
                if (auto err = parseDirectiveOptions(optPart, {"sep", "followedBy", "precededBy"}, {}, values))
                    return ErrorDirective{err->message, err->start, err->end};
                d.sep        = values["sep"];
                d.followedBy = values["followedBy"];
                d.precededBy = values["precededBy"];
            }
            return d;
        }

        // Detect function call: name(arg1, arg2, ...)
        {
            size_t parenOpen = t.find('(');
            if (parenOpen != std::string::npos && t.back() == ')')
            {
                std::string name = trim(t.substr(0, parenOpen));
                bool validName = !name.empty();
                for (char c : name)
                    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                        { validName = false; break; }
                if (validName)
                {
                    FunctionCallDirective d;
                    d.name = name;
                    std::string argsStr = t.substr(parenOpen + 1, t.size() - parenOpen - 2);
                    if (!trim(argsStr).empty())
                    {
                        std::vector<std::string> argParts;
                        int depth = 0;
                        std::string current;
                        for (char c : argsStr)
                        {
                            if      (c == '(')           { depth++; current += c; }
                            else if (c == ')')           { depth--; current += c; }
                            else if (c == ',' && depth == 0) { argParts.push_back(current); current.clear(); }
                            else                         { current += c; }
                        }
                        if (!current.empty())
                            argParts.push_back(current);
                        for (auto &arg : argParts)
                            d.args.push_back(parseExpression(arg));
                    }
                    return d;
                }
            }
        }

        return ExprDirective{parseExpression(t)};
    }

    // isStructuralDirective is now an inline function in Parser.h

    std::vector<TemplateLine> parseTemplateLines(const std::string &body)
    {
        std::vector<std::string> rawLines;
        std::istringstream iss(body);
        std::string line;
        while (std::getline(iss, line))
        {
            rawLines.push_back(line);
        }

        std::vector<TemplateLine> result;
        for (auto &raw : rawLines)
        {
            TemplateLine tl;
            auto segs = tokenize_template(raw);
            bool hasText = false;
            bool hasExprDirective = false;
            bool hasStructural = false;

            for (auto &seg : segs)
            {
                LineSeg ls;
                ls.isDirective = seg.isDirective;
                ls.text = seg.text;
                ls.startCol = seg.startCol;
                ls.endCol = seg.endCol;
                if (seg.isDirective)
                {
                    ls.info = classifyDirective(seg.text);
                    if (std::holds_alternative<ExprDirective>(ls.info) ||
                        std::holds_alternative<FunctionCallDirective>(ls.info))
                    {
                        hasExprDirective = true;
                    }
                    else
                    {
                        hasStructural = true;
                    }
                }
                else
                {
                    if (trim(seg.text) != "")
                    {
                        hasText = true;
                    }
                }
                tl.segments.push_back(std::move(ls));
            }

            tl.isBlockLine = hasStructural && !hasText && !hasExprDirective;

            size_t ws = 0;
            while (ws < raw.size() && (raw[ws] == ' ' || raw[ws] == '\t'))
                ws++;
            tl.indent = (int)ws;

            result.push_back(std::move(tl));
        }
        return result;
    }

    std::vector<ASTNode> ASTBuilder::parseInline(const std::vector<LineSeg> &segs, size_t startPos)
    {
        std::vector<ASTNode> nodes;
        size_t pos = startPos;

        // Helper to recurse into a sub-range (e.g. for/if body on same line)
        std::function<std::vector<ASTNode>()> recurse = [&]() -> std::vector<ASTNode>
        {
            std::vector<ASTNode> sub;
            while (pos < segs.size())
            {
                auto &s = segs[pos];
                if (!s.isDirective)
                {
                    if (!s.text.empty())
                        sub.push_back(TextNode{s.text});
                    ++pos;
                }
                else if (auto *d = std::get_if<ExprDirective>(&s.info))
                {
                    sub.push_back(InterpolationNode{d->expr});
                    ++pos;
                }
                else if (auto *d = std::get_if<FunctionCallDirective>(&s.info))
                {
                    auto callNode = std::make_shared<FunctionCallNode>();
                    callNode->functionName = d->name;
                    callNode->arguments    = d->args;
                    sub.push_back(std::move(callNode));
                    ++pos;
                }
                else if (auto *d = std::get_if<ForDirective>(&s.info))
                {
                    auto forNode = std::make_shared<ForNode>();
                    forNode->varName         = d->var;
                    forNode->iteratorVarName = d->iteratorVar;
                    forNode->collectionExpr  = d->collection;
                    forNode->sep             = d->sep;
                    forNode->followedBy      = d->followedBy;
                    forNode->precededBy      = d->precededBy;
                    forNode->isBlock         = false;
                    ++pos;
                    forNode->body = recurse();
                    sub.push_back(std::move(forNode));
                }
                else if (std::holds_alternative<EndForDirective>(s.info))
                {
                    ++pos;
                    return sub;
                }
                else if (auto *d = std::get_if<IfDirective>(&s.info))
                {
                    auto ifNode = std::make_shared<IfNode>();
                    ifNode->condExpr = d->cond;
                    ifNode->negated  = d->negated;
                    ifNode->condText = d->condText;
                    ifNode->isBlock  = false;
                    ++pos;
                    ifNode->thenBody = recurse();
                    if (pos < segs.size() && segs[pos].isDirective &&
                        std::holds_alternative<ElseDirective>(segs[pos].info))
                    {
                        ++pos;
                        ifNode->elseBody = recurse();
                    }
                    sub.push_back(std::move(ifNode));
                }
                else if (std::holds_alternative<ElseDirective>(s.info))
                {
                    return sub;
                }
                else if (std::holds_alternative<EndIfDirective>(s.info))
                {
                    ++pos;
                    return sub;
                }
                else if (auto *d = std::get_if<SwitchDirective>(&s.info))
                {
                    auto switchNode = std::make_shared<SwitchNode>();
                    switchNode->expr            = d->expr;
                    switchNode->checkExhaustive = d->checkExhaustive;
                    switchNode->isBlock         = false;
                    ++pos;
                    while (pos < segs.size())
                    {
                        auto &cs = segs[pos];
                        if (!cs.isDirective) { ++pos; continue; }
                        if (std::holds_alternative<EndSwitchDirective>(cs.info)) { ++pos; break; }
                        if (auto *cd = std::get_if<CaseDirective>(&cs.info))
                        {
                            CaseNode cn;
                            cn.tag         = cd->tag;
                            cn.bindingName = cd->binding;
                            ++pos;
                            cn.body = recurse();
                            switchNode->cases.push_back(std::move(cn));
                        }
                        else
                        {
                            ++pos;
                        }
                    }
                    sub.push_back(std::move(switchNode));
                }
                else if (std::holds_alternative<EndCaseDirective>(s.info))
                {
                    ++pos;
                    return sub;
                }
                else if (std::holds_alternative<EndSwitchDirective>(s.info))
                {
                    return sub;
                }
                else
                {
                    ++pos;
                }
            }
            return sub;
        };

        nodes = recurse();
        return nodes;
    }

    std::vector<ASTNode> ASTBuilder::parseBlock(int insertCol)
    {
        std::vector<ASTNode> nodes;
        while (pos < lines.size())
        {
            auto &tl = lines[pos];

            if (tl.isBlockLine)
            {
                for (auto &seg : tl.segments)
                {
                    if (!seg.isDirective)
                        continue;
                    if (auto *d = std::get_if<ForDirective>(&seg.info))
                    {
                        auto forNode = std::make_shared<ForNode>();
                        forNode->varName         = d->var;
                        forNode->iteratorVarName = d->iteratorVar;
                        forNode->collectionExpr  = d->collection;
                        forNode->sep             = d->sep;
                        forNode->followedBy      = d->followedBy;
                        forNode->precededBy      = d->precededBy;
                        forNode->isBlock         = true;
                        forNode->insertCol       = tl.indent;
                        ++pos;
                        forNode->body = parseBlock(tl.indent);
                        nodes.push_back(std::move(forNode));
                    }
                    else if (std::holds_alternative<EndForDirective>(seg.info))
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (auto *d = std::get_if<IfDirective>(&seg.info))
                    {
                        auto ifNode = std::make_shared<IfNode>();
                        ifNode->condExpr  = d->cond;
                        ifNode->negated   = d->negated;
                        ifNode->condText  = d->condText;
                        ifNode->isBlock   = true;
                        ifNode->insertCol = tl.indent;
                        ++pos;
                        ifNode->thenBody = parseBlock(tl.indent);
                        if (pos < lines.size() && lines[pos].isBlockLine)
                        {
                            for (auto &s2 : lines[pos].segments)
                            {
                                if (s2.isDirective && std::holds_alternative<ElseDirective>(s2.info))
                                {
                                    ++pos;
                                    ifNode->elseBody = parseBlock(tl.indent);
                                    break;
                                }
                            }
                        }
                        nodes.push_back(std::move(ifNode));
                    }
                    else if (std::holds_alternative<ElseDirective>(seg.info))
                    {
                        return nodes;
                    }
                    else if (std::holds_alternative<EndIfDirective>(seg.info))
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (auto *d = std::get_if<SwitchDirective>(&seg.info))
                    {
                        auto switchNode = std::make_shared<SwitchNode>();
                        switchNode->expr            = d->expr;
                        switchNode->checkExhaustive = d->checkExhaustive;
                        switchNode->isBlock         = true;
                        switchNode->insertCol       = tl.indent;
                        ++pos;
                        while (pos < lines.size())
                        {
                            if (lines[pos].isBlockLine)
                            {
                                bool done = false;
                                for (auto &s2 : lines[pos].segments)
                                {
                                    if (!s2.isDirective)
                                        continue;
                                    if (std::holds_alternative<EndSwitchDirective>(s2.info))
                                    {
                                        ++pos;
                                        done = true;
                                        break;
                                    }
                                    if (auto *cd = std::get_if<CaseDirective>(&s2.info))
                                    {
                                        CaseNode cn;
                                        cn.tag         = cd->tag;
                                        cn.bindingName = cd->binding;
                                        ++pos;
                                        cn.body = parseBlock(tl.indent);
                                        switchNode->cases.push_back(std::move(cn));
                                        break;
                                    }
                                    if (auto *rd = std::get_if<RenderDirective>(&s2.info))
                                    {
                                        CaseNode cn;
                                        cn.tag         = rd->exprText;
                                        cn.bindingName = "__payload";
                                        auto callNode  = std::make_shared<FunctionCallNode>();
                                        callNode->functionName = rd->func;
                                        callNode->arguments.push_back(Variable{"__payload"});
                                        cn.body.push_back(std::move(callNode));
                                        switchNode->cases.push_back(std::move(cn));
                                        ++pos;
                                        break;
                                    }
                                    // ErrorDirective or unrecognized on a block line inside switch: skip
                                    ++pos;
                                    break;
                                }
                                if (done)
                                    break;
                            }
                            else
                            {
                                ++pos;
                            }
                        }
                        nodes.push_back(std::move(switchNode));
                    }
                    else if (std::holds_alternative<EndSwitchDirective>(seg.info))
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (std::holds_alternative<CaseDirective>(seg.info))
                    {
                        return nodes;
                    }
                    else if (std::holds_alternative<EndCaseDirective>(seg.info))
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (auto *d = std::get_if<RenderDirective>(&seg.info))
                    {
                        auto renderNode = std::make_shared<RenderViaNode>();
                        renderNode->collectionExpr = d->expr;
                        renderNode->functionName   = d->func;
                        renderNode->sep            = d->sep;
                        renderNode->followedBy     = d->followedBy;
                        renderNode->precededBy     = d->precededBy;
                        renderNode->isBlock        = true;
                        renderNode->insertCol      = tl.indent;
                        nodes.push_back(std::move(renderNode));
                        ++pos;
                    }
                    else
                    {
                        // ErrorDirective or any unrecognized directive on a block line: skip it
                        ++pos;
                    }
                    break;
                }
            }
            else
            {
                // Non-block line: text/expr line, possibly with inline structural directives
                bool hasInline = false;
                for (auto &s : tl.segments)
                {
                    if (s.isDirective && isStructuralDirective(s.info))
                    {
                        hasInline = true;
                        break;
                    }
                }
                if (hasInline)
                {
                    auto inlineNodes = parseInline(tl.segments);
                    for (auto &n : inlineNodes)
                        nodes.push_back(std::move(n));
                    nodes.push_back(TextNode{"\n"});
                }
                else
                {
                    for (auto &seg : tl.segments)
                    {
                        if (!seg.isDirective)
                        {
                            if (!seg.text.empty())
                                nodes.push_back(TextNode{seg.text});
                        }
                        else if (auto *d = std::get_if<FunctionCallDirective>(&seg.info))
                        {
                            auto callNode = std::make_shared<FunctionCallNode>();
                            callNode->functionName = d->name;
                            callNode->arguments    = d->args;
                            nodes.push_back(std::move(callNode));
                        }
                        else if (auto *d = std::get_if<ExprDirective>(&seg.info))
                        {
                            nodes.push_back(InterpolationNode{d->expr});
                        }
                    }
                    nodes.push_back(TextNode{"\n"});
                }
                ++pos;
            }
        }
        return nodes;
    }

    static bool isOptionalType(const TypeRef &t)
    {
        return std::holds_alternative<std::shared_ptr<OptionalType>>(t);
    }

    static TypeRef unwrapOptionalType(const TypeRef &t)
    {
        if (auto opt = std::get_if<std::shared_ptr<OptionalType>>(&t))
            return (*opt)->innerType;
        return t;
    }

    static std::string expressionToPath(const Expression &expr)
    {
        return std::visit([&](auto &&arg) -> std::string
                          {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Variable>) {
                return arg.name;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<FieldAccess>>) {
                std::string base = expressionToPath(arg->base);
                if (base.empty())
                    return arg->field;
                return base + "." + arg->field;
            }
            return ""; },
                          expr);
    }

    enum class ScopeKind { Root, For, If, Switch, Case };

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
        std::set<std::string> switchCaseTags;
        int openLineIndex = -1;
        LineSeg openSeg;
    };

    static const StructDef *resolveStructFromType(const TypeRef &type, const TypeRegistry &types)
    {
        if (auto named = std::get_if<NamedType>(&type))
        {
            auto it = types.nameIndex.find(named->name);
            if (it != types.nameIndex.end() && it->second.kind == TypeKind::Struct)
                return &types.structs[it->second.index];
            return nullptr;
        }
        if (auto opt = std::get_if<std::shared_ptr<OptionalType>>(&type))
            return resolveStructFromType((*opt)->innerType, types);
        if (auto list = std::get_if<std::shared_ptr<ListType>>(&type))
            return resolveStructFromType((*list)->elementType, types);
        return nullptr;
    }

    static const EnumDef *resolveEnumFromType(const TypeRef &type, const TypeRegistry &types)
    {
        if (auto named = std::get_if<NamedType>(&type))
        {
            auto it = types.nameIndex.find(named->name);
            if (it != types.nameIndex.end() && it->second.kind == TypeKind::Enum)
                return &types.enums[it->second.index];
            return nullptr;
        }
        if (auto opt = std::get_if<std::shared_ptr<OptionalType>>(&type))
            return resolveEnumFromType((*opt)->innerType, types);
        return nullptr;
    }

    static const VariantDef *findEnumVariant(const EnumDef *ed, const std::string &tag)
    {
        if (!ed)
            return nullptr;
        for (const auto &variant : ed->variants)
        {
            if (variant.tag == tag)
                return &variant;
        }
        return nullptr;
    }

    static const EnumDef *findNearestSwitchEnum(const std::vector<ValidationFrame> &frames,
                                                const TypeRegistry &types)
    {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it)
        {
            if (it->kind != ScopeKind::Switch || !it->switchExprType.has_value())
                continue;
            return resolveEnumFromType(it->switchExprType.value(), types);
        }
        return nullptr;
    }

    static bool lookupVariableType(const std::string &name,
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

    static std::set<std::string> collectActiveOptionalNarrowing(const std::vector<ValidationFrame> &frames)
    {
        std::set<std::string> result;
        for (const auto &frame : frames)
        {
            result.insert(frame.narrowedOptionals.begin(), frame.narrowedOptionals.end());
        }
        return result;
    }

    static std::set<std::string> collectActiveAbsentOptionals(const std::vector<ValidationFrame> &frames)
    {
        std::set<std::string> result;
        for (const auto &frame : frames)
        {
            result.insert(frame.absentOptionals.begin(), frame.absentOptionals.end());
        }
        return result;
    }

    static bool validatePathExpressionType(const std::string &path,
                                           const std::vector<ValidationFrame> &frames,
                                           const TypeRegistry &types,
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

            const StructDef *sd = resolveStructFromType(current, types);
            if (!sd)
            {
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

    static void addTemplateDiagnostic(std::vector<Diagnostic> &diags,
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

    static void validateTemplateSemantics(const TemplateFunction &f,
                                          const std::vector<TemplateLine> &templateLines,
                                          size_t bodyStartLine,
                                          const TypeRegistry &types,
                                          std::vector<Diagnostic> &diags)
    {
        std::vector<ValidationFrame> frames;
        ValidationFrame root{ScopeKind::Root};
        for (const auto &param : f.params)
        {
            root.vars[param.name] = param.type;
        }
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
                    std::string path = expressionToPath(d->expr);
                    if (!validatePathExpressionType(path, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                }
                else if (auto *d = std::get_if<FunctionCallDirective>(&seg.info))
                {
                    for (const auto &argExpr : d->args)
                    {
                        std::string path = expressionToPath(argExpr);
                        if (!validatePathExpressionType(path, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                        {
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                            break;
                        }
                    }
                }
                else if (auto *d = std::get_if<ForDirective>(&seg.info))
                {
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
                    if (!d->iteratorVar.empty())
                        frame.vars[d->iteratorVar] = IntType{};
                    frame.openLineIndex = (int)li;
                    frame.openSeg = seg;
                    frames.push_back(std::move(frame));
                }
                else if (std::holds_alternative<EndForDirective>(seg.info))
                {
                    popTo(ScopeKind::For);
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
                else if (std::holds_alternative<EndIfDirective>(seg.info))
                {
                    popTo(ScopeKind::If);
                }
                else if (auto *d = std::get_if<SwitchDirective>(&seg.info))
                {
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
                                switchEnum = resolveEnumFromType(it->switchExprType.value(), types);
                            if (switchEnum)
                            {
                                std::vector<std::string> missing;
                                for (const auto &variant : switchEnum->variants)
                                {
                                    if (it->switchCaseTags.count(variant.tag) == 0)
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
                    const VariantDef *variant = findEnumVariant(switchEnum, d->tag);
                    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                    {
                        if (it->kind == ScopeKind::Switch)
                        {
                            it->switchCaseTags.insert(d->tag);
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
                    const EnumDef *switchEnum = findNearestSwitchEnum(frames, types);
                    const VariantDef *variant = findEnumVariant(switchEnum, d->exprText);
                    if (switchEnum && variant)
                        continue; // Pointfree variant visitor inside switch: @render Tag via fn@

                    if (!validatePathExpressionType(d->exprText, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                        continue;
                    }
                    if (!std::holds_alternative<std::shared_ptr<ListType>>(exprType))
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "render expression must be a list");
                }
            }
        }
        // Report any unclosed scoped blocks
        for (size_t i = 1; i < frames.size(); ++i)
        {
            const auto &frame = frames[i];
            if (frame.openLineIndex < 0)
                continue;
            std::string msg;
            switch (frame.kind)
            {
            case ScopeKind::For:    msg = "missing @end for@";    break;
            case ScopeKind::Switch: msg = "missing @end switch@"; break;
            case ScopeKind::Case:   msg = "missing @end case@";   break;
            case ScopeKind::If:     msg = "missing @endif@";      break;
            default: break;
            }
            if (!msg.empty())
                addTemplateDiagnostic(diags, bodyStartLine, frame.openLineIndex, frame.openSeg, msg);
        }
    }

    TypeRef parseParamType(const std::string &s)
    {
        std::string t = trim(s);
        if (t == "string" || t == "String")
            return StringType{};
        if (t == "int" || t == "Int")
            return IntType{};
        if (t == "bool" || t == "Bool")
            return BoolType{};
        // Check for list<...> or optional<...>
        if (startsWith(t, "list<") && t.back() == '>')
        {
            std::string inner = t.substr(5, t.size() - 6);
            return std::make_shared<ListType>(ListType{parseParamType(inner)});
        }
        if (startsWith(t, "optional<") && t.back() == '>')
        {
            std::string inner = t.substr(9, t.size() - 10);
            return std::make_shared<OptionalType>(OptionalType{parseParamType(inner)});
        }
        return NamedType{t};
    }

    bool parseOneTemplate(const std::string &text, size_t &pos,
                          TemplateFunction &func,
                          size_t *outBodyStartLine,
                          std::string *outBodyText,
                          std::vector<TemplateLine> *outTemplateLines,
                          std::vector<Diagnostic> *diags)
    {
        // Find "template " line
        size_t lineStart = pos;
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos)
            return false;
        std::string firstLine = trim(text.substr(lineStart, nl - lineStart));
        if (!startsWith(firstLine, "template "))
            return false;

        // Parse function name early (needed for error message when END is missing)
        std::string afterTemplate = firstLine.substr(9);
        size_t parenOpen = afterTemplate.find('(');
        if (parenOpen == std::string::npos)
            return false;
        func.name = trim(afterTemplate.substr(0, parenOpen));

        // Find END
        size_t endPos = text.find("\nEND", nl);
        if (endPos == std::string::npos)
        {
            if (diags)
            {
                size_t lineNum = 0;
                for (size_t i = 0; i < lineStart; ++i)
                    if (text[i] == '\n') ++lineNum;
                Diagnostic d;
                d.range = {{(int)lineNum, 0}, {(int)lineNum, (int)firstLine.size()}};
                d.message = "missing END for template '" + func.name + "'";
                d.severity = DiagnosticSeverity::Error;
                diags->push_back(std::move(d));
                pos = text.size();
            }
            return false;
        }

        std::string body = text.substr(nl + 1, endPos - nl - 1);

        size_t parenClose = afterTemplate.find(')', parenOpen);
        if (parenClose == std::string::npos)
            return false;
        std::string paramsStr = afterTemplate.substr(parenOpen + 1, parenClose - parenOpen - 1);

        if (!trim(paramsStr).empty())
        {
            // Split by comma, but respect angle brackets
            std::vector<std::string> paramParts;
            int depth = 0;
            std::string current;
            for (char c : paramsStr)
            {
                if (c == '<')
                {
                    depth++;
                    current += c;
                }
                else if (c == '>')
                {
                    depth--;
                    current += c;
                }
                else if (c == ',' && depth == 0)
                {
                    paramParts.push_back(current);
                    current.clear();
                }
                else
                {
                    current += c;
                }
            }
            if (!current.empty())
                paramParts.push_back(current);

            for (auto &param : paramParts)
            {
                param = trim(param);
                size_t colonPos = param.find(':');
                if (colonPos == std::string::npos)
                    continue;
                ParamDef pd;
                pd.name = trim(param.substr(0, colonPos));
                pd.type = parseParamType(param.substr(colonPos + 1));
                func.params.push_back(std::move(pd));
            }
        }

        auto templateLines = parseTemplateLines(body);
        ASTBuilder builder{templateLines};
        func.body = builder.parseBlock(0);

        if (outBodyText)
            *outBodyText = body;
        if (outTemplateLines)
            *outTemplateLines = templateLines;
        if (outBodyStartLine)
        {
            // count newlines up to the start of the body to produce a 0-based line number
            size_t lineCount = 0;
            for (size_t i = 0; i < lineStart; ++i)
                if (text[i] == '\n')
                    ++lineCount;
            // body starts on the line after the template header
            if (nl < text.size() && text[nl] == '\n')
                ++lineCount;
            *outBodyStartLine = lineCount;
        }

        pos = endPos + 4; // skip past \nEND
        return true;
    }

    bool Compiler::compile(const std::string &templateString,
                           CompilerOutput &output,
                           std::vector<Diagnostic> &diagnostics) const noexcept
    {
        try
        {
            size_t pos = 0;
            bool foundAny = false;
            while (pos < templateString.size())
            {
                // Skip whitespace between template blocks
                while (pos < templateString.size() &&
                       std::isspace(static_cast<unsigned char>(templateString[pos])))
                {
                    ++pos;
                }
                if (pos >= templateString.size())
                    break;

                TemplateFunction func;
                size_t bodyStartLine = 0;
                std::vector<TemplateLine> templateLines;
                if (!parseOneTemplate(templateString, pos, func, &bodyStartLine, nullptr, &templateLines, &diagnostics))
                {
                    if (!foundAny && diagnostics.empty())
                        return false;
                    break;
                }
                validateTemplateSemantics(func, templateLines, bodyStartLine, types, diagnostics);

                output.functions.push_back(std::move(func));
                foundAny = true;
            }
            output.types = types;
            // if diagnostics were added, compilation considered failed
            if (!diagnostics.empty())
                return false;
            return foundAny;
        }
        catch (...)
        {
            return false;
        }
    }

} // namespace tpp
