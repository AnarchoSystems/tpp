#include <tpp/Compiler.h>
#include <tpp/Tokenizer.h>
#include <tpp/Parser.h>
#include <sstream>
#include <functional>
#include <iostream>

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
            else if (tokens[p].kind == TokKind::KwString || tokens[p].kind == TokKind::KwInt)
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
                segs.push_back({false, src.substr(i)});
                break;
            }
            if (at > i)
            {
                segs.push_back({false, src.substr(i, at - i)});
            }
            size_t end = src.find('@', at + 1);
            if (end == std::string::npos)
            {
                segs.push_back({false, src.substr(at)});
                break;
            }
            if (end == at + 1)
            {
                // Empty @@ pair — skip it; re-examine 'end' as potential opener
                i = end;
                continue;
            }
            segs.push_back({true, src.substr(at + 1, end - at - 1)});
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
            info.kind = DirectiveKind::EndCase;
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
            }
            std::string rest = mainPart.substr(4);
            size_t inPos = rest.find(" in ");
            if (inPos != std::string::npos)
            {
                info.forVar = trim(rest.substr(0, inPos));
                info.forCollection = parseExpression(rest.substr(inPos + 4));
            }
            if (!optPart.empty())
            {
                size_t p = 0;
                while (p < optPart.size())
                {
                    while (p < optPart.size() && optPart[p] == ' ')
                        ++p;
                    if (p >= optPart.size())
                        break;
                    size_t keyStart = p;
                    while (p < optPart.size() && optPart[p] != '=')
                        ++p;
                    std::string key = trim(optPart.substr(keyStart, p - keyStart));
                    if (p < optPart.size())
                        ++p;
                    std::string val = parseStringLiteral(optPart, p);
                    if (key == "sep")
                        info.sep = val;
                    else if (key == "followedBy")
                        info.followedBy = val;
                    while (p < optPart.size() && optPart[p] == ' ')
                        ++p;
                }
            }
            return info;
        }

        if (startsWith(t, "if "))
        {
            info.kind = DirectiveKind::If;
            info.ifCond = parseExpression(t.substr(3));
            return info;
        }

        if (startsWith(t, "switch "))
        {
            info.kind = DirectiveKind::Switch;
            info.switchExpr = parseExpression(t.substr(7));
            return info;
        }

        if (startsWith(t, "case "))
        {
            info.kind = DirectiveKind::Case;
            std::string rest = trim(t.substr(5));
            size_t paren = rest.find('(');
            if (paren != std::string::npos)
            {
                info.caseTag = rest.substr(0, paren);
                size_t cparen = rest.find(')', paren);
                info.caseBinding = rest.substr(paren + 1, cparen - paren - 1);
            }
            else
            {
                info.caseTag = rest;
            }
            return info;
        }

        info.kind = DirectiveKind::Expr;
        info.expr = parseExpression(t);
        return info;
    }

    static bool isStructuralDirective(DirectiveKind k)
    {
        return k != DirectiveKind::Expr;
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
                if (seg.isDirective)
                {
                    ls.info = classifyDirective(seg.text);
                    if (ls.info.kind == DirectiveKind::Expr)
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
            else if (s.info.kind == DirectiveKind::For)
            {
                auto forNode = std::make_shared<ForNode>();
                forNode->varName = s.info.forVar;
                forNode->collectionExpr = s.info.forCollection;
                forNode->sep = s.info.sep;
                forNode->followedBy = s.info.followedBy;
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
                        forNode->collectionExpr = seg.info.forCollection;
                        forNode->sep = seg.info.sep;
                        forNode->followedBy = seg.info.followedBy;
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

    TypeRef parseParamType(const std::string &s)
    {
        std::string t = trim(s);
        if (t == "string" || t == "String")
            return StringType{};
        if (t == "int" || t == "Int")
            return IntType{};
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
                // Validate field references in template against known types
                auto validateTemplateFieldAccesses = [&](const TemplateFunction &f, const std::string &body, size_t bodyLine, const TypeRegistry &types, std::vector<Diagnostic> &diags)
                {
                    std::istringstream iss(body);
                    std::string line;
                    size_t li = 0;
                    while (std::getline(iss, line))
                    {
                        size_t p = 0;
                        while (p < line.size())
                        {
                            size_t at = line.find('@', p);
                            if (at == std::string::npos)
                                break;
                            size_t end = line.find('@', at + 1);
                            if (end == std::string::npos)
                                break;
                            std::string inner = line.substr(at + 1, end - at - 1);
                            auto expr = parseExpression(inner);
                            // extract chain of names
                            std::vector<std::string> chain;
                            std::function<void(const Expression &)> buildChain = [&](const Expression &e)
                            {
                                std::visit([&](auto &&arg)
                                           {
                                    using T = std::decay_t<decltype(arg)>;
                                    if constexpr (std::is_same_v<T, Variable>) {
                                        chain.push_back(arg.name);
                                    } else if constexpr (std::is_same_v<T, std::shared_ptr<FieldAccess>>) {
                                        // recursively build base then append field
                                        buildChain(arg->base);
                                        chain.push_back(arg->field);
                                    } }, e);
                            };
                            buildChain(expr);

                            if (!chain.empty())
                            {
                                // find param matching root name
                                for (auto &pdef : f.params)
                                {
                                    if (pdef.name == chain[0])
                                    {
                                        // resolve type and walk fields
                                        TypeRef curType = pdef.type;
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
                                            if (auto spList = std::get_if<std::shared_ptr<ListType>>(&tr))
                                            {
                                                return resolveStruct((*spList)->elementType);
                                            }
                                            return nullptr;
                                        };

                                        for (size_t ci = 1; ci < chain.size(); ++ci)
                                        {
                                            const std::string &fieldName = chain[ci];
                                            const StructDef *sd = resolveStruct(curType);
                                            if (!sd)
                                                break;
                                            bool found = false;
                                            for (auto &fd : sd->fields)
                                            {
                                                if (fd.name == fieldName)
                                                {
                                                    curType = fd.type;
                                                    found = true;
                                                    break;
                                                }
                                            }
                                            if (!found)
                                            {
                                                Diagnostic d;
                                                d.range = {{(int)(bodyLine + li), (int)at}, {(int)(bodyLine + li), (int)(end + 1)}};
                                                d.message = "Reference to undeclared member '" + fieldName + "' of struct '" + (sd ? sd->name : "") + "'";
                                                d.severity = DiagnosticSeverity::Error;
                                                diags.push_back(std::move(d));
                                                break;
                                            }
                                        }
                                        break; // only check the matching param
                                    }
                                }
                            }

                            p = end + 1;
                        }
                        ++li;
                    }
                };

                validateTemplateFieldAccesses(func, bodyText, bodyStartLine, types, diagnostics);

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

    bool FunctionSymbol::bind(const nlohmann::json &input,
                              Program &program,
                              std::string &error) const noexcept
    {
        try
        {
            program.function = function;
            program.types = types;

            if (function.params.empty())
            {
                program.boundArgs = input;
            }
            else if (function.params.size() == 1)
            {
                program.boundArgs = nlohmann::json::object();
                program.boundArgs[function.params[0].name] = input;
            }
            else
            {
                program.boundArgs = nlohmann::json::object();
                for (auto &p : function.params)
                {
                    if (input.contains(p.name))
                    {
                        program.boundArgs[p.name] = input[p.name];
                    }
                }
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
                if (auto spList = std::get_if<std::shared_ptr<ListType>>(&tr))
                {
                    return resolveStruct((*spList)->elementType);
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
                        if (!obj.contains(fd.name))
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
                        if (!obj.contains(fd.name))
                        {
                            error = "Missing field '" + fd.name + "' in input data for function '" + function.name + "'";
                            return false;
                        }
                    }
                }
            }

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
                        std::string iterResult;
                        if (arg->isBlock) {
                            iterResult = renderBlockBody(arg->body, ctx, arg->insertCol);
                        } else {
                            iterResult = renderNodes(arg->body, ctx);
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
                bool cond = !val.is_null();
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
            RenderContext ctx{types};

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