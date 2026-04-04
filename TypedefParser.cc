#include <tpp/Compiler.h>
#include <tpp/Tokenizer.h>
#include <tpp/TypedefParser.h>
#include <tpp/Diagnostic.h>

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

} // namespace tpp
