#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <tpp/IRAssembler.h>
#include "tpp/PublicIRConverter.h"
#include <tpp/Lowering.h>
#include <tpp/Tooling.h>
#include <tpp/TemplateParser.h>
#include <tpp/TypedefParser.h>
#include <tpp/Diagnostic.h>
#include <sstream>
#include <functional>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace tpp::compiler
{

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
            // \@ → literal @
            if (at > 0 && src[at - 1] == '\\')
            {
                if (at - 1 > i)
                    segs.push_back({false, src.substr(i, at - 1 - i), (int)i, (int)(at - 1)});
                segs.push_back({false, "@", (int)at, (int)(at + 1)});
                i = at + 1;
                continue;
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

    struct ParseError
    {
        std::string message;
        int start = 0, end = 0;
    };

    // Parses key=value option pairs (quoted strings or bare identifiers).
    // Optionally also accepts bare flag keywords (no `=`).
    // Returns an error with position info, or nullopt on success.
    static std::optional<ParseError> parseDirectiveOptions(
        const std::string &optPart,
        const std::set<std::string> &allowedStringKeys,
        const std::set<std::string> &allowedIdentKeys,
        std::map<std::string, std::string> &outValues,
        const std::set<std::string> &allowedFlags = {},
        std::set<std::string> *outFlags = nullptr)
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

            // Bare flag (no '=')
            if (p >= optPart.size() || optPart[p] != '=')
            {
                const bool knownKey = allowedStringKeys.count(key) != 0 || allowedIdentKeys.count(key) != 0;
                if (allowedFlags.count(key) == 0)
                    return ParseError{knownKey
                        ? "expected '=' after option key '" + key + "'"
                        : "unknown option '" + key + "'", (int)keyStart, (int)p};
                if (seen.count(key))
                    return ParseError{"duplicate option '" + key + "'", (int)keyStart, (int)p};
                seen.insert(key);
                if (outFlags) outFlags->insert(key);
                continue;
            }
            ++p; // skip '='
            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;

            if (seen.count(key))
                return ParseError{"duplicate option '" + key + "'", (int)keyStart, (int)p};
            seen.insert(key);

            if (allowedStringKeys.count(key) == 0 && allowedIdentKeys.count(key) == 0)
                return ParseError{"unknown option '" + key + "'", (int)keyStart, (int)(keyStart + key.size())};

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

    // Parses a mix of key="value" quoted-string pairs and bare flag keywords.
    static std::optional<ParseError> parseDirectiveMixed(
        const std::string &optPart,
        const std::set<std::string> &allowedStringKeys,
        const std::set<std::string> &allowedFlags,
        std::map<std::string, std::string> &outValues,
        std::set<std::string> &outFlags)
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
                return ParseError{"invalid option '" + key + "'", (int)keyStart, (int)p};

            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;

            if (p < optPart.size() && optPart[p] == '=')
            {
                ++p;
                while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                    ++p;
                if (seen.count(key))
                    return ParseError{"duplicate option '" + key + "'", (int)keyStart, (int)p};
                seen.insert(key);
                if (allowedStringKeys.count(key) == 0)
                    return ParseError{"unknown option '" + key + "'", (int)keyStart, (int)p};
                size_t before = p;
                std::string value = parseStringLiteral(optPart, p);
                if (before == p)
                    return ParseError{"option '" + key + "' expects a quoted string value", (int)keyStart, (int)p};
                outValues[key] = value;
            }
            else
            {
                if (seen.count(key))
                    return ParseError{"duplicate option '" + key + "'", (int)keyStart, (int)p};
                seen.insert(key);
                if (allowedFlags.count(key) == 0)
                    return ParseError{"unknown option '" + key + "'", (int)keyStart, (int)p};
                outFlags.insert(key);
            }
        }
        return std::nullopt;
    }

    DirectiveInfo classifyDirective(const std::string &s)
    {
        std::string t = trim(s);

        if (t.empty())
            return ErrorDirective{"empty directive"};

        if (t == "&")
            return AlignmentCellDirective{};

        if (t == "comment")
            return CommentDirective{};
        if (t == "end comment")
            return EndCommentDirective{};

        if (t == "else")
            return ElseDirective{};
        if (t == "end if")
            return EndIfDirective{};
        if (t == "end for")
            return EndForDirective{};
        if (t == "end switch")
            return EndSwitchDirective{};

        if (startsWith(t, "end case"))
        {
            if (!trim(t.substr(8)).empty())
                return ErrorDirective{"unexpected trailing tokens in end case directive"};
            return EndCaseDirective{};
        }

        if (startsWith(t, "default"))
        {
            if (!trim(t.substr(7)).empty())
                return ErrorDirective{"unexpected trailing tokens in default directive"};
            return DefaultDirective{};
        }

        if (startsWith(t, "for "))
        {
            std::string mainPart = t;
            std::string optPart;
            size_t pipe = t.find('|');
            if (pipe != std::string::npos)
            {
                mainPart = trim(t.substr(0, pipe));
                optPart = trim(t.substr(pipe + 1));
                if (optPart.empty())
                    return ErrorDirective{"for options list is empty after '|'"};
            }
            std::string rest = mainPart.substr(4);
            size_t inPos = rest.find(" in ");
            if (inPos == std::string::npos)
                return ErrorDirective{"invalid for directive; expected 'for <item> in <collection>'"};

            ForDirective d;
            d.var = trim(rest.substr(0, inPos));
            d.collectionText = trim(rest.substr(inPos + 4));
            d.collection = parseExpression(d.collectionText);

            if (!isIdentifier(d.var))
                return ErrorDirective{"invalid loop variable name '" + d.var + "'"};
            if (!isPathExpressionText(d.collectionText))
                return ErrorDirective{"for collection must be a variable or member path"};
            if (pipe == std::string::npos)
            {
                if (rest.find("sep=") != std::string::npos ||
                    rest.find("followedBy=") != std::string::npos ||
                    rest.find("precededBy=") != std::string::npos ||
                    rest.find("enumerator=") != std::string::npos)
                    return ErrorDirective{"for options require '|' before option list"};
            }
            if (!optPart.empty())
            {
                // Compute absolute offset of optPart within directive text t
                // so error positions are correct relative to the full line.
                std::string optRaw = t.substr(pipe + 1);
                size_t lead = 0;
                while (lead < optRaw.size() && std::isspace(static_cast<unsigned char>(optRaw[lead])))
                    ++lead;
                const int optOffset = static_cast<int>(pipe + 1 + lead);

                std::map<std::string, std::string> values;
                std::set<std::string> flags;
                if (auto err = parseDirectiveOptions(optPart, {"sep", "followedBy", "precededBy", "policy", "align"}, {"enumerator"}, values, {"align"}, &flags))
                    d.parseErrors.emplace_back(err->message, optOffset + err->start, optOffset + err->end);
                d.sep = values["sep"];
                d.followedBy = values["followedBy"];
                d.precededBy = values["precededBy"];
                d.enumerator = values["enumerator"];
                d.policy = values["policy"];
                if (flags.count("align") || values.count("align"))
                    d.alignSpec = values.count("align") ? values["align"] : std::string{};
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
                condText = trim(condText.substr(4));
            }
            d.condText = condText;
            if (!isPathExpressionText(condText))
                return ErrorDirective{"if condition must be a variable or member path"};
            d.cond = parseExpression(condText);
            return d;
        }

        if (startsWith(t, "switch "))
        {
            std::string rest = trim(t.substr(7));
            std::string exprText = rest;
            std::string optPart;
            size_t pipe = rest.find('|');
            if (pipe != std::string::npos)
            {
                exprText = trim(rest.substr(0, pipe));
                optPart = trim(rest.substr(pipe + 1));
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
                std::map<std::string, std::string> values;
                std::set<std::string> flags;
                if (auto err = parseDirectiveMixed(optPart, {"policy"}, {"checkExhaustive"}, values, flags))
                    return ErrorDirective{err->message, err->start, err->end};
                d.checkExhaustive = flags.count("checkExhaustive") != 0;
                d.policy = values["policy"];
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
            std::string rest = t.substr(7);
            std::string mainPart = rest;
            std::string optPart;
            size_t pipe = rest.find('|');
            if (pipe != std::string::npos)
            {
                mainPart = trim(rest.substr(0, pipe));
                optPart = trim(rest.substr(pipe + 1));
                if (optPart.empty())
                    return ErrorDirective{"render options list is empty after '|'"};
            }
            size_t viaPos = mainPart.find(" via ");
            if (viaPos == std::string::npos)
                return ErrorDirective{"invalid render directive; expected 'render <collection> via <function>'"};
            RenderDirective d;
            d.exprText = trim(mainPart.substr(0, viaPos));
            d.expr = parseExpression(d.exprText);
            d.func = trim(mainPart.substr(viaPos + 5));
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
                const size_t renderPrefixLen = 7;
                std::string optRaw = t.substr(renderPrefixLen + pipe + 1);
                size_t lead = 0;
                while (lead < optRaw.size() && std::isspace(static_cast<unsigned char>(optRaw[lead])))
                    ++lead;
                const int optOffset = static_cast<int>(renderPrefixLen + pipe + 1 + lead);

                std::map<std::string, std::string> values;
                if (auto err = parseDirectiveOptions(optPart, {"sep", "followedBy", "precededBy", "policy"}, {}, values))
                    d.parseErrors.emplace_back(err->message, optOffset + err->start, optOffset + err->end);
                else
                {
                    d.sep = values["sep"];
                    d.followedBy = values["followedBy"];
                    d.precededBy = values["precededBy"];
                    d.policy = values["policy"];
                }
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
                    {
                        validName = false;
                        break;
                    }
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
                            if (c == '(')
                            {
                                depth++;
                                current += c;
                            }
                            else if (c == ')')
                            {
                                depth--;
                                current += c;
                            }
                            else if (c == ',' && depth == 0)
                            {
                                argParts.push_back(current);
                                current.clear();
                            }
                            else
                            {
                                current += c;
                            }
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

        // Check for pipe: @expr | policy="tag"@
        {
            size_t pipe = t.find('|');
            if (pipe != std::string::npos)
            {
                std::string exprPart = trim(t.substr(0, pipe));
                std::string optsPart = trim(t.substr(pipe + 1));
                std::map<std::string, std::string> values;
                if (auto err = parseDirectiveOptions(optsPart, {"policy"}, {}, values))
                    return ErrorDirective{err->message, err->start, err->end};
                ExprDirective d;
                d.expr = parseExpression(exprPart);
                d.policy = values["policy"];
                return d;
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

    std::vector<ASTNode> ASTBuilder::parseInline(const std::vector<LineSeg> &segs,
                                                  size_t startPos,
                                                  int lineIndex,
                                                  Range *outEndRange)
    {
        std::vector<ASTNode> nodes;
        size_t pos = startPos;
        Range lastTerminatorRange{}; // set by terminator handlers, read by enclosing directive

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
                    InterpolationNode in;
                    in.expr = d->expr;
                    in.policy = d->policy;
                    in.sourceRange = makeRange(lineIndex, s);
                    sub.push_back(std::move(in));
                    ++pos;
                }
                else if (auto *d = std::get_if<FunctionCallDirective>(&s.info))
                {
                    auto callNode = std::make_shared<FunctionCallNode>();
                    callNode->functionName = d->name;
                    callNode->arguments = d->args;
                    callNode->sourceRange = makeRange(lineIndex, s);
                    sub.push_back(std::move(callNode));
                    ++pos;
                }
                else if (auto *d = std::get_if<ForDirective>(&s.info))
                {
                    auto forNode = std::make_shared<ForNode>();
                    forNode->varName = d->var;
                    forNode->enumeratorName = d->enumerator;
                    forNode->collectionExpr = d->collection;
                    forNode->sep = d->sep;
                    forNode->followedBy = d->followedBy;
                    forNode->precededBy = d->precededBy;
                    forNode->policy = d->policy;
                    forNode->alignSpec = d->alignSpec;
                    forNode->isBlock = false;
                    forNode->sourceRange = makeRange(lineIndex, s);
                    ++pos;
                    forNode->body = recurse();
                    forNode->endRange = lastTerminatorRange;
                    lastTerminatorRange = {};
                    sub.push_back(std::move(forNode));
                }
                else if (std::holds_alternative<AlignmentCellDirective>(s.info))
                {
                    AlignmentCellNode cell;
                    cell.sourceRange = makeRange(lineIndex, s);
                    sub.push_back(std::move(cell));
                    ++pos;
                }
                else if (std::holds_alternative<CommentDirective>(s.info))
                {
                    // @comment@ ... @end comment@: consume until EndCommentDirective, emit a CommentNode
                    CommentNode cn;
                    cn.startRange = makeRange(lineIndex, s);
                    ++pos;
                    // Consume all segments until @end comment@
                    while (pos < segs.size())
                    {
                        const auto &cs = segs[pos];
                        if (cs.isDirective && std::holds_alternative<EndCommentDirective>(cs.info))
                        {
                            cn.endRange = makeRange(lineIndex, cs);
                            ++pos;
                            break;
                        }
                        ++pos;
                    }
                    sub.push_back(std::move(cn));
                }
                else if (std::holds_alternative<EndForDirective>(s.info))
                {
                    lastTerminatorRange = makeRange(lineIndex, s);
                    ++pos;
                    return sub;
                }
                else if (auto *d = std::get_if<IfDirective>(&s.info))
                {
                    auto ifNode = std::make_shared<IfNode>();
                    ifNode->condExpr = d->cond;
                    ifNode->negated = d->negated;
                    ifNode->condText = d->condText;
                    ifNode->isBlock = false;
                    ifNode->sourceRange = makeRange(lineIndex, s);
                    ++pos;
                    ifNode->thenBody = recurse();
                    if (pos < segs.size() && segs[pos].isDirective &&
                        std::holds_alternative<ElseDirective>(segs[pos].info))
                    {
                        ifNode->elseRange = makeRange(lineIndex, segs[pos]);
                        ++pos;
                        lastTerminatorRange = {};
                        ifNode->elseBody = recurse();
                    }
                    ifNode->endRange = lastTerminatorRange;
                    lastTerminatorRange = {};
                    sub.push_back(std::move(ifNode));
                }
                else if (std::holds_alternative<ElseDirective>(s.info))
                {
                    return sub;
                }
                else if (std::holds_alternative<DefaultDirective>(s.info))
                {
                    return sub;
                }
                else if (std::holds_alternative<EndIfDirective>(s.info))
                {
                    lastTerminatorRange = makeRange(lineIndex, s);
                    ++pos;
                    return sub;
                }
                else if (auto *d = std::get_if<SwitchDirective>(&s.info))
                {
                    auto switchNode = std::make_shared<SwitchNode>();
                    switchNode->expr = d->expr;
                    switchNode->checkExhaustive = d->checkExhaustive;
                    switchNode->policy = d->policy;
                    switchNode->isBlock = false;
                    switchNode->sourceRange = makeRange(lineIndex, s);
                    ++pos;
                    while (pos < segs.size())
                    {
                        auto &cs = segs[pos];
                        if (!cs.isDirective)
                        {
                            ++pos;
                            continue;
                        }
                        if (std::holds_alternative<EndSwitchDirective>(cs.info))
                        {
                            switchNode->endRange = makeRange(lineIndex, cs);
                            ++pos;
                            break;
                        }
                        if (auto *cd = std::get_if<CaseDirective>(&cs.info))
                        {
                            CaseNode cn;
                            cn.tag = cd->tag;
                            cn.bindingName = cd->binding;
                            cn.sourceRange = makeRange(lineIndex, cs);
                            ++pos;
                            lastTerminatorRange = {};
                            cn.body = recurse();
                            cn.endRange = lastTerminatorRange;
                            lastTerminatorRange = {};
                            switchNode->cases.push_back(std::move(cn));
                        }
                        else if (std::holds_alternative<DefaultDirective>(cs.info))
                        {
                            CaseNode cn;
                            cn.sourceRange = makeRange(lineIndex, cs);
                            ++pos;
                            lastTerminatorRange = {};
                            cn.body = recurse();
                            cn.endRange = lastTerminatorRange;
                            lastTerminatorRange = {};
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
                    lastTerminatorRange = makeRange(lineIndex, s);
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
        if (outEndRange)
            *outEndRange = lastTerminatorRange;
        return nodes;
    }

    std::vector<ASTNode> ASTBuilder::parseBlock(int insertCol, Range *outEndRange)
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
                        forNode->varName = d->var;
                        forNode->enumeratorName = d->enumerator;
                        forNode->collectionExpr = d->collection;
                        forNode->sep = d->sep;
                        forNode->followedBy = d->followedBy;
                        forNode->precededBy = d->precededBy;
                        forNode->policy = d->policy;
                        forNode->alignSpec = d->alignSpec;
                        forNode->isBlock = true;
                        forNode->insertCol = tl.indent;
                        forNode->sourceRange = makeRange((int)pos, seg);
                        ++pos;
                        Range forEndRange{};
                        forNode->body = parseBlock(tl.indent, &forEndRange);
                        forNode->endRange = forEndRange;
                        nodes.push_back(std::move(forNode));
                    }
                    else if (std::holds_alternative<AlignmentCellDirective>(seg.info))
                    {
                        AlignmentCellNode cell;
                        cell.sourceRange = makeRange((int)pos, seg);
                        nodes.push_back(std::move(cell));
                        ++pos;
                    }
                    else if (std::holds_alternative<CommentDirective>(seg.info))
                    {
                        // @comment@ on its own block line: consume lines until @end comment@
                        CommentNode cn;
                        cn.startRange = makeRange((int)pos, seg);
                        ++pos;
                        while (pos < lines.size())
                        {
                            bool done = false;
                            for (const auto &s2 : lines[pos].segments)
                            {
                                if (s2.isDirective && std::holds_alternative<EndCommentDirective>(s2.info))
                                { cn.endRange = makeRange((int)pos, s2); ++pos; done = true; break; }
                            }
                            if (done) break;
                            ++pos;
                        }
                        nodes.push_back(std::move(cn));
                    }
                    else if (std::holds_alternative<EndForDirective>(seg.info))
                    {
                        if (outEndRange) *outEndRange = makeRange((int)pos, seg);
                        ++pos;
                        return nodes;
                    }
                    else if (auto *d = std::get_if<IfDirective>(&seg.info))
                    {
                        auto ifNode = std::make_shared<IfNode>();
                        ifNode->condExpr = d->cond;
                        ifNode->negated = d->negated;
                        ifNode->condText = d->condText;
                        ifNode->isBlock = true;
                        ifNode->insertCol = tl.indent;
                        ifNode->sourceRange = makeRange((int)pos, seg);
                        ++pos;
                        Range ifEndRange{};
                        ifNode->thenBody = parseBlock(tl.indent, &ifEndRange);
                        if (pos < lines.size() && lines[pos].isBlockLine)
                        {
                            for (auto &s2 : lines[pos].segments)
                            {
                                if (s2.isDirective && std::holds_alternative<ElseDirective>(s2.info))
                                {
                                    ifNode->elseRange = makeRange((int)pos, s2);
                                    ++pos;
                                    ifNode->elseBody = parseBlock(tl.indent, &ifEndRange);
                                    break;
                                }
                            }
                        }
                        ifNode->endRange = ifEndRange;
                        nodes.push_back(std::move(ifNode));
                    }
                    else if (std::holds_alternative<ElseDirective>(seg.info))
                    {
                        return nodes;
                    }
                    else if (std::holds_alternative<EndIfDirective>(seg.info))
                    {
                        if (outEndRange) *outEndRange = makeRange((int)pos, seg);
                        ++pos;
                        return nodes;
                    }
                    else if (std::holds_alternative<DefaultDirective>(seg.info))
                    {
                        return nodes;
                    }
                    else if (auto *d = std::get_if<SwitchDirective>(&seg.info))
                    {
                        auto switchNode = std::make_shared<SwitchNode>();
                        switchNode->expr = d->expr;
                        switchNode->checkExhaustive = d->checkExhaustive;
                        switchNode->policy = d->policy;
                        switchNode->isBlock = true;
                        switchNode->insertCol = tl.indent;
                        switchNode->sourceRange = makeRange((int)pos, seg);
                        ++pos;
                        while (pos < lines.size())
                        {
                            bool done = false;
                            bool advancedLine = false;

                            for (size_t segIndex = 0; segIndex < lines[pos].segments.size(); ++segIndex)
                            {
                                auto &s2 = lines[pos].segments[segIndex];
                                if (!s2.isDirective)
                                    continue;

                                if (std::holds_alternative<EndSwitchDirective>(s2.info))
                                {
                                    switchNode->endRange = makeRange((int)pos, s2);
                                    ++pos;
                                    done = true;
                                    break;
                                }

                                const bool hasTrailingInlineSegments = segIndex + 1 < lines[pos].segments.size();

                                if (auto *cd = std::get_if<CaseDirective>(&s2.info))
                                {
                                    CaseNode cn;
                                    cn.tag = cd->tag;
                                    cn.bindingName = cd->binding;
                                    cn.sourceRange = makeRange((int)pos, s2);

                                    if (hasTrailingInlineSegments)
                                    {
                                        Range caseEndRange{};
                                        cn.body = parseInline(lines[pos].segments, segIndex + 1, (int)pos, &caseEndRange);
                                        cn.endRange = caseEndRange;
                                        cn.body.push_back(TextNode{"\n"});
                                        switchNode->cases.push_back(std::move(cn));
                                        ++pos;
                                        advancedLine = true;
                                    }
                                    else
                                    {
                                        ++pos;
                                        Range caseEndRange{};
                                        cn.body = parseBlock(tl.indent, &caseEndRange);
                                        cn.endRange = caseEndRange;
                                        switchNode->cases.push_back(std::move(cn));
                                        advancedLine = true;
                                    }
                                    break;
                                }

                                if (std::holds_alternative<DefaultDirective>(s2.info))
                                {
                                    CaseNode cn;
                                    cn.sourceRange = makeRange((int)pos, s2);

                                    if (hasTrailingInlineSegments)
                                    {
                                        Range caseEndRange{};
                                        cn.body = parseInline(lines[pos].segments, segIndex + 1, (int)pos, &caseEndRange);
                                        cn.endRange = caseEndRange;
                                        cn.body.push_back(TextNode{"\n"});
                                        switchNode->defaultCase = std::move(cn);
                                        ++pos;
                                        advancedLine = true;
                                    }
                                    else
                                    {
                                        ++pos;
                                        Range caseEndRange{};
                                        cn.body = parseBlock(tl.indent, &caseEndRange);
                                        cn.endRange = caseEndRange;
                                        switchNode->defaultCase = std::move(cn);
                                        advancedLine = true;
                                    }
                                    break;
                                }

                                if (auto *rd = std::get_if<RenderDirective>(&s2.info))
                                {
                                    CaseNode cn;
                                    cn.tag = rd->exprText;
                                    cn.isSyntheticRenderCase = true;
                                    cn.sourceRange = makeRange((int)pos, s2);
                                    auto callNode = std::make_shared<FunctionCallNode>();
                                    callNode->functionName = rd->func;
                                    cn.body.push_back(std::move(callNode));
                                    switchNode->cases.push_back(std::move(cn));
                                    ++pos;
                                    advancedLine = true;
                                    break;
                                }

                                if (lines[pos].isBlockLine)
                                {
                                    ++pos;
                                    advancedLine = true;
                                    break;
                                }
                            }

                            if (done)
                                break;
                            if (!advancedLine)
                                ++pos;
                        }
                        nodes.push_back(std::move(switchNode));
                    }
                    else if (std::holds_alternative<EndSwitchDirective>(seg.info))
                    {
                        if (outEndRange) *outEndRange = makeRange((int)pos, seg);
                        ++pos;
                        return nodes;
                    }
                    else if (std::holds_alternative<CaseDirective>(seg.info))
                    {
                        return nodes;
                    }
                    else if (std::holds_alternative<DefaultDirective>(seg.info))
                    {
                        return nodes;
                    }
                    else if (std::holds_alternative<EndCaseDirective>(seg.info))
                    {
                        if (outEndRange) *outEndRange = makeRange((int)pos, seg);
                        ++pos;
                        return nodes;
                    }
                    else if (auto *d = std::get_if<RenderDirective>(&seg.info))
                    {
                        auto renderNode = std::make_shared<RenderViaNode>();
                        renderNode->collectionExpr = d->expr;
                        renderNode->functionName = d->func;
                        renderNode->sep = d->sep;
                        renderNode->followedBy = d->followedBy;
                        renderNode->precededBy = d->precededBy;
                        renderNode->policy = d->policy;
                        renderNode->isBlock = true;
                        renderNode->insertCol = tl.indent;
                        renderNode->sourceRange = makeRange((int)pos, seg);
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
                    auto inlineNodes = parseInline(tl.segments, 0, (int)pos);
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
                            callNode->arguments = d->args;
                            callNode->sourceRange = makeRange((int)pos, seg);
                            nodes.push_back(std::move(callNode));
                        }
                        else if (auto *d = std::get_if<ExprDirective>(&seg.info))
                        {
                            InterpolationNode in;
                            in.expr = d->expr;
                            in.policy = d->policy;
                            in.sourceRange = makeRange((int)pos, seg);
                            nodes.push_back(std::move(in));
                        }
                    }
                    nodes.push_back(TextNode{"\n"});
                }
                ++pos;
            }
        }
        return nodes;
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
                          std::vector<Diagnostic> *diags,
                          Range *outHeaderRange,
                          std::string *outHeaderText)
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
                    if (text[i] == '\n')
                        ++lineNum;
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

        // Unescape \END at the start of lines → literal END
        {
            std::string unescaped;
            unescaped.reserve(body.size());
            size_t p = 0;
            while (p < body.size())
            {
                bool atLineStart = (p == 0) || body[p - 1] == '\n';
                if (atLineStart && body.size() - p >= 4 &&
                    body[p] == '\\' && body[p+1] == 'E' && body[p+2] == 'N' && body[p+3] == 'D' &&
                    (p + 4 >= body.size() || body[p+4] == '\n' || body[p+4] == '\r'))
                {
                    unescaped += "END";
                    p += 4;
                    continue;
                }
                unescaped += body[p++];
            }
            body = std::move(unescaped);
        }

        size_t parenClose = afterTemplate.find(')', parenOpen);
        if (parenClose == std::string::npos)
            return false;
        std::string paramsStr = afterTemplate.substr(parenOpen + 1, parenClose - parenOpen - 1);

        // Extract | policy="..." from paramsStr (not inside <> brackets)
        {
            int angleDepth = 0;
            for (size_t i = 0; i < paramsStr.size(); ++i)
            {
                if (paramsStr[i] == '<') ++angleDepth;
                else if (paramsStr[i] == '>') --angleDepth;
                else if (paramsStr[i] == '|' && angleDepth == 0)
                {
                    std::string optsPart = trim(paramsStr.substr(i + 1));
                    paramsStr = paramsStr.substr(0, i);
                    if (!optsPart.empty())
                    {
                        std::map<std::string, std::string> values;
                        if (auto err = parseDirectiveOptions(optsPart, {"policy"}, {}, values))
                        {
                            if (diags)
                            {
                                size_t lineNum = 0;
                                for (size_t k = 0; k < lineStart; ++k)
                                    if (text[k] == '\n') ++lineNum;
                                Diagnostic diag;
                                diag.range = {{(int)lineNum, 0}, {(int)lineNum, (int)firstLine.size()}};
                                diag.message = "invalid template option: " + err->message;
                                diag.severity = DiagnosticSeverity::Error;
                                diags->push_back(std::move(diag));
                            }
                        }
                        else
                        {
                            func.policy = values["policy"];
                        }
                    }
                    break;
                }
            }
        }

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

        // Compute bodyStartLine: number of newlines before the body in the full text.
        size_t bodyStartLine = 0;
        for (size_t i = 0; i < lineStart; ++i)
            if (text[i] == '\n') ++bodyStartLine;
        if (nl < text.size() && text[nl] == '\n') ++bodyStartLine;

        // Compute header line number for func.sourceRange
        size_t headerLineNum = bodyStartLine > 0 ? bodyStartLine - 1 : 0;
        func.sourceRange = {{(int)headerLineNum, 0}, {(int)headerLineNum, (int)(nl - lineStart)}};

        ASTBuilder builder{templateLines, 0, bodyStartLine};
        func.body = builder.parseBlock(0);

        if (outBodyText)
            *outBodyText = body;
        if (outTemplateLines)
            *outTemplateLines = templateLines;
        if (outBodyStartLine)
            *outBodyStartLine = bodyStartLine;

        if (outHeaderRange)
        {
            // column: "template " is 9 chars, then any leading-whitespace-trimmed name starts
            std::string afterTemplate = trim(text.substr(lineStart, nl - lineStart)).substr(9);
            size_t nameStart = afterTemplate.find_first_not_of(' ');
            int col = 9 + (int)(nameStart == std::string::npos ? 0 : nameStart);
            *outHeaderRange = {{(int)headerLineNum, col},
                               {(int)headerLineNum, col + (int)func.name.size()}};
        }

        if (outHeaderText)
            *outHeaderText = firstLine;

        pos = endPos + 4; // skip past \nEND
        return true;
    }

} // namespace tpp::compiler
