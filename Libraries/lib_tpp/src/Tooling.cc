#include <tpp/Tooling.h>
#include "tpp/Tokenizer.h"
#include "tpp/TypedefParser.h"
#include "tpp/TemplateParser.h"
#include <string_view>

namespace tpp
{

namespace {

static std::string trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return std::string(text);
}

static bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

static bool advanceToNextTemplateStart(const std::string &src,
                                       size_t &pos,
                                       std::vector<Diagnostic> *diagnostics)
{
    while (pos < src.size())
    {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
        if (pos >= src.size())
            return false;

        const size_t lineStart = pos;
        size_t lineEnd = src.find('\n', pos);
        if (lineEnd == std::string::npos)
            lineEnd = src.size();

        std::string lineText = trim(std::string_view(src.data() + lineStart, lineEnd - lineStart));

        if (lineText.size() >= 3 && lineText[0] == '/' && lineText[1] == '/' && lineText[2] == '/')
        {
            pos = (lineEnd < src.size()) ? lineEnd + 1 : lineEnd;
            continue;
        }

        if (lineText.size() >= 2 && lineText[0] == '/' && lineText[1] == '/')
        {
            pos = (lineEnd < src.size()) ? lineEnd + 1 : lineEnd;
            continue;
        }

        if (lineText.size() >= 3 && lineText[0] == '/' && lineText[1] == '*' && lineText[2] == '*')
        {
            size_t closePos = src.find("*/", pos + 3);
            pos = (closePos != std::string::npos) ? closePos + 2 : src.size();
            continue;
        }

        if (lineText.size() >= 2 && lineText[0] == '/' && lineText[1] == '*')
        {
            size_t closePos = src.find("*/", pos + 2);
            pos = (closePos != std::string::npos) ? closePos + 2 : src.size();
            continue;
        }

        if (startsWith(lineText, "template "))
            return true;

        if (diagnostics)
        {
            const int lineNum = static_cast<int>(std::count(src.begin(), src.begin() + static_cast<std::ptrdiff_t>(lineStart), '\n'));
            Diagnostic d;
            d.range = {{lineNum, 0}, {lineNum, static_cast<int>(lineText.size())}};
            d.message = "unexpected content outside template: '" + lineText + "'";
            d.severity = DiagnosticSeverity::Error;
            diagnostics->push_back(std::move(d));
        }

        pos = (lineEnd < src.size()) ? lineEnd + 1 : lineEnd;
    }

    return false;
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

std::vector<TemplateToken> tokenizeTemplateSource(const std::string &src)
{
    std::vector<TemplateToken> result;

    size_t lineStart = 0;
    int lineNumber = 0;
    while (lineStart <= src.size())
    {
        size_t lineEnd = src.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = src.size();

        const std::string line = src.substr(lineStart, lineEnd - lineStart);
        const auto segments = compiler::tokenize_template(line);
        for (const auto &segment : segments)
        {
            result.push_back({
                segment.isDirective,
                segment.text,
                {{lineNumber, segment.startCol}, {lineNumber, segment.endCol}}
            });
        }

        if (lineEnd == src.size())
            break;

        lineStart = lineEnd + 1;
        ++lineNumber;
    }

    return result;
}

std::vector<TypeSourceSemanticSpan> classifyTypeSource(const std::string &src)
{
    return classifyTypeSourceTokens(tokenizeTypeSource(src));
}

std::vector<TypeSourceSemanticSpan> classifyTypeSourceTokens(const std::vector<TypeSourceToken> &tokens)
{
    std::vector<TypeSourceSemanticSpan> spans;

    bool inEnum = false;
    int depth = 0;
    bool expectDeclName = false;
    bool expectMemberName = false;

    for (const auto &token : tokens)
    {
        if (token.kind == TypeSourceTokenKind::Eof)
            break;

        TypeSourceSemanticKind kind;
        bool emit = true;

        switch (token.kind)
        {
        case TypeSourceTokenKind::Struct:
            inEnum = false;
            expectDeclName = true;
            expectMemberName = false;
            kind = TypeSourceSemanticKind::Keyword;
            break;
        case TypeSourceTokenKind::Enum:
            inEnum = true;
            expectDeclName = true;
            expectMemberName = false;
            kind = TypeSourceSemanticKind::Keyword;
            break;
        case TypeSourceTokenKind::KwOptional:
        case TypeSourceTokenKind::KwList:
        case TypeSourceTokenKind::KwString:
        case TypeSourceTokenKind::KwInt:
        case TypeSourceTokenKind::KwBool:
            kind = TypeSourceSemanticKind::Keyword;
            break;
        case TypeSourceTokenKind::LBrace:
            ++depth;
            if (depth == 1) expectMemberName = true;
            kind = TypeSourceSemanticKind::Operator;
            break;
        case TypeSourceTokenKind::RBrace:
            --depth;
            if (depth == 0)
            {
                inEnum = false;
                expectMemberName = false;
            }
            kind = TypeSourceSemanticKind::Operator;
            break;
        case TypeSourceTokenKind::Semi:
            if (depth == 1) expectMemberName = true;
            kind = TypeSourceSemanticKind::Operator;
            break;
        case TypeSourceTokenKind::LParen:
        case TypeSourceTokenKind::RParen:
        case TypeSourceTokenKind::Colon:
        case TypeSourceTokenKind::Comma:
        case TypeSourceTokenKind::LAngle:
        case TypeSourceTokenKind::RAngle:
            if (token.kind == TypeSourceTokenKind::Comma && inEnum && depth == 1)
                expectMemberName = true;
            kind = TypeSourceSemanticKind::Operator;
            break;
        case TypeSourceTokenKind::Comment:
        case TypeSourceTokenKind::DocComment:
            kind = TypeSourceSemanticKind::Comment;
            break;
        case TypeSourceTokenKind::Ident:
            if (expectDeclName)
            {
                kind = TypeSourceSemanticKind::Type;
                expectDeclName = false;
            }
            else if (expectMemberName && depth == 1)
            {
                kind = inEnum ? TypeSourceSemanticKind::EnumMember : TypeSourceSemanticKind::Property;
                expectMemberName = false;
            }
            else
            {
                kind = TypeSourceSemanticKind::Type;
            }
            break;
        default:
            emit = false;
            break;
        }

        if (emit)
            spans.push_back({token.range, kind});
    }

    return spans;
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
    while (advanceToNextTemplateStart(src, pos, &diagnostics))
    {
        compiler::TemplateFunction function;
        size_t bodyStartLine = 0;
        std::string bodyText;
        std::string headerText;
        Range headerRange;

        if (!compiler::parseOneTemplate(src, pos, function, &bodyStartLine, &bodyText,
                                        nullptr, &diagnostics, &headerRange, &headerText))
            return false;

        ParsedTemplateSource parsed;
        parsed.name = std::move(function.name);
        parsed.params = std::move(function.params);
        parsed.body = std::move(function.body);
        parsed.policy = std::move(function.policy);
        parsed.doc = std::move(function.doc);
        parsed.sourceRange = function.sourceRange;
        parsed.bodyStartLine = bodyStartLine;
        parsed.bodyText = std::move(bodyText);
        parsed.headerText = std::move(headerText);
        parsed.headerRange = headerRange;
        templates.push_back(std::move(parsed));
    }
    return true;
}

std::vector<TemplateDirectiveRange> extractTemplateDirectiveRanges(const std::string &bodyText,
                                                                   size_t bodyStartLine)
{
    std::vector<TemplateDirectiveRange> ranges;
    const auto templateLines = compiler::parseTemplateLines(bodyText);
    for (size_t lineIndex = 0; lineIndex < templateLines.size(); ++lineIndex)
    {
        const auto &line = templateLines[lineIndex];
        for (const auto &segment : line.segments)
        {
            if (!segment.isDirective)
                continue;

            ranges.push_back({
                {{static_cast<int>(bodyStartLine + lineIndex), segment.startCol - 1},
                 {static_cast<int>(bodyStartLine + lineIndex), segment.endCol + 1}},
                compiler::isStructuralDirective(segment.info)
            });
        }
    }

    return ranges;
}

}