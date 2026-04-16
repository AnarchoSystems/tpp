#include <tpp/Tokenizer.h>
#include <tpp/TypedefParser.h>
#include <tpp/Diagnostic.h>
#include <algorithm>
#include <map>
#include <set>

namespace tpp::compiler
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
            // ── Comment handling ──────────────────────────────────────────────
            if (c == '/' && i + 1 < src.size() && (src[i + 1] == '/' || src[i + 1] == '*'))
            {
                if (src[i + 1] == '/')
                {
                    // Line comment: // or ///
                    bool isDoc = i + 2 < src.size() && src[i + 2] == '/';
                    std::string commentText;
                    while (i < src.size() && src[i] != '\n')
                    { commentText += src[i]; advance(); }
                    TokKind kind = isDoc ? TokKind::DocComment : TokKind::Comment;
                    tokens.push_back({kind, commentText, sline, scol});
                }
                else // src[i+1] == '*'
                {
                    // Block comment: /* or /**
                    bool isDoc = i + 2 < src.size() && src[i + 2] == '*';
                    std::string commentText;
                    while (i < src.size())
                    {
                        if (src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/')
                        {
                            commentText += src[i]; advance();
                            commentText += src[i]; advance();
                            break;
                        }
                        commentText += src[i];
                        advance();
                    }
                    TokKind kind = isDoc ? TokKind::DocComment : TokKind::Comment;
                    tokens.push_back({kind, commentText, sline, scol});
                }
                continue;
            }
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

    const Token &TypedefParser::cur()
    {
        // Transparently skip Comment and DocComment tokens.
        // DocComment text is accumulated into pendingDoc; a plain Comment resets it.
        while (pos < tokens.size() &&
               (tokens[pos].kind == TokKind::Comment ||
                tokens[pos].kind == TokKind::DocComment))
        {
            if (tokens[pos].kind == TokKind::DocComment)
            {
                // Strip the /// or /** */ markers and accumulate
                std::string raw = tokens[pos].text;
                std::string text;
                if (raw.size() >= 3 && raw[0] == '/' && raw[1] == '/' && raw[2] == '/')
                {
                    // /// line comment: strip "///" and optional leading space
                    text = raw.substr(3);
                    if (!text.empty() && text[0] == ' ') text = text.substr(1);
                }
                else if (raw.size() >= 3 && raw[0] == '/' && raw[1] == '*' && raw[2] == '*')
                {
                    // /** block doc comment: strip /** and */
                    text = raw.substr(3);
                    if (text.size() >= 2 && text[text.size()-2] == '*' && text.back() == '/')
                        text.resize(text.size() - 2);
                    // Strip leading/trailing whitespace
                    size_t a = text.find_first_not_of(" \t\r\n");
                    size_t b = text.find_last_not_of(" \t\r\n");
                    text = (a == std::string::npos) ? "" : text.substr(a, b - a + 1);
                }
                if (!pendingDoc.empty()) pendingDoc += "\n";
                pendingDoc += text;
            }
            else
            {
                // Plain // or /* */ resets any accumulated doc
                pendingDoc.clear();
            }
            ++pos;
        }
        return tokens[pos];
    }

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
        case TokKind::Comment:
            return "comment";
        case TokKind::DocComment:
            return "doc comment";
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
    bool TypedefParser::at(TokKind k) { return cur().kind == k; }

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
        std::string typeDoc = takeDoc(); // grab doc accumulated before 'struct'
        eat(TokKind::Struct);
        auto &nameTok = cur();
        eat(TokKind::Ident);
        StructDef sd;
        sd.name = nameTok.text;
        sd.doc  = typeDoc;
        sd.sourceRange = {{nameTok.line, nameTok.col},
                          {nameTok.line, nameTok.col + (int)nameTok.text.size()}};
        eat(TokKind::LBrace);
        while (!at(TokKind::RBrace) && !at(TokKind::Eof) && ok)
        {
            std::string fieldDoc = takeDoc(); // grab doc before field name
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
            FieldDef fd;
            fd.name = fnameTok.text;
            fd.type = type;
            fd.doc  = fieldDoc;
            fd.sourceRange = {{fnameTok.line, fnameTok.col},
                              {fnameTok.line, fnameTok.col + (int)fnameTok.text.size()}};
            sd.fields.push_back(std::move(fd));
        }
        eat(TokKind::RBrace);
        if (at(TokKind::Semi))
            eat(TokKind::Semi);

        size_t idx = semanticModel.structs.size();
        semanticModel.structs.push_back(std::move(sd));
        if (semanticModel.nameIndex.count(semanticModel.structs[idx].name))
        {
            Diagnostic d;
            d.range = {{nameTok.line, nameTok.col},
                       {nameTok.line, nameTok.col + (int)nameTok.text.size()}};
            d.message = "type '" + semanticModel.structs[idx].name + "' is already defined";
            d.severity = DiagnosticSeverity::Error;
            diags.push_back(std::move(d));
            ok = false;
            return;
        }
        semanticModel.nameIndex[semanticModel.structs[idx].name] = {TypeKind::Struct, idx};
    }

    void TypedefParser::parseEnum()
    {
        std::string typeDoc = takeDoc(); // grab doc accumulated before 'enum'
        eat(TokKind::Enum);
        auto &nameTok = cur();
        eat(TokKind::Ident);
        EnumDef ed;
        ed.name = nameTok.text;
        ed.doc  = typeDoc;
        ed.sourceRange = {{nameTok.line, nameTok.col},
                          {nameTok.line, nameTok.col + (int)nameTok.text.size()}};
        eat(TokKind::LBrace);
        while (!at(TokKind::RBrace) && !at(TokKind::Eof) && ok)
        {
            std::string variantDoc = takeDoc(); // grab doc before variant tag
            auto &tagTok = cur();
            eat(TokKind::Ident);
            if (!ok)
                break;
            VariantDef vd;
            vd.tag = tagTok.text;
            vd.doc = variantDoc;
            vd.sourceRange = {{tagTok.line, tagTok.col},
                              {tagTok.line, tagTok.col + (int)tagTok.text.size()}};
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

        size_t idx = semanticModel.enums.size();
        semanticModel.enums.push_back(std::move(ed));
        if (semanticModel.nameIndex.count(semanticModel.enums[idx].name))
        {
            Diagnostic d;
            d.range = {{nameTok.line, nameTok.col},
                       {nameTok.line, nameTok.col + (int)nameTok.text.size()}};
            d.message = "type '" + semanticModel.enums[idx].name + "' is already defined";
            d.severity = DiagnosticSeverity::Error;
            diags.push_back(std::move(d));
            ok = false;
            return;
        }
        semanticModel.nameIndex[semanticModel.enums[idx].name] = {TypeKind::Enum, idx};
    }

    bool TypedefParser::validateTypes()
    {
        // Walk tokens again to find ident tokens used as type refs and validate them
        size_t p = 0;
        // Skip any Comment/DocComment tokens at the current position.
        auto skipComments = [&]()
        {
            while (p < tokens.size() &&
                   (tokens[p].kind == TokKind::Comment || tokens[p].kind == TokKind::DocComment))
                ++p;
        };
        auto skip = [&]()
        { ++p; skipComments(); };
        skipComments(); // skip any leading comments before first token
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
                if (semanticModel.nameIndex.find(t.text) == semanticModel.nameIndex.end())
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

    // ── Helpers shared by computeFiniteTypes and annotateRecursiveFields ──

    static void collectNamedTypesFromRef(const TypeRef &tr, std::set<std::string> &out)
    {
        std::visit([&](const auto &arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, NamedType>)
                out.insert(arg.name);
            else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
                collectNamedTypesFromRef(arg->elementType, out);
            else if constexpr (std::is_same_v<T, std::shared_ptr<OptionalType>>)
                collectNamedTypesFromRef(arg->innerType, out);
        }, tr);
    }

    bool TypedefParser::computeFiniteTypes()
    {
        // Fixed-point: determine which NamedTypes have a finite minimal JSON value.
        //   finite(string|int|bool)     = true
        //   finite(list<T>)             = true  ([] is always valid)
        //   finite(optional<T>)         = true  (null is always valid)
        //   finite(NamedType{N})        = finite(reg[N])
        //   finite(struct)              = ALL fields finite
        //   finite(enum)                = ANY variant finite
        //   finite(variant no payload)  = true
        //   finite(variant payload T)   = finite(T)
        std::set<std::string> finite;

        auto isTypeRefFinite = [&](const TypeRef &tr) -> bool
        {
            return std::visit([&](const auto &arg) -> bool
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, StringType> || std::is_same_v<T, IntType> || std::is_same_v<T, BoolType>)
                    return true;
                else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
                    return true;
                else if constexpr (std::is_same_v<T, std::shared_ptr<OptionalType>>)
                    return true;
                else if constexpr (std::is_same_v<T, NamedType>)
                    return finite.count(arg.name) > 0;
                return false;
            }, tr);
        };

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto &sd : semanticModel.structs)
            {
                if (finite.count(sd.name)) continue;
                bool all = true;
                for (const auto &fd : sd.fields)
                    if (!isTypeRefFinite(fd.type)) { all = false; break; }
                if (all) { finite.insert(sd.name); changed = true; }
            }
            for (const auto &ed : semanticModel.enums)
            {
                if (finite.count(ed.name)) continue;
                bool any = false;
                for (const auto &vd : ed.variants)
                    if (!vd.payload.has_value() || isTypeRefFinite(vd.payload.value()))
                    { any = true; break; }
                if (any) { finite.insert(ed.name); changed = true; }
            }
        }

        // Helper: find the token position of a type name (struct/enum keyword followed by name ident)
        auto findTypeNameToken = [&](const std::string &typeName) -> const Token *
        {
            for (size_t p = 0; p + 1 < tokens.size(); ++p)
            {
                if ((tokens[p].kind == TokKind::Struct || tokens[p].kind == TokKind::Enum) &&
                    tokens[p + 1].kind == TokKind::Ident &&
                    tokens[p + 1].text == typeName)
                    return &tokens[p + 1];
            }
            return nullptr;
        };

        bool allOk = true;
        for (const auto &sd : semanticModel.structs)
        {
            if (finite.count(sd.name)) continue;
            const Token *t = findTypeNameToken(sd.name);
            if (!t) continue;
            Diagnostic d;
            d.range = {{t->line, t->col}, {t->line, t->col + (int)sd.name.size()}};
            d.message = "type '" + sd.name + "' has no finite minimal JSON value";
            d.severity = DiagnosticSeverity::Error;
            diags.push_back(std::move(d));
            allOk = false;
        }
        for (const auto &ed : semanticModel.enums)
        {
            if (finite.count(ed.name)) continue;
            const Token *t = findTypeNameToken(ed.name);
            if (!t) continue;
            Diagnostic d;
            d.range = {{t->line, t->col}, {t->line, t->col + (int)ed.name.size()}};
            d.message = "type '" + ed.name + "' has no finite minimal JSON value";
            d.severity = DiagnosticSeverity::Error;
            diags.push_back(std::move(d));
            allOk = false;
        }
        return allOk;
    }

    void annotateRecursiveFields(SemanticModel &semanticModel)
    {
        // Build direct NamedType reference sets for each type.
        std::map<std::string, std::set<std::string>> directRefs;
        for (const auto &sd : semanticModel.structs)
        {
            auto &refs = directRefs[sd.name];
            for (const auto &fd : sd.fields)
                collectNamedTypesFromRef(fd.type, refs);
        }
        for (const auto &ed : semanticModel.enums)
        {
            auto &refs = directRefs[ed.name];
            for (const auto &vd : ed.variants)
                if (vd.payload.has_value())
                    collectNamedTypesFromRef(vd.payload.value(), refs);
        }

        // Compute transitive closure.
        std::map<std::string, std::set<std::string>> closure = directRefs;
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (auto &[name, reachable] : closure)
            {
                std::vector<std::string> deps(reachable.begin(), reachable.end());
                for (const auto &dep : deps)
                {
                    auto it = closure.find(dep);
                    if (it == closure.end()) continue;
                    for (const auto &transitive : it->second)
                    {
                        if (reachable.insert(transitive).second)
                            changed = true;
                    }
                }
            }
        }

        // A type is cyclic if its transitive closure contains itself.
        std::set<std::string> cyclic;
        for (const auto &[name, reachable] : closure)
            if (reachable.count(name))
                cyclic.insert(name);

        // Annotate: a field/variant is recursive if any NamedType in its TypeRef is cyclic.
        auto containsCyclic = [&](const TypeRef &tr) -> bool
        {
            std::set<std::string> mentioned;
            collectNamedTypesFromRef(tr, mentioned);
            for (const auto &n : mentioned)
                if (cyclic.count(n)) return true;
            return false;
        };

        for (auto &sd : semanticModel.structs)
            for (auto &fd : sd.fields)
                fd.recursive = containsCyclic(fd.type);
        for (auto &ed : semanticModel.enums)
            for (auto &vd : ed.variants)
                if (vd.payload.has_value())
                    vd.recursive = containsCyclic(vd.payload.value());
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

} // namespace tpp::compiler
