#include <tpp/Compiler.h>
#include <tpp/Program.h>
#include <tpp/Tokenizer.h>
#include <tpp/Parser.h>
#include <sstream>
#include <functional>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace tpp
{

    // ═══════════════════════════════════════════════════════════════════
    // Typedef Tokenizer & Parser  (add_types)
    // ═══════════════════════════════════════════════════════════════════

    std::vector<Token> tokenize_typedefs(const std::string &src)
    {
        std::vector<Token> tokens;
        int line = 0, col = 0;
        size_t i = 0;
        auto advance = [&]()
        {
            if (i < src.size())
            {
                if (src[i] == '\n')
                {
                    ++line;
                    col = 0;
                }
                else
                {
                    ++col;
                }
                ++i;
            }
        };
        while (i < src.size())
        {
            if (std::isspace(static_cast<unsigned char>(src[i])))
            {
                advance();
                continue;
            }
            int sline = line, scol = col;
            char c = src[i];
            if (c == '{')
            {
                tokens.push_back({TokKind::LBrace, "{", sline, scol});
                advance();
                continue;
            }
            if (c == '}')
            {
                tokens.push_back({TokKind::RBrace, "}", sline, scol});
                advance();
                continue;
            }
            if (c == ';')
            {
                tokens.push_back({TokKind::Semi, ";", sline, scol});
                advance();
                continue;
            }
            if (c == ':')
            {
                tokens.push_back({TokKind::Colon, ":", sline, scol});
                advance();
                continue;
            }
            if (c == '(')
            {
                tokens.push_back({TokKind::LParen, "(", sline, scol});
                advance();
                continue;
            }
            if (c == ')')
            {
                tokens.push_back({TokKind::RParen, ")", sline, scol});
                advance();
                continue;
            }
            if (c == ',')
            {
                tokens.push_back({TokKind::Comma, ",", sline, scol});
                advance();
                continue;
            }
            if (c == '<')
            {
                tokens.push_back({TokKind::LAngle, "<", sline, scol});
                advance();
                continue;
            }
            if (c == '>')
            {
                tokens.push_back({TokKind::RAngle, ">", sline, scol});
                advance();
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
            {
                std::string word;
                while (i < src.size() && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_'))
                {
                    word += src[i];
                    advance();
                }
                TokKind k = TokKind::Ident;
                if (word == "struct")
                    k = TokKind::Struct;
                else if (word == "enum")
                    k = TokKind::Enum;
                else if (word == "optional")
                    k = TokKind::KwOptional;
                else if (word == "list")
                    k = TokKind::KwList;
                else if (word == "string")
                    k = TokKind::KwString;
                else if (word == "int")
                    k = TokKind::KwInt;
                else if (word == "bool")
                    k = TokKind::KwBool;
                tokens.push_back({k, word, sline, scol});
                continue;
            }
            advance();
        }
        tokens.push_back({TokKind::Eof, "", line, col});
        return tokens;
    }

    const Token &TypedefParser::cur() const { return tokens[pos]; }

    static const char *tokKindName(TokKind k)
    {
        switch (k)
        {
        case TokKind::Struct:
            return "'struct'";
        case TokKind::Enum:
            return "'enum'";
        case TokKind::LBrace:
            return "'{'";
        case TokKind::RBrace:
            return "'}'";
        case TokKind::Semi:
            return "';'";
        case TokKind::Colon:
            return "':'";
        case TokKind::LParen:
            return "'('";
        case TokKind::RParen:
            return "')'";
        case TokKind::Comma:
            return "','";
        case TokKind::LAngle:
            return "'<'";
        case TokKind::RAngle:
            return "'>'";
        case TokKind::KwOptional:
            return "'optional'";
        case TokKind::KwList:
            return "'list'";
        case TokKind::KwString:
            return "'string'";
        case TokKind::KwInt:
            return "'int'";
        case TokKind::KwBool:
            return "'bool'";
        case TokKind::Ident:
            return "identifier";
        case TokKind::Eof:
            return "end of file";
        }
        return "unknown";
    }

    const Token &TypedefParser::eat(TokKind k)
    {
        if (cur().kind != k)
        {
            if (ok)
            {
                Diagnostic d;
                auto &t = cur();
                d.range = {{t.line, t.col}, {t.line, t.col + (int)t.text.size()}};
                d.message = std::string("expected ") + tokKindName(k) + ", got " +
                            (t.kind == TokKind::Eof ? "end of file" : "'" + t.text + "'");
                d.severity = DiagnosticSeverity::Error;
                diags.push_back(d);
            }
            ok = false;
            return tokens[pos]; // don't advance on error
        }
        return tokens[pos++];
    }
    bool TypedefParser::at(TokKind k) const { return cur().kind == k; }

    TypeRef TypedefParser::parseTypeRef()
    {
        if (at(TokKind::KwString))
        {
            eat(TokKind::KwString);
            return StringType{};
        }
        if (at(TokKind::KwInt))
        {
            eat(TokKind::KwInt);
            return IntType{};
        }
        if (at(TokKind::KwBool))
        {
            eat(TokKind::KwBool);
            return BoolType{};
        }
        if (at(TokKind::KwOptional))
        {
            eat(TokKind::KwOptional);
            eat(TokKind::LAngle);
            auto inner = parseTypeRef();
            eat(TokKind::RAngle);
            return std::make_shared<OptionalType>(OptionalType{inner});
        }
        if (at(TokKind::KwList))
        {
            eat(TokKind::KwList);
            eat(TokKind::LAngle);
            auto elem = parseTypeRef();
            eat(TokKind::RAngle);
            return std::make_shared<ListType>(ListType{elem});
        }
        auto &tok = cur();
        eat(TokKind::Ident);
        return NamedType{tok.text};
    }

    void TypedefParser::parseStruct()
    {
        eat(TokKind::Struct);
        auto &nameTok = cur();
        eat(TokKind::Ident);
        StructDef sd;
        sd.name = nameTok.text;
        eat(TokKind::LBrace);
        while (!at(TokKind::RBrace) && !at(TokKind::Eof) && ok)
        {
            auto &fnameTok = cur();
            eat(TokKind::Ident);
            if (!ok)
                break;
            eat(TokKind::Colon);
            if (!ok)
                break;
            auto type = parseTypeRef();
            if (!ok)
                break;
            eat(TokKind::Semi);
            sd.fields.push_back({fnameTok.text, type});
        }
        eat(TokKind::RBrace);
        if (at(TokKind::Semi))
            eat(TokKind::Semi);

        size_t idx = reg.structs.size();
        reg.structs.push_back(std::move(sd));
        reg.nameIndex[reg.structs[idx].name] = {TypeKind::Struct, idx};
    }

    void TypedefParser::parseEnum()
    {
        eat(TokKind::Enum);
        auto &nameTok = cur();
        eat(TokKind::Ident);
        EnumDef ed;
        ed.name = nameTok.text;
        eat(TokKind::LBrace);
        while (!at(TokKind::RBrace) && !at(TokKind::Eof) && ok)
        {
            auto &tagTok = cur();
            eat(TokKind::Ident);
            if (!ok)
                break;
            VariantDef vd;
            vd.tag = tagTok.text;
            if (at(TokKind::LParen))
            {
                eat(TokKind::LParen);
                vd.payload = parseTypeRef();
                eat(TokKind::RParen);
            }
            if (!ok)
                break;
            if (at(TokKind::Comma))
                eat(TokKind::Comma);
            ed.variants.push_back(std::move(vd));
        }
        eat(TokKind::RBrace);
        if (at(TokKind::Semi))
            eat(TokKind::Semi);

        size_t idx = reg.enums.size();
        reg.enums.push_back(std::move(ed));
        reg.nameIndex[reg.enums[idx].name] = {TypeKind::Enum, idx};
    }

    bool TypedefParser::validateTypes()
    {
        // Walk tokens again to find ident tokens used as type refs and validate them
        size_t p = 0;
        auto skip = [&]()
        { ++p; };
        bool allOk = true;

        std::function<bool()> validateTypeFromTokens = [&]() -> bool
        {
            if (tokens[p].kind == TokKind::KwOptional || tokens[p].kind == TokKind::KwList)
            {
                skip();
                skip(); // kw <
                bool r = validateTypeFromTokens();
                skip(); // >
                return r;
            }
            else if (tokens[p].kind == TokKind::KwString || tokens[p].kind == TokKind::KwInt || tokens[p].kind == TokKind::KwBool)
            {
                skip();
                return true;
            }
            else
            {
                auto &t = tokens[p];
                if (reg.nameIndex.find(t.text) == reg.nameIndex.end())
                {
                    Diagnostic d;
                    d.range = {{t.line, t.col}, {t.line, t.col + (int)t.text.size()}};
                    d.message = "undefined type '" + t.text + "'";
                    d.severity = DiagnosticSeverity::Error;
                    diags.push_back(d);
                    skip();
                    return false;
                }
                skip();
                return true;
            }
        };

        while (p < tokens.size() && tokens[p].kind != TokKind::Eof)
        {
            if (tokens[p].kind == TokKind::Struct)
            {
                skip();
                skip();
                skip(); // struct name {
                while (tokens[p].kind != TokKind::RBrace && tokens[p].kind != TokKind::Eof)
                {
                    skip();
                    skip(); // field_name :
                    if (!validateTypeFromTokens())
                        allOk = false;
                    skip(); // ;
                }
                skip(); // }
                if (p < tokens.size() && tokens[p].kind == TokKind::Semi)
                    skip();
            }
            else if (tokens[p].kind == TokKind::Enum)
            {
                skip();
                skip();
                skip(); // enum name {
                while (tokens[p].kind != TokKind::RBrace && tokens[p].kind != TokKind::Eof)
                {
                    skip(); // tag
                    if (tokens[p].kind == TokKind::LParen)
                    {
                        skip(); // (
                        if (!validateTypeFromTokens())
                            allOk = false;
                        skip(); // )
                    }
                    if (tokens[p].kind == TokKind::Comma)
                        skip();
                }
                skip(); // }
                if (p < tokens.size() && tokens[p].kind == TokKind::Semi)
                    skip();
            }
            else
            {
                skip();
            }
        }

        return allOk;
    }

    void TypedefParser::parse()
    {
        while (!at(TokKind::Eof) && ok)
        {
            if (at(TokKind::Struct))
                parseStruct();
            else if (at(TokKind::Enum))
                parseEnum();
            else
            {
                ok = false;
                pos++;
            }
        }
    }

    void Compiler::clear_types() noexcept
    {
        types = TypeRegistry{};
    }

    bool Compiler::add_types(const std::string &typedefs, std::vector<Diagnostic> &diagnostics) noexcept
    {
        try
        {
            if (typedefs.empty())
                return true;
            auto tokens = tokenize_typedefs(typedefs);
            TypedefParser parser{tokens, 0, types, diagnostics};
            parser.parse();
            if (!parser.ok)
                return false;
            return parser.validateTypes();
        }
        catch (...)
        {
            return false;
        }
    }

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

    static void setDirectiveError(DirectiveInfo &info, const std::string &message, int start = 0, int end = 0)
    {
        if (info.hasError)
            return;
        info.hasError = true;
        info.errorMessage = message;
        info.errorStart = start;
        info.errorEnd = end;
    }

    static void parseDirectiveOptions(const std::string &optPart,
                                      const std::set<std::string> &allowedStringKeys,
                                      const std::set<std::string> &allowedIdentKeys,
                                      std::map<std::string, std::string> &outValues,
                                      DirectiveInfo &info)
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
            {
                setDirectiveError(info, "invalid option key '" + key + "'", (int)keyStart, (int)p);
                return;
            }
            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;
            if (p >= optPart.size() || optPart[p] != '=')
            {
                setDirectiveError(info, "expected '=' after option key '" + key + "'", (int)keyStart, (int)p);
                return;
            }
            ++p;
            while (p < optPart.size() && std::isspace(static_cast<unsigned char>(optPart[p])))
                ++p;

            if (seen.count(key))
            {
                setDirectiveError(info, "duplicate option '" + key + "'", (int)keyStart, (int)p);
                return;
            }
            seen.insert(key);

            if (allowedStringKeys.count(key) == 0 && allowedIdentKeys.count(key) == 0)
            {
                setDirectiveError(info, "unknown option '" + key + "'", (int)keyStart, (int)p);
                return;
            }

            std::string value;
            if (allowedStringKeys.count(key) != 0)
            {
                size_t before = p;
                value = parseStringLiteral(optPart, p);
                if (before == p)
                {
                    setDirectiveError(info, "option '" + key + "' expects a quoted string value", (int)before, (int)before + 1);
                    return;
                }
            }
            else
            {
                size_t valueStart = p;
                while (p < optPart.size() && !std::isspace(static_cast<unsigned char>(optPart[p])))
                    ++p;
                value = trim(optPart.substr(valueStart, p - valueStart));
                if (!isIdentifier(value))
                {
                    setDirectiveError(info, "option '" + key + "' expects an identifier value", (int)valueStart, (int)p);
                    return;
                }
            }

            outValues[key] = value;
        }
    }

    static void parseDirectiveFlags(const std::string &optPart,
                                    const std::set<std::string> &allowedFlags,
                                    std::set<std::string> &outFlags,
                                    DirectiveInfo &info)
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
            {
                setDirectiveError(info, "invalid option '" + flag + "'", (int)start, (int)p);
                return;
            }
            if (allowedFlags.count(flag) == 0)
            {
                setDirectiveError(info, "unknown option '" + flag + "'", (int)start, (int)p);
                return;
            }
            if (outFlags.count(flag) != 0)
            {
                setDirectiveError(info, "duplicate option '" + flag + "'", (int)start, (int)p);
                return;
            }
            outFlags.insert(flag);
        }
    }

    DirectiveInfo classifyDirective(const std::string &s)
    {
        std::string t = trim(s);
        DirectiveInfo info;

        if (t == "else")
        {
            info.kind = DirectiveKind::Else;
            return info;
        }
        if (t == "endif")
        {
            info.kind = DirectiveKind::Endif;
            return info;
        }
        if (t == "end for")
        {
            info.kind = DirectiveKind::EndFor;
            return info;
        }
        if (t == "end switch")
        {
            info.kind = DirectiveKind::EndSwitch;
            return info;
        }
        if (startsWith(t, "end case"))
        {
            if (trim(t.substr(8)).empty())
            {
                info.kind = DirectiveKind::EndCase;
            }
            else
            {
                info.kind = DirectiveKind::EndCase;
                setDirectiveError(info, "unexpected trailing tokens in end case directive");
            }
            return info;
        }

        if (startsWith(t, "for "))
        {
            info.kind = DirectiveKind::For;
            std::string mainPart = t;
            std::string optPart;
            size_t pipe = t.find('|');
            if (pipe != std::string::npos)
            {
                mainPart = trim(t.substr(0, pipe));
                optPart = trim(t.substr(pipe + 1));
                if (optPart.empty())
                {
                    setDirectiveError(info, "for options list is empty after '|'");
                    return info;
                }
            }
            std::string rest = mainPart.substr(4);
            size_t inPos = rest.find(" in ");
            if (inPos != std::string::npos)
            {
                info.forVar = trim(rest.substr(0, inPos));
                info.forCollectionText = trim(rest.substr(inPos + 4));
                info.forCollection = parseExpression(info.forCollectionText);
            }
            else
            {
                setDirectiveError(info, "invalid for directive; expected 'for <item> in <collection>'");
                return info;
            }

            if (!isIdentifier(info.forVar))
            {
                setDirectiveError(info, "invalid loop variable name '" + info.forVar + "'");
                return info;
            }
            if (!isPathExpressionText(info.forCollectionText))
            {
                setDirectiveError(info, "for collection must be a variable or member path");
                return info;
            }
            if (pipe == std::string::npos)
            {
                if (rest.find("sep=") != std::string::npos ||
                    rest.find("followedBy=") != std::string::npos ||
                    rest.find("precededBy=") != std::string::npos ||
                    rest.find("iteratorVar=") != std::string::npos)
                {
                    setDirectiveError(info, "for options require '|' before option list");
                    return info;
                }
            }
            if (!optPart.empty())
            {
                std::map<std::string, std::string> values;
                parseDirectiveOptions(optPart,
                                      {"sep", "followedBy", "precededBy"},
                                      {"iteratorVar"},
                                      values,
                                      info);
                if (info.hasError)
                    return info;
                info.sep = values["sep"];
                info.followedBy = values["followedBy"];
                info.precededBy = values["precededBy"];
                info.iteratorVar = values["iteratorVar"];
            }
            return info;
        }

        if (startsWith(t, "if "))
        {
            info.kind = DirectiveKind::If;
            std::string condText = trim(t.substr(3));
            if (startsWith(condText, "not "))
            {
                info.ifNegated = true;
                condText = trim(condText.substr(4));
            }
            info.ifExprText = condText;
            if (!isPathExpressionText(condText))
            {
                setDirectiveError(info, "if condition must be a variable or member path");
                return info;
            }
            info.ifCond = parseExpression(condText);
            return info;
        }

        if (startsWith(t, "switch "))
        {
            info.kind = DirectiveKind::Switch;
            std::string rest = trim(t.substr(7));
            std::string exprText = rest;
            std::string optPart;
            size_t pipe = rest.find('|');
            if (pipe != std::string::npos)
            {
                exprText = trim(rest.substr(0, pipe));
                optPart = trim(rest.substr(pipe + 1));
                if (optPart.empty())
                {
                    setDirectiveError(info, "switch options list is empty after '|'");
                    return info;
                }
            }
            if (exprText.empty())
            {
                setDirectiveError(info, "switch directive requires an expression");
                return info;
            }
            if (!isPathExpressionText(exprText))
            {
                setDirectiveError(info, "switch expression must be a variable or member path");
                return info;
            }
            info.switchExpr = parseExpression(exprText);
            if (!optPart.empty())
            {
                std::set<std::string> flags;
                parseDirectiveFlags(optPart, {"checkExhaustive"}, flags, info);
                if (info.hasError)
                    return info;
                info.switchCheckExhaustive = flags.count("checkExhaustive") != 0;
            }
            return info;
        }

        if (startsWith(t, "case "))
        {
            info.kind = DirectiveKind::Case;
            std::string rest = trim(t.substr(5));
            if (rest.find('|') != std::string::npos)
            {
                setDirectiveError(info, "case directive does not accept options");
                return info;
            }
            size_t paren = rest.find('(');
            if (paren != std::string::npos)
            {
                info.caseTag = trim(rest.substr(0, paren));
                size_t cparen = rest.find(')', paren);
                if (cparen == std::string::npos)
                {
                    setDirectiveError(info, "case binding is missing closing ')'");
                    return info;
                }
                info.caseBinding = rest.substr(paren + 1, cparen - paren - 1);
                if (!trim(rest.substr(cparen + 1)).empty())
                {
                    setDirectiveError(info, "unexpected trailing tokens in case directive");
                    return info;
                }
                if (!trim(info.caseBinding).empty() && !isIdentifier(trim(info.caseBinding)))
                {
                    setDirectiveError(info, "invalid case binding variable name");
                    return info;
                }
            }
            else
            {
                info.caseTag = rest;
            }
            if (!isIdentifier(info.caseTag))
            {
                setDirectiveError(info, "invalid case tag '" + info.caseTag + "'");
                return info;
            }
            info.caseBinding = trim(info.caseBinding);
            return info;
        }

        if (startsWith(t, "render "))
        {
            info.kind = DirectiveKind::Render;
            std::string rest = t.substr(7);
            std::string mainPart = rest;
            std::string optPart;
            size_t pipe = rest.find('|');
            if (pipe != std::string::npos)
            {
                mainPart = trim(rest.substr(0, pipe));
                optPart = trim(rest.substr(pipe + 1));
                if (optPart.empty())
                {
                    setDirectiveError(info, "render options list is empty after '|'");
                    return info;
                }
            }
            size_t viaPos = mainPart.find(" via ");
            if (viaPos != std::string::npos)
            {
                info.renderExprText = trim(mainPart.substr(0, viaPos));
                info.renderExpr = parseExpression(info.renderExprText);
                info.renderFunc = trim(mainPart.substr(viaPos + 5));
            }
            else
            {
                setDirectiveError(info, "invalid render directive; expected 'render <collection> via <function>'");
                return info;
            }
            if (!isPathExpressionText(info.renderExprText))
            {
                setDirectiveError(info, "render expression must be a variable or member path");
                return info;
            }
            if (!isIdentifier(info.renderFunc))
            {
                setDirectiveError(info, "invalid render function name '" + info.renderFunc + "'");
                return info;
            }
            if (pipe == std::string::npos)
            {
                if (rest.find("sep=") != std::string::npos ||
                    rest.find("followedBy=") != std::string::npos ||
                    rest.find("precededBy=") != std::string::npos)
                {
                    setDirectiveError(info, "render options require '|' before option list");
                    return info;
                }
            }
            if (!optPart.empty())
            {
                std::map<std::string, std::string> values;
                parseDirectiveOptions(optPart,
                                      {"sep", "followedBy", "precededBy"},
                                      {},
                                      values,
                                      info);
                if (info.hasError)
                    return info;
                info.renderSep = values["sep"];
                info.renderFollowedBy = values["followedBy"];
                info.renderPrecededBy = values["precededBy"];
            }
            return info;
        }

        // Detect function call: name(arg1, arg2, ...)
        {
            size_t parenOpen = t.find('(');
            if (parenOpen != std::string::npos && t.back() == ')')
            {
                std::string name = trim(t.substr(0, parenOpen));
                // Ensure name is a valid identifier (no spaces, dots etc.)
                bool validName = !name.empty();
                for (char c : name)
                {
                    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                    {
                        validName = false;
                        break;
                    }
                }
                if (validName)
                {
                    info.kind = DirectiveKind::FunctionCall;
                    info.funcCallName = name;
                    std::string argsStr = t.substr(parenOpen + 1, t.size() - parenOpen - 2);
                    if (!trim(argsStr).empty())
                    {
                        // Split by comma, respecting parentheses
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
                        {
                            info.funcCallArgs.push_back(parseExpression(arg));
                        }
                    }
                    return info;
                }
            }
        }

        info.kind = DirectiveKind::Expr;
        info.expr = parseExpression(t);
        return info;
    }

    static bool isStructuralDirective(DirectiveKind k)
    {
        return k != DirectiveKind::Expr && k != DirectiveKind::FunctionCall;
    }

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
                    if (ls.info.kind == DirectiveKind::Expr || ls.info.kind == DirectiveKind::FunctionCall)
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

    std::vector<ASTNode> InlineParser::parse()
    {
        std::vector<ASTNode> nodes;
        while (pos < segs.size())
        {
            auto &s = segs[pos];
            if (!s.isDirective)
            {
                if (!s.text.empty())
                    nodes.push_back(TextNode{s.text});
                ++pos;
            }
            else if (s.info.kind == DirectiveKind::Expr)
            {
                nodes.push_back(InterpolationNode{s.info.expr});
                ++pos;
            }
            else if (s.info.kind == DirectiveKind::FunctionCall)
            {
                auto callNode = std::make_shared<FunctionCallNode>();
                callNode->functionName = s.info.funcCallName;
                callNode->arguments = s.info.funcCallArgs;
                nodes.push_back(std::move(callNode));
                ++pos;
            }
            else if (s.info.kind == DirectiveKind::For)
            {
                auto forNode = std::make_shared<ForNode>();
                forNode->varName = s.info.forVar;
                forNode->iteratorVarName = s.info.iteratorVar;
                forNode->collectionExpr = s.info.forCollection;
                forNode->sep = s.info.sep;
                forNode->followedBy = s.info.followedBy;
                forNode->precededBy = s.info.precededBy;
                forNode->isBlock = false;
                ++pos;
                forNode->body = parse();
                nodes.push_back(std::move(forNode));
            }
            else if (s.info.kind == DirectiveKind::EndFor)
            {
                ++pos;
                return nodes;
            }
            else if (s.info.kind == DirectiveKind::If)
            {
                auto ifNode = std::make_shared<IfNode>();
                ifNode->condExpr = s.info.ifCond;
                ifNode->negated = s.info.ifNegated;
                ifNode->condText = s.info.ifExprText;
                ifNode->isBlock = false;
                ++pos;
                ifNode->thenBody = parse();
                if (pos < segs.size() && segs[pos].isDirective && segs[pos].info.kind == DirectiveKind::Else)
                {
                    ++pos;
                    ifNode->elseBody = parse();
                }
                nodes.push_back(std::move(ifNode));
            }
            else if (s.info.kind == DirectiveKind::Else)
            {
                return nodes;
            }
            else if (s.info.kind == DirectiveKind::Endif)
            {
                ++pos;
                return nodes;
            }
            else if (s.info.kind == DirectiveKind::Switch)
            {
                auto switchNode = std::make_shared<SwitchNode>();
                switchNode->expr = s.info.switchExpr;
                switchNode->checkExhaustive = s.info.switchCheckExhaustive;
                switchNode->isBlock = false;
                ++pos;
                while (pos < segs.size())
                {
                    auto &cs = segs[pos];
                    if (!cs.isDirective)
                    {
                        ++pos;
                        continue;
                    }
                    if (cs.info.kind == DirectiveKind::EndSwitch)
                    {
                        ++pos;
                        break;
                    }
                    if (cs.info.kind == DirectiveKind::Case)
                    {
                        CaseNode cn;
                        cn.tag = cs.info.caseTag;
                        cn.bindingName = cs.info.caseBinding;
                        ++pos;
                        cn.body = parse();
                        switchNode->cases.push_back(std::move(cn));
                    }
                    else
                    {
                        ++pos;
                    }
                }
                nodes.push_back(std::move(switchNode));
            }
            else if (s.info.kind == DirectiveKind::EndCase)
            {
                ++pos;
                return nodes;
            }
            else if (s.info.kind == DirectiveKind::EndSwitch)
            {
                return nodes;
            }
            else
            {
                ++pos;
            }
        }
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
                    if (seg.info.kind == DirectiveKind::For)
                    {
                        auto forNode = std::make_shared<ForNode>();
                        forNode->varName = seg.info.forVar;
                        forNode->iteratorVarName = seg.info.iteratorVar;
                        forNode->collectionExpr = seg.info.forCollection;
                        forNode->sep = seg.info.sep;
                        forNode->followedBy = seg.info.followedBy;
                        forNode->precededBy = seg.info.precededBy;
                        forNode->isBlock = true;
                        forNode->insertCol = tl.indent;
                        ++pos;
                        forNode->body = parseBlock(tl.indent);
                        nodes.push_back(std::move(forNode));
                    }
                    else if (seg.info.kind == DirectiveKind::EndFor)
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (seg.info.kind == DirectiveKind::If)
                    {
                        auto ifNode = std::make_shared<IfNode>();
                        ifNode->condExpr = seg.info.ifCond;
                        ifNode->negated = seg.info.ifNegated;
                        ifNode->condText = seg.info.ifExprText;
                        ifNode->isBlock = true;
                        ifNode->insertCol = tl.indent;
                        ++pos;
                        ifNode->thenBody = parseBlock(tl.indent);
                        if (pos < lines.size() && lines[pos].isBlockLine)
                        {
                            for (auto &s2 : lines[pos].segments)
                            {
                                if (s2.isDirective && s2.info.kind == DirectiveKind::Else)
                                {
                                    ++pos;
                                    ifNode->elseBody = parseBlock(tl.indent);
                                    break;
                                }
                            }
                        }
                        nodes.push_back(std::move(ifNode));
                    }
                    else if (seg.info.kind == DirectiveKind::Else)
                    {
                        return nodes;
                    }
                    else if (seg.info.kind == DirectiveKind::Endif)
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (seg.info.kind == DirectiveKind::Switch)
                    {
                        auto switchNode = std::make_shared<SwitchNode>();
                        switchNode->expr = seg.info.switchExpr;
                        switchNode->checkExhaustive = seg.info.switchCheckExhaustive;
                        switchNode->isBlock = true;
                        switchNode->insertCol = tl.indent;
                        ++pos;
                        while (pos < lines.size())
                        {
                            if (lines[pos].isBlockLine)
                            {
                                bool done = false;
                                for (auto &s2 : lines[pos].segments)
                                {
                                    if (s2.isDirective && s2.info.kind == DirectiveKind::EndSwitch)
                                    {
                                        ++pos;
                                        done = true;
                                        break;
                                    }
                                    if (s2.isDirective && s2.info.kind == DirectiveKind::Case)
                                    {
                                        CaseNode cn;
                                        cn.tag = s2.info.caseTag;
                                        cn.bindingName = s2.info.caseBinding;
                                        ++pos;
                                        cn.body = parseBlock(tl.indent);
                                        switchNode->cases.push_back(std::move(cn));
                                        break;
                                    }
                                    if (s2.isDirective && s2.info.kind == DirectiveKind::Render)
                                    {
                                        CaseNode cn;
                                        cn.tag = s2.info.renderExprText;
                                        cn.bindingName = "__payload";
                                        auto callNode = std::make_shared<FunctionCallNode>();
                                        callNode->functionName = s2.info.renderFunc;
                                        callNode->arguments.push_back(Variable{"__payload"});
                                        cn.body.push_back(std::move(callNode));
                                        switchNode->cases.push_back(std::move(cn));
                                        ++pos;
                                        break;
                                    }
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
                    else if (seg.info.kind == DirectiveKind::EndSwitch)
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (seg.info.kind == DirectiveKind::Case)
                    {
                        return nodes;
                    }
                    else if (seg.info.kind == DirectiveKind::EndCase)
                    {
                        ++pos;
                        return nodes;
                    }
                    else if (seg.info.kind == DirectiveKind::Render)
                    {
                        auto renderNode = std::make_shared<RenderViaNode>();
                        renderNode->collectionExpr = seg.info.renderExpr;
                        renderNode->functionName = seg.info.renderFunc;
                        renderNode->sep = seg.info.renderSep;
                        renderNode->followedBy = seg.info.renderFollowedBy;
                        renderNode->precededBy = seg.info.renderPrecededBy;
                        renderNode->isBlock = true;
                        renderNode->insertCol = tl.indent;
                        nodes.push_back(std::move(renderNode));
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
                    if (s.isDirective && isStructuralDirective(s.info.kind))
                    {
                        hasInline = true;
                        break;
                    }
                }
                if (hasInline)
                {
                    InlineParser ip{tl.segments};
                    auto inlineNodes = ip.parse();
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
                        else if (seg.info.kind == DirectiveKind::FunctionCall)
                        {
                            auto callNode = std::make_shared<FunctionCallNode>();
                            callNode->functionName = seg.info.funcCallName;
                            callNode->arguments = seg.info.funcCallArgs;
                            nodes.push_back(std::move(callNode));
                        }
                        else
                        {
                            nodes.push_back(InterpolationNode{seg.info.expr});
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

    struct ValidationFrame
    {
        DirectiveKind kind;
        std::map<std::string, TypeRef> vars;
        std::set<std::string> narrowedOptionals;
        std::set<std::string> absentOptionals;
        std::string optionalGuardPath;
        bool optionalGuardNegated = false;
        std::optional<TypeRef> switchExprType;
        bool switchCheckExhaustive = false;
        std::set<std::string> switchCaseTags;
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
            if (it->kind != DirectiveKind::Switch || !it->switchExprType.has_value())
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
        ValidationFrame root{DirectiveKind::Expr};
        for (const auto &param : f.params)
        {
            root.vars[param.name] = param.type;
        }
        frames.push_back(std::move(root));

        auto popTo = [&](DirectiveKind kind)
        {
            while (frames.size() > 1)
            {
                DirectiveKind top = frames.back().kind;
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

                if (seg.info.hasError)
                {
                    addTemplateDiagnostic(diags, bodyStartLine, li, seg, seg.info.errorMessage, seg.info.errorStart, seg.info.errorEnd);
                    continue;
                }

                auto activeNarrowing = collectActiveOptionalNarrowing(frames);
                auto activeAbsent = collectActiveAbsentOptionals(frames);
                std::string err;
                TypeRef exprType;

                switch (seg.info.kind)
                {
                case DirectiveKind::Expr:
                {
                    std::string path = expressionToPath(seg.info.expr);
                    if (!validatePathExpressionType(path, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                    }
                    break;
                }
                case DirectiveKind::FunctionCall:
                {
                    for (const auto &argExpr : seg.info.funcCallArgs)
                    {
                        std::string path = expressionToPath(argExpr);
                        if (!validatePathExpressionType(path, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                        {
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                            break;
                        }
                    }
                    break;
                }
                case DirectiveKind::For:
                {
                    if (!validatePathExpressionType(seg.info.forCollectionText, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                        break;
                    }
                    if (!std::holds_alternative<std::shared_ptr<ListType>>(exprType))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "for collection must be a list");
                        break;
                    }

                    auto listType = std::get<std::shared_ptr<ListType>>(exprType);
                    ValidationFrame frame{DirectiveKind::For};
                    frame.vars[seg.info.forVar] = listType->elementType;
                    if (!seg.info.iteratorVar.empty())
                    {
                        frame.vars[seg.info.iteratorVar] = IntType{};
                    }
                    frames.push_back(std::move(frame));
                    break;
                }
                case DirectiveKind::EndFor:
                    popTo(DirectiveKind::For);
                    break;
                case DirectiveKind::If:
                {
                    if (!validatePathExpressionType(seg.info.ifExprText, frames, types, activeNarrowing, activeAbsent, true, exprType, err))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                        break;
                    }

                    ValidationFrame frame{DirectiveKind::If};
                    if (seg.info.ifNegated)
                    {
                        if (isOptionalType(exprType))
                        {
                            frame.optionalGuardPath = seg.info.ifExprText;
                            frame.optionalGuardNegated = true;
                            frame.absentOptionals.insert(seg.info.ifExprText);
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
                            frame.optionalGuardPath = seg.info.ifExprText;
                            frame.optionalGuardNegated = false;
                            if (activeAbsent.count(seg.info.ifExprText) != 0)
                            {
                                addTemplateDiagnostic(diags, bodyStartLine, li, seg,
                                                      "cannot guard optional value '" + seg.info.ifExprText + "' because it does not exist in this branch");
                            }
                            frame.narrowedOptionals.insert(seg.info.ifExprText);
                        }
                        else if (!std::holds_alternative<BoolType>(exprType))
                        {
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, "if condition must be bool or optional");
                        }
                    }
                    frames.push_back(std::move(frame));
                    break;
                }
                case DirectiveKind::Else:
                {
                    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                    {
                        if (it->kind == DirectiveKind::If)
                        {
                            it->narrowedOptionals.clear();
                            it->absentOptionals.clear();
                            if (!it->optionalGuardPath.empty())
                            {
                                if (it->optionalGuardNegated)
                                {
                                    it->narrowedOptionals.insert(it->optionalGuardPath);
                                }
                                else
                                {
                                    it->absentOptionals.insert(it->optionalGuardPath);
                                }
                            }
                            break;
                        }
                    }
                    break;
                }
                case DirectiveKind::Endif:
                    popTo(DirectiveKind::If);
                    break;
                case DirectiveKind::Switch:
                {
                    std::string switchPath = expressionToPath(seg.info.switchExpr);
                    if (!validatePathExpressionType(switchPath, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                    }
                    ValidationFrame frame{DirectiveKind::Switch};
                    frame.switchExprType = exprType;
                    frame.switchCheckExhaustive = seg.info.switchCheckExhaustive;
                    frames.push_back(std::move(frame));
                    break;
                }
                case DirectiveKind::EndSwitch:
                {
                    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                    {
                        if (it->kind != DirectiveKind::Switch)
                            continue;
                        if (it->switchCheckExhaustive)
                        {
                            const EnumDef *switchEnum = nullptr;
                            if (it->switchExprType.has_value())
                            {
                                switchEnum = resolveEnumFromType(it->switchExprType.value(), types);
                            }
                            if (switchEnum)
                            {
                                std::vector<std::string> missing;
                                for (const auto &variant : switchEnum->variants)
                                {
                                    if (it->switchCaseTags.count(variant.tag) == 0)
                                    {
                                        missing.push_back(variant.tag);
                                    }
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
                                    addTemplateDiagnostic(diags,
                                                          bodyStartLine,
                                                          li,
                                                          seg,
                                                          "switch is missing cases for enum '" + switchEnum->name + "': " + missingText);
                                }
                            }
                        }
                        break;
                    }
                    popTo(DirectiveKind::Switch);
                    break;
                }
                case DirectiveKind::Case:
                {
                    ValidationFrame frame{DirectiveKind::Case};
                    const EnumDef *switchEnum = findNearestSwitchEnum(frames, types);
                    const VariantDef *variant = findEnumVariant(switchEnum, seg.info.caseTag);
                    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
                    {
                        if (it->kind == DirectiveKind::Switch)
                        {
                            it->switchCaseTags.insert(seg.info.caseTag);
                            break;
                        }
                    }
                    if (!switchEnum)
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "case directive must be inside a switch over an enum");
                    }
                    else if (!variant)
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "unknown case tag '" + seg.info.caseTag + "' for enum '" + switchEnum->name + "'");
                    }
                    else if (!seg.info.caseBinding.empty())
                    {
                        if (!variant->payload.has_value())
                        {
                            addTemplateDiagnostic(diags, bodyStartLine, li, seg, "case tag '" + seg.info.caseTag + "' has no payload to bind");
                        }
                        else
                        {
                            frame.vars[seg.info.caseBinding] = variant->payload.value();
                        }
                    }
                    frames.push_back(std::move(frame));
                    break;
                }
                case DirectiveKind::EndCase:
                    popTo(DirectiveKind::Case);
                    break;
                case DirectiveKind::Render:
                {
                    const EnumDef *switchEnum = findNearestSwitchEnum(frames, types);
                    const VariantDef *variant = findEnumVariant(switchEnum, seg.info.renderExprText);
                    if (switchEnum && variant)
                    {
                        // Pointfree variant visitor inside switch: @render Tag via fn@
                        break;
                    }
                    if (!validatePathExpressionType(seg.info.renderExprText, frames, types, activeNarrowing, activeAbsent, false, exprType, err))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, err);
                        break;
                    }
                    if (!std::holds_alternative<std::shared_ptr<ListType>>(exprType))
                    {
                        addTemplateDiagnostic(diags, bodyStartLine, li, seg, "render expression must be a list");
                    }
                    break;
                }
                }
            }
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
                          std::string *outBodyText)
    {
        // Find "template " line
        size_t lineStart = pos;
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos)
            return false;
        std::string firstLine = trim(text.substr(lineStart, nl - lineStart));
        if (!startsWith(firstLine, "template "))
            return false;

        // Find END
        size_t endPos = text.find("\nEND", nl);
        if (endPos == std::string::npos)
            return false;

        std::string body = text.substr(nl + 1, endPos - nl - 1);

        std::string afterTemplate = firstLine.substr(9);
        size_t parenOpen = afterTemplate.find('(');
        if (parenOpen == std::string::npos)
            return false;
        func.name = trim(afterTemplate.substr(0, parenOpen));

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
                std::string bodyText;
                if (!parseOneTemplate(templateString, pos, func, &bodyStartLine, &bodyText))
                {
                    if (!foundAny)
                        return false;
                    break;
                }
                auto templateLines = parseTemplateLines(bodyText);
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

    // ═══════════════════════════════════════════════════════════════════
    // Function Lookup  (get_function)
    // ═══════════════════════════════════════════════════════════════════

    bool CompilerOutput::get_function(const std::string &functionName,
                                      FunctionSymbol &functionSymbol,
                                      std::string &error) const noexcept
    {
        try
        {
            for (auto &f : functions)
            {
                if (f.name == functionName)
                {
                    functionSymbol.function = f;
                    functionSymbol.types = types;
                    functionSymbol.allFunctions = functions;
                    return true;
                }
            }
            error = "function '" + functionName + "' not found";
            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // JSON Binding  (bind)
    // ═══════════════════════════════════════════════════════════════════

    bool FunctionSymbol::render(const nlohmann::json &input,
                               std::string &output,
                               std::string &error) const noexcept
    {
        try
        {
            Program program;
            program.function = function;
            program.types = types;
            program.allFunctions = allFunctions;

            if (function.params.empty())
            {
                program.boundArgs = input;
            }
            else if (function.params.size() == 1)
            {
                const auto &p = function.params[0];
                bool isList = std::holds_alternative<std::shared_ptr<ListType>>(p.type);
                nlohmann::json value;
                if (isList)
                {
                    // list param: if input is a length-1 array whose single element is
                    // also an array, treat that inner array as the list (unary wrap).
                    // Otherwise treat input itself as the list.
                    if (input.is_array() && input.size() == 1 && input[0].is_array())
                        value = input[0];
                    else
                        value = input;
                }
                else
                {
                    // non-list param: plain value, or unwrap a length-1 array
                    if (input.is_array())
                    {
                        if (input.size() == 1)
                            value = input[0];
                        else
                        {
                            error = "Expected 1 argument for function '" + function.name +
                                    "', got " + std::to_string(input.size());
                            return false;
                        }
                    }
                    else
                    {
                        value = input;
                    }
                }
                program.boundArgs = nlohmann::json::object();
                program.boundArgs[p.name] = value;
            }
            else
            {
                // multi-param: input must be a positional array of exactly params.size() elements
                if (!input.is_array() || input.size() != function.params.size())
                {
                    const std::size_t got = input.is_array() ? input.size() : 0;
                    const std::size_t expected = function.params.size();
                    error = "Expected " + std::to_string(expected) +
                            " argument" + (expected == 1 ? "" : "s") +
                            " for function '" + function.name +
                            "', got " + std::to_string(got);
                    return false;
                }
                program.boundArgs = nlohmann::json::object();
                for (std::size_t i = 0; i < function.params.size(); ++i)
                    program.boundArgs[function.params[i].name] = input[i];
            }
            // Validate that required struct fields are present in input JSON
            std::function<const StructDef *(const TypeRef &)> resolveStruct = [&](const TypeRef &tr) -> const StructDef *
            {
                if (std::holds_alternative<NamedType>(tr))
                {
                    auto &nt = std::get<NamedType>(tr);
                    auto it = types.nameIndex.find(nt.name);
                    if (it != types.nameIndex.end() && it->second.kind == TypeKind::Struct)
                        return &types.structs[it->second.index];
                    return nullptr;
                }
                if (auto spOpt = std::get_if<std::shared_ptr<OptionalType>>(&tr))
                {
                    return resolveStruct((*spOpt)->innerType);
                }
                return nullptr;
            };

            if (function.params.size() == 1)
            {
                const auto &p = function.params[0];
                const StructDef *sd = resolveStruct(p.type);
                if (sd)
                {
                    // boundArgs should contain the param name mapping to the object
                    if (!program.boundArgs.contains(p.name) || !program.boundArgs[p.name].is_object())
                    {
                        error = "Missing field '" + sd->fields[0].name + "' in input data for function '" + function.name + "'";
                        return false;
                    }
                    const auto &obj = program.boundArgs[p.name];
                    for (auto &fd : sd->fields)
                    {
                        if (isOptionalType(fd.type))
                            continue;
                        if (!obj.contains(fd.name) || obj[fd.name].is_null())
                        {
                            error = "Missing field '" + fd.name + "' in input data for function '" + function.name + "'";
                            return false;
                        }
                    }
                }
            }
            else if (function.params.size() > 1)
            {
                for (auto &p : function.params)
                {
                    const StructDef *sd = resolveStruct(p.type);
                    if (!sd)
                        continue;
                    if (!program.boundArgs.contains(p.name) || !program.boundArgs[p.name].is_object())
                    {
                        error = "Missing field '" + sd->fields[0].name + "' in input data for function '" + function.name + "'";
                        return false;
                    }
                    const auto &obj = program.boundArgs[p.name];
                    for (auto &fd : sd->fields)
                    {
                        if (isOptionalType(fd.type))
                            continue;
                        if (!obj.contains(fd.name) || obj[fd.name].is_null())
                        {
                            error = "Missing field '" + fd.name + "' in input data for function '" + function.name + "'";
                            return false;
                        }
                    }
                }
            }

            output = program.run();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Rendering  (run)
    // ═══════════════════════════════════════════════════════════════════

    struct RenderContext
    {
        const TypeRegistry &types;
        const std::vector<TemplateFunction> &allFunctions;
        std::vector<std::pair<std::string, nlohmann::json>> bindings;

        void pushBinding(const std::string &name, const nlohmann::json &val)
        {
            bindings.push_back({name, val});
        }
        void popBinding()
        {
            bindings.pop_back();
        }

        nlohmann::json resolve(const Expression &expr) const
        {
            return std::visit([&](auto &&arg) -> nlohmann::json
                              {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Variable>) {
                for (int i = (int)bindings.size() - 1; i >= 0; --i) {
                    if (bindings[i].first == arg.name) {
                        return bindings[i].second;
                    }
                }
                return nullptr;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<FieldAccess>>) {
                auto base = resolve(arg->base);
                if (base.is_object() && base.contains(arg->field)) {
                    return base[arg->field];
                }
                return nullptr;
            }
            return nullptr; }, expr);
        }

        std::string jsonToString(const nlohmann::json &val) const
        {
            if (val.is_string())
                return val.get<std::string>();
            if (val.is_number_integer())
                return std::to_string(val.get<int64_t>());
            if (val.is_number_float())
            {
                double d = val.get<double>();
                if (d == std::floor(d))
                    return std::to_string((int64_t)d);
                return std::to_string(d);
            }
            if (val.is_boolean())
                return val.get<bool>() ? "true" : "false";
            if (val.is_null())
                return "";
            return val.dump();
        }
    };

    static std::string renderNodes(const std::vector<ASTNode> &nodes, RenderContext &ctx);
    static std::string renderBlockBody(const std::vector<ASTNode> &nodes, RenderContext &ctx, int insertCol);

    static std::string renderNodes(const std::vector<ASTNode> &nodes, RenderContext &ctx)
    {
        std::string result;
        for (auto &node : nodes)
        {
            std::visit([&](auto &&arg)
                       {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, TextNode>) {
                result += arg.text;
            } else if constexpr (std::is_same_v<T, InterpolationNode>) {
                auto val = ctx.resolve(arg.expr);
                result += ctx.jsonToString(val);
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>) {
                auto collection = ctx.resolve(arg->collectionExpr);
                if (collection.is_array()) {
                    for (size_t i = 0; i < collection.size(); ++i) {
                        ctx.pushBinding(arg->varName, collection[i]);
                        if (!arg->iteratorVarName.empty()) {
                            ctx.pushBinding(arg->iteratorVarName, (int64_t)i);
                        }
                        std::string iterResult;
                        if (arg->isBlock) {
                            iterResult = renderBlockBody(arg->body, ctx, arg->insertCol);
                        } else {
                            iterResult = renderNodes(arg->body, ctx);
                        }
                        if (!arg->iteratorVarName.empty()) {
                            ctx.popBinding();
                        }
                        ctx.popBinding();
                        // For block for-loops with sep: if the body is a single
                        // line (trailing \n is the only newline), strip it so
                        // sep replaces the line boundary. Multi-line bodies keep
                        // their trailing \n and sep is added on top.
                        bool strippedNl = false;
                        if (arg->isBlock && !arg->sep.empty() &&
                            !iterResult.empty() && iterResult.back() == '\n') {
                            // Count newlines — single trailing \n means single-line body
                            auto nlCount = std::count(iterResult.begin(), iterResult.end(), '\n');
                            if (nlCount == 1) {
                                iterResult.pop_back();
                                strippedNl = true;
                            }
                        }
                        result += arg->precededBy;
                        result += iterResult;
                        if (i + 1 < collection.size()) {
                            result += arg->sep;
                        } else {
                            if (!arg->followedBy.empty() && !collection.empty()) {
                                result += arg->followedBy;
                            }
                            // Restore trailing newline for last iteration
                            if (strippedNl) {
                                result += "\n";
                            }
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>) {
                auto val = ctx.resolve(arg->condExpr);
                bool cond = false;
                if (arg->negated) {
                    cond = val.is_boolean() ? !val.template get<bool>() : val.is_null();
                } else if (val.is_boolean()) {
                    cond = val.template get<bool>();
                } else {
                    cond = !val.is_null();
                }
                if (cond) {
                    if (arg->isBlock) {
                        result += renderBlockBody(arg->thenBody, ctx, arg->insertCol);
                    } else {
                        result += renderNodes(arg->thenBody, ctx);
                    }
                } else {
                    if (arg->isBlock) {
                        result += renderBlockBody(arg->elseBody, ctx, arg->insertCol);
                    } else {
                        result += renderNodes(arg->elseBody, ctx);
                    }
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>) {
                auto val = ctx.resolve(arg->expr);
                if (val.is_object()) {
                    for (auto &c : arg->cases) {
                        if (val.contains(c.tag)) {
                            if (!c.bindingName.empty()) {
                                ctx.pushBinding(c.bindingName, val[c.tag]);
                            }
                            if (arg->isBlock) {
                                result += renderBlockBody(c.body, ctx, arg->insertCol);
                            } else {
                                result += renderNodes(c.body, ctx);
                            }
                            if (!c.bindingName.empty()) {
                                ctx.popBinding();
                            }
                            break;
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>) {
                // Find the function by name
                const TemplateFunction *targetFunc = nullptr;
                for (auto &f : ctx.allFunctions) {
                    if (f.name == arg->functionName) {
                        targetFunc = &f;
                        break;
                    }
                }
                if (targetFunc) {
                    // Evaluate arguments and push positional bindings
                    for (size_t i = 0; i < arg->arguments.size() && i < targetFunc->params.size(); ++i) {
                        auto val = ctx.resolve(arg->arguments[i]);
                        ctx.pushBinding(targetFunc->params[i].name, val);
                    }
                    std::string callResult = renderNodes(targetFunc->body, ctx);
                    // Strip trailing newline from function result
                    if (!callResult.empty() && callResult.back() == '\n')
                        callResult.pop_back();
                    result += callResult;
                    // Pop bindings in reverse
                    for (size_t i = 0; i < arg->arguments.size() && i < targetFunc->params.size(); ++i) {
                        ctx.popBinding();
                    }
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>) {
                auto collection = ctx.resolve(arg->collectionExpr);
                if (collection.is_array()) {
                    for (size_t i = 0; i < collection.size(); ++i) {
                        // Find the right function overload
                        const TemplateFunction *targetFunc = nullptr;
                        std::vector<const TemplateFunction *> overloads;
                        for (auto &f : ctx.allFunctions) {
                            if (f.name == arg->functionName)
                                overloads.push_back(&f);
                        }
                        if (overloads.size() == 1) {
                            targetFunc = overloads[0];
                        } else if (overloads.size() > 1 && collection[i].is_object()) {
                            // Variant dispatch: find which overload matches the payload type
                            for (auto it = collection[i].begin(); it != collection[i].end(); ++it) {
                                const std::string &tag = it.key();
                                const auto &payload = it.value();
                                for (auto *ovl : overloads) {
                                    if (ovl->params.size() == 1) {
                                        const auto &ptype = ovl->params[0].type;
                                        bool matches = false;
                                        if (std::holds_alternative<StringType>(ptype) && payload.is_string())
                                            matches = true;
                                        else if (std::holds_alternative<IntType>(ptype) && payload.is_number())
                                            matches = true;
                                        else if (std::holds_alternative<BoolType>(ptype) && payload.is_boolean())
                                            matches = true;
                                        if (matches) {
                                            targetFunc = ovl;
                                            break;
                                        }
                                    }
                                }
                                if (targetFunc) break;
                            }
                        }
                        if (targetFunc && !targetFunc->params.empty()) {
                            nlohmann::json elemVal = collection[i];
                            // For variant dispatch with overloads, extract the payload value
                            if (overloads.size() > 1 && elemVal.is_object()) {
                                for (auto it = elemVal.begin(); it != elemVal.end(); ++it) {
                                    elemVal = it.value();
                                    break;
                                }
                            }
                            ctx.pushBinding(targetFunc->params[0].name, elemVal);
                            std::string iterResult = renderNodes(targetFunc->body, ctx);
                            if (!iterResult.empty() && iterResult.back() == '\n')
                                iterResult.pop_back();
                            ctx.popBinding();
                            result += arg->precededBy;
                            result += iterResult;
                            if (i + 1 < collection.size()) {
                                result += arg->sep;
                            } else if (!arg->followedBy.empty()) {
                                result += arg->followedBy;
                            }
                        }
                    }
                }
            } }, node);
        }
        return result;
    }

    static std::string renderBlockBody(const std::vector<ASTNode> &nodes, RenderContext &ctx, int insertCol)
    {
        std::string raw = renderNodes(nodes, ctx);

        if (raw.empty())
            return "";

        std::vector<std::string> lines;
        std::istringstream iss(raw);
        std::string line;
        while (std::getline(iss, line))
        {
            lines.push_back(line);
        }
        bool trailingNl = !raw.empty() && raw.back() == '\n';

        // Find zero-marker
        std::string zeroMarker;
        for (auto &l : lines)
        {
            if (!trim(l).empty())
            {
                size_t ws = 0;
                while (ws < l.size() && (l[ws] == ' ' || l[ws] == '\t'))
                    ws++;
                zeroMarker = l.substr(0, ws);
                break;
            }
        }

        std::string indent(insertCol, ' ');
        std::string result;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            std::string l = lines[i];
            // De-indent by zero-marker
            if (!l.empty() && !zeroMarker.empty() && l.size() >= zeroMarker.size() &&
                l.compare(0, zeroMarker.size(), zeroMarker) == 0)
            {
                l = l.substr(zeroMarker.size());
            }
            // Re-indent
            if (!l.empty())
            {
                result += indent + l;
            }
            if (i + 1 < lines.size() || trailingNl)
            {
                result += "\n";
            }
        }

        return result;
    }

    std::string Program::run() const noexcept
    {
        try
        {
            RenderContext ctx{types, allFunctions};

            for (auto &p : function.params)
            {
                if (boundArgs.contains(p.name))
                {
                    ctx.pushBinding(p.name, boundArgs[p.name]);
                }
            }

            std::string result = renderNodes(function.body, ctx);

            // Strip trailing newline — the template body naturally adds one for
            // the last line, but expected outputs don't include it
            if (!result.empty() && result.back() == '\n')
            {
                result.pop_back();
            }

            return result;
        }
        catch (...)
        {
            return "";
        }
    }

} // namespace tpp