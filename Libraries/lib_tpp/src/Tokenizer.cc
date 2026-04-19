#include "tpp/Tokenizer.h"
#include <algorithm>
#include <cctype>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // Type definition tokenizer
    // ════════════════════════════════════════════════════════════════════════════

    TokenizeResult tokenize_types(const std::string &source, const std::string &,
                                  std::vector<TypeToken> &tokens)
    {
        tokens.clear();
        int line = 1, col = 1;
        size_t i = 0;
        auto peek = [&]() -> char { return i < source.size() ? source[i] : '\0'; };
        auto advance = [&]() -> char {
            char c = source[i++];
            if (c == '\n') { ++line; col = 1; } else { ++col; }
            return c;
        };
        auto skipWS = [&]() {
            while (i < source.size() && (source[i] == ' ' || source[i] == '\t' || source[i] == '\n' || source[i] == '\r'))
                advance();
        };

        while (true)
        {
            skipWS();
            if (i >= source.size())
            {
                tokens.push_back({TypeTokenKind::EndOfFile, "", line, col});
                break;
            }
            int tl = line, tc = col;
            int toffset = (int)i;
            char c = peek();

            // Line comment
            if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
            {
                // Check for doc comment (///)
                bool isDoc = (i + 2 < source.size() && source[i + 2] == '/');
                std::string text;
                while (i < source.size() && source[i] != '\n')
                    text += advance();
                if (isDoc)
                {
                    // Strip "/// " prefix
                    text = text.substr(3);
                    if (!text.empty() && text[0] == ' ') text = text.substr(1);
                    tokens.push_back({TypeTokenKind::DocComment, text, tl, tc, toffset});
                }
                continue;
            }

            // Block comment
            if (c == '/' && i + 1 < source.size() && source[i + 1] == '*')
            {
                bool isDoc = (i + 2 < source.size() && source[i + 2] == '*');
                advance(); advance(); // skip /*
                if (isDoc && i < source.size() && source[i] != '/') advance(); // skip second *
                std::string text;
                while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/'))
                    text += advance();
                if (i + 1 < source.size()) { advance(); advance(); } // skip */
                if (isDoc)
                {
                    // Trim
                    while (!text.empty() && (text.front() == ' ' || text.front() == '\n'))
                        text.erase(text.begin());
                    while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '*'))
                        text.pop_back();
                    tokens.push_back({TypeTokenKind::DocComment, text, tl, tc, toffset});
                }
                continue;
            }

            if (c == '{') { advance(); tokens.push_back({TypeTokenKind::LeftBrace, "{", tl, tc, toffset}); continue; }
            if (c == '}') { advance(); tokens.push_back({TypeTokenKind::RightBrace, "}", tl, tc, toffset}); continue; }
            if (c == '(') { advance(); tokens.push_back({TypeTokenKind::LeftParen, "(", tl, tc, toffset}); continue; }
            if (c == ')') { advance(); tokens.push_back({TypeTokenKind::RightParen, ")", tl, tc, toffset}); continue; }
            if (c == '<') { advance(); tokens.push_back({TypeTokenKind::LeftAngle, "<", tl, tc, toffset}); continue; }
            if (c == '>') { advance(); tokens.push_back({TypeTokenKind::RightAngle, ">", tl, tc, toffset}); continue; }
            if (c == ':') { advance(); tokens.push_back({TypeTokenKind::Colon, ":", tl, tc, toffset}); continue; }
            if (c == ';') { advance(); tokens.push_back({TypeTokenKind::Semicolon, ";", tl, tc, toffset}); continue; }
            if (c == ',') { advance(); tokens.push_back({TypeTokenKind::Comma, ",", tl, tc, toffset}); continue; }

            // Identifier or keyword
            if (std::isalpha(c) || c == '_')
            {
                std::string id;
                while (i < source.size() && (std::isalnum(source[i]) || source[i] == '_'))
                    id += advance();
                TypeTokenKind kind = TypeTokenKind::Identifier;
                if (id == "struct") kind = TypeTokenKind::Struct;
                else if (id == "enum") kind = TypeTokenKind::Enum;
                tokens.push_back({kind, id, tl, tc, toffset});
                continue;
            }

            // Unknown character — skip
            advance();
        }

        return {true, "", 0, 0};
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Template tokenizer — mode-based (text / directive)
    // ════════════════════════════════════════════════════════════════════════════

    TokenizeResult tokenize_template(const std::string &source, const std::string &,
                                     std::vector<TplToken> &tokens)
    {
        tokens.clear();
        int line = 1, col = 1;
        size_t i = 0;
        bool inDirective = false;
        bool templateHeader = false;  // true while tokenizing a bare "template" header line

        auto advance = [&]() -> char {
            char c = source[i++];
            if (c == '\n') { ++line; col = 1; } else { ++col; }
            return c;
        };

        // Start: scan for "template" keyword at beginning (skipping comments)
        while (i < source.size())
        {
            if (inDirective)
            {
                // Inside @ ... @
                // Skip whitespace inside directive
                while (i < source.size() && (source[i] == ' ' || source[i] == '\t'))
                    advance();

                if (i >= source.size())
                {
                    if (templateHeader)
                    {
                        // End of file closes template header implicitly
                        tokens.push_back({TplTokenKind::DirectiveClose, "@", line, col});
                        inDirective = false;
                        templateHeader = false;
                        continue;
                    }
                    return {false, "Unterminated directive", line, col};
                }

                int tl = line, tc = col;
                char c = source[i];

                // Template header lines end at newline (no explicit @)
                if (templateHeader && (c == '\n' || c == '\r'))
                {
                    tokens.push_back({TplTokenKind::DirectiveClose, "@", tl, tc});
                    inDirective = false;
                    templateHeader = false;
                    // consume the newline
                    if (c == '\r') advance();
                    if (i < source.size() && source[i] == '\n') advance();
                    continue;
                }

                if (c == '@')
                {
                    advance();
                    tokens.push_back({TplTokenKind::DirectiveClose, "@", tl, tc});
                    inDirective = false;
                    continue;
                }

                if (c == '.') { advance(); tokens.push_back({TplTokenKind::Dot, ".", tl, tc}); continue; }
                if (c == '(') { advance(); tokens.push_back({TplTokenKind::LeftParen, "(", tl, tc}); continue; }
                if (c == ')') { advance(); tokens.push_back({TplTokenKind::RightParen, ")", tl, tc}); continue; }
                if (c == '|') { advance(); tokens.push_back({TplTokenKind::Pipe, "|", tl, tc}); continue; }
                if (c == '=') { advance(); tokens.push_back({TplTokenKind::Equals, "=", tl, tc}); continue; }
                if (c == '&') { advance(); tokens.push_back({TplTokenKind::Ampersand, "&", tl, tc}); continue; }
                if (c == '<') { advance(); tokens.push_back({TplTokenKind::LeftAngle, "<", tl, tc}); continue; }
                if (c == '>') { advance(); tokens.push_back({TplTokenKind::RightAngle, ">", tl, tc}); continue; }
                if (c == ',') { advance(); tokens.push_back({TplTokenKind::Comma, ",", tl, tc}); continue; }
                if (c == ':') { advance(); tokens.push_back({TplTokenKind::Colon, ":", tl, tc}); continue; }

                // String literal
                if (c == '"')
                {
                    advance(); // skip opening "
                    std::string text;
                    while (i < source.size() && source[i] != '"')
                    {
                        if (source[i] == '\\' && i + 1 < source.size())
                        {
                            advance(); // skip backslash
                            char ec = advance();
                            switch (ec)
                            {
                            case 'n': text += '\n'; break;
                            case 't': text += '\t'; break;
                            case '"': text += '"'; break;
                            case '\\': text += '\\'; break;
                            default: text += '\\'; text += ec; break;
                            }
                        }
                        else
                        {
                            text += advance();
                        }
                    }
                    if (i < source.size()) advance(); // skip closing "
                    tokens.push_back({TplTokenKind::StringLiteral, text, tl, tc});
                    continue;
                }

                // Identifier or keyword
                if (std::isalpha(c) || c == '_')
                {
                    std::string id;
                    while (i < source.size() && (std::isalnum(source[i]) || source[i] == '_'))
                        id += advance();

                    // Check for multi-word keywords
                    if (id == "end")
                    {
                        // Skip whitespace
                        size_t savedI = i; int savedLine = line, savedCol = col;
                        while (i < source.size() && source[i] == ' ') advance();
                        std::string next;
                        while (i < source.size() && std::isalpha(source[i]))
                            next += advance();
                        if (next == "for") { tokens.push_back({TplTokenKind::EndFor, "end for", tl, tc}); continue; }
                        else if (next == "if") { tokens.push_back({TplTokenKind::EndIf, "end if", tl, tc}); continue; }
                        else if (next == "switch") { tokens.push_back({TplTokenKind::EndSwitch, "end switch", tl, tc}); continue; }
                        else if (next == "case") { tokens.push_back({TplTokenKind::EndCase, "end case", tl, tc}); continue; }
                        else if (next == "comment") { tokens.push_back({TplTokenKind::EndComment, "end comment", tl, tc}); continue; }
                        else
                        {
                            // Restore and treat "end" as identifier
                            i = savedI; line = savedLine; col = savedCol;
                            id = "end";
                        }
                    }

                    TplTokenKind kind = TplTokenKind::Identifier;
                    if (id == "template") kind = TplTokenKind::Template_;
                    else if (id == "END") kind = TplTokenKind::End;
                    else if (id == "for") kind = TplTokenKind::For;
                    else if (id == "in") kind = TplTokenKind::In;
                    else if (id == "if") kind = TplTokenKind::If;
                    else if (id == "else") kind = TplTokenKind::Else;
                    else if (id == "switch") kind = TplTokenKind::Switch;
                    else if (id == "case") kind = TplTokenKind::Case;
                    else if (id == "comment") kind = TplTokenKind::Comment;
                    else if (id == "render") kind = TplTokenKind::Render;
                    else if (id == "via") kind = TplTokenKind::Via;
                    else if (id == "not") kind = TplTokenKind::Not;

                    tokens.push_back({kind, id, tl, tc});
                    continue;
                }

                // Number
                if (std::isdigit(c))
                {
                    std::string num;
                    while (i < source.size() && std::isdigit(source[i]))
                        num += advance();
                    tokens.push_back({TplTokenKind::Identifier, num, tl, tc});
                    continue;
                }

                // Skip unknown character inside directive
                advance();
            }
            else
            {
                // Text mode
                // Look for template/END at start of line, or @ for directive
                int tl = line, tc = col;

                // Check for "template" or "END" at start of a line (first non-space)
                // These are outside-directive structural markers
                if (tc == 1 || (tokens.empty() && i == 0))
                {
                    // Check if line starts with "template "
                    if (source.substr(i, 9) == "template " || source.substr(i, 9) == "template\t")
                    {
                        // Emit as directive: Template_ keyword, then enter directive mode
                        // The template header runs until the end of the line (no closing @)
                        tokens.push_back({TplTokenKind::DirectiveOpen, "@", tl, tc});
                        std::string kw;
                        while (i < source.size() && std::isalpha(source[i]))
                            kw += advance();
                        tokens.push_back({TplTokenKind::Template_, kw, tl, tc});
                        inDirective = true;
                        templateHeader = true;
                        continue;
                    }
                    if (source.substr(i, 3) == "END" &&
                        (i + 3 >= source.size() || source[i + 3] == '\n' || source[i + 3] == '\r'))
                    {
                        tokens.push_back({TplTokenKind::DirectiveOpen, "@", tl, tc});
                        advance(); advance(); advance(); // skip END
                        tokens.push_back({TplTokenKind::End, "END", tl, tc});
                        tokens.push_back({TplTokenKind::DirectiveClose, "@", tl, tc});
                        // Skip the newline after END
                        if (i < source.size() && source[i] == '\r') advance();
                        if (i < source.size() && source[i] == '\n') advance();
                        continue;
                    }
                }

                // Scan text until @, template/END at line start, or end of input
                std::string text;
                while (i < source.size())
                {
                    if (source[i] == '@')
                    {
                        // Emit any accumulated text
                        if (!text.empty())
                        {
                            tokens.push_back({TplTokenKind::Text, text, tl, tc});
                            text.clear();
                        }
                        int al = line, ac = col;
                        advance(); // skip @

                        tokens.push_back({TplTokenKind::DirectiveOpen, "@", al, ac});
                        inDirective = true;
                        break;
                    }

                    if (source[i] == '\\' && i + 1 < source.size() && source[i + 1] == '@')
                    {
                        advance(); // skip backslash
                        text += advance(); // take @
                        continue;
                    }

                    // Check for newline: after it, we may need to recognize template/END
                    if (source[i] == '\n' || source[i] == '\r')
                    {
                        text += advance();
                        if (source[i - 1] == '\r' && i < source.size() && source[i] == '\n')
                            text += advance();
                        // Check if next line starts with "template " or "END"
                        if (i < source.size())
                        {
                            bool isTemplate = (i + 9 <= source.size() &&
                                              (source.substr(i, 9) == "template " ||
                                               source.substr(i, 9) == "template\t"));
                            bool isEnd = (source.substr(i, 3) == "END" &&
                                         (i + 3 >= source.size() || source[i + 3] == '\n' || source[i + 3] == '\r'));
                            // \END escapes the END keyword — consume the backslash
                            if (!isEnd && i < source.size() && source[i] == '\\' &&
                                i + 4 <= source.size() && source.substr(i + 1, 3) == "END" &&
                                (i + 4 >= source.size() || source[i + 4] == '\n' || source[i + 4] == '\r'))
                            {
                                advance(); // skip backslash
                                continue;
                            }
                            if (isTemplate || isEnd)
                            {
                                // Emit accumulated text and break back to main loop
                                if (!text.empty())
                                {
                                    tokens.push_back({TplTokenKind::Text, text, tl, tc});
                                    text.clear();
                                }
                                break;
                            }
                        }
                        continue;
                    }

                    text += advance();
                }

                if (!text.empty() && !inDirective)
                    tokens.push_back({TplTokenKind::Text, text, tl, tc});
            }
        }

        if (tokens.empty() || tokens.back().kind != TplTokenKind::EndOfFile)
            tokens.push_back({TplTokenKind::EndOfFile, "", line, col});

        return {true, "", 0, 0};
    }

} // namespace tpp
