#include <tpp/Tooling.h>
#include "tpp/Tokenizer.h"
#include "tpp/TypedefParser.h"
#include "tpp/TemplateParser.h"
#include <string_view>

namespace tpp
{

namespace {

static bool isTemplateHeaderLine(std::string_view line)
{
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
        line.remove_suffix(1);
    return line.size() >= 9 && line.substr(0, 9) == "template ";
}

static size_t skipToNextTemplateHeader(const std::string &src, size_t pos)
{
    while (pos < src.size())
    {
        size_t lineEnd = src.find('\n', pos);
        if (lineEnd == std::string::npos)
            lineEnd = src.size();

        std::string_view line(src.data() + pos, lineEnd - pos);
        if (isTemplateHeaderLine(line))
            return pos;

        const size_t firstNonWhitespace = line.find_first_not_of(" \t\r");
        if (firstNonWhitespace != std::string::npos)
        {
            std::string_view trimmed = line.substr(firstNonWhitespace);
            if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '*')
            {
                size_t blockStart = pos + firstNonWhitespace;
                size_t blockEnd = src.find("*/", blockStart + 2);
                if (blockEnd == std::string::npos)
                    return src.size();
                pos = blockEnd + 2;
                continue;
            }
        }

        pos = (lineEnd == src.size()) ? src.size() : lineEnd + 1;
    }

    return pos;
}

} // namespace

static TypeSourceTokenKind convertTokenKind(TokKind kind)
{
    switch (kind)
    {
    case TokKind::Struct: return TypeSourceTokenKind::Struct;
    case TokKind::Enum: return TypeSourceTokenKind::Enum;
    case TokKind::LBrace: return TypeSourceTokenKind::LBrace;
    case TokKind::RBrace: return TypeSourceTokenKind::RBrace;
    case TokKind::Semi: return TypeSourceTokenKind::Semi;
    case TokKind::Colon: return TypeSourceTokenKind::Colon;
    case TokKind::LParen: return TypeSourceTokenKind::LParen;
    case TokKind::RParen: return TypeSourceTokenKind::RParen;
    case TokKind::Comma: return TypeSourceTokenKind::Comma;
    case TokKind::LAngle: return TypeSourceTokenKind::LAngle;
    case TokKind::RAngle: return TypeSourceTokenKind::RAngle;
    case TokKind::KwOptional: return TypeSourceTokenKind::KwOptional;
    case TokKind::KwList: return TypeSourceTokenKind::KwList;
    case TokKind::KwString: return TypeSourceTokenKind::KwString;
    case TokKind::KwInt: return TypeSourceTokenKind::KwInt;
    case TokKind::KwBool: return TypeSourceTokenKind::KwBool;
    case TokKind::Ident: return TypeSourceTokenKind::Ident;
    case TokKind::Comment: return TypeSourceTokenKind::Comment;
    case TokKind::DocComment: return TypeSourceTokenKind::DocComment;
    case TokKind::Eof: return TypeSourceTokenKind::Eof;
    }
    return TypeSourceTokenKind::Eof;
}

std::vector<TypeSourceToken> tokenizeTypeSource(const std::string &src)
{
    std::vector<TypeSourceToken> result;
    const auto tokens = compiler::tokenize_typedefs(src);
    result.reserve(tokens.size());
    for (const auto &token : tokens)
    {
        result.push_back({
            convertTokenKind(token.kind),
            token.text,
            {{token.line, token.col}, {token.line, token.col + static_cast<int>(token.text.size())}}
        });
    }
    return result;
}

bool parseTemplateSource(const std::string &src,
                         std::vector<ParsedTemplateSource> &templates)
{
    std::vector<Diagnostic> diagnostics;
    return parseTemplateSource(src, templates, diagnostics);
}

bool parseTemplateSource(const std::string &src,
                         std::vector<ParsedTemplateSource> &templates,
                         std::vector<Diagnostic> &diagnostics)
{
    templates.clear();
    size_t pos = 0;
    while (pos < src.size())
    {
        pos = skipToNextTemplateHeader(src, pos);
        if (pos >= src.size())
            break;

        compiler::TemplateFunction function;
        size_t bodyStartLine = 0;
        std::string bodyText;
        std::string headerText;
        Range headerRange;
        size_t prevPos = pos;

        if (!compiler::parseOneTemplate(src, pos, function, &bodyStartLine, &bodyText,
                                        nullptr, &diagnostics, &headerRange, &headerText))
        {
            return pos != prevPos;
        }

        templates.push_back({std::move(function), bodyStartLine, std::move(bodyText),
                             std::move(headerText), headerRange});
    }
    return true;
}

}