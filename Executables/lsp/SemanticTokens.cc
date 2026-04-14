#include "SemanticTokens.h"
#include "LspDefinitions.h"
#include <tpp/Tooling.h>
#include <vector>
#include <tuple>

// Shorthand aliases for the token type indices (must match lsp::SemanticTokenType order).
static constexpr int TT_KEYWORD   = lsp::tokenTypeIndex(lsp::SemanticTokenType::keyword);
static constexpr int TT_TYPE      = lsp::tokenTypeIndex(lsp::SemanticTokenType::type);
static constexpr int TT_VARIABLE  = lsp::tokenTypeIndex(lsp::SemanticTokenType::variable);
static constexpr int TT_PROPERTY  = lsp::tokenTypeIndex(lsp::SemanticTokenType::property);
static constexpr int TT_FUNCTION  = lsp::tokenTypeIndex(lsp::SemanticTokenType::function);
[[maybe_unused]] static constexpr int TT_STRING    = lsp::tokenTypeIndex(lsp::SemanticTokenType::string_);
static constexpr int TT_PARAMETER = lsp::tokenTypeIndex(lsp::SemanticTokenType::parameter);
static constexpr int TT_OPERATOR  = lsp::tokenTypeIndex(lsp::SemanticTokenType::operator_);
static constexpr int TT_ENUM_MEMBER = lsp::tokenTypeIndex(lsp::SemanticTokenType::enumMember);
static constexpr int TT_COMMENT   = lsp::tokenTypeIndex(lsp::SemanticTokenType::comment);

namespace tpp
{

// ── Compiler type aliases (these live in tpp::compiler after the IR migration) ─
using Expression        = compiler::Expression;
using Variable          = compiler::Variable;
using FieldAccess       = compiler::FieldAccess;
using ASTNode           = compiler::ASTNode;
using CaseNode          = compiler::CaseNode;
using FunctionCallNode  = compiler::FunctionCallNode;
using AlignmentCellNode = compiler::AlignmentCellNode;
using CommentNode       = compiler::CommentNode;
using InterpolationNode = compiler::InterpolationNode;
using ForNode           = compiler::ForNode;
using IfNode            = compiler::IfNode;
using SwitchNode        = compiler::SwitchNode;
using RenderViaNode     = compiler::RenderViaNode;
using TemplateFunction  = compiler::TemplateFunction;
using TypeRegistry      = compiler::TypeRegistry;

// Each raw token: {line, startChar, length, tokenType, modifiers}
using RawToken = std::tuple<int, int, int, int, int>;

// ── Delta-encode a sorted list of raw tokens ─────────────────────────────────
static nlohmann::json encodeTokens(std::vector<RawToken> &tokens)
{
    // Sort by line then character
    std::sort(tokens.begin(), tokens.end());

    nlohmann::json data = nlohmann::json::array();
    int prevLine = 0, prevStartChar = 0;
    for (const auto &[line, startChar, length, type, mods] : tokens)
    {
        int deltaLine = line - prevLine;
        int deltaChar = (deltaLine == 0) ? (startChar - prevStartChar) : startChar;
        data.push_back(deltaLine);
        data.push_back(deltaChar);
        data.push_back(length);
        data.push_back(type);
        data.push_back(mods);
        prevLine = line;
        prevStartChar = startChar;
    }
    return {{"data", data}};
}

// ── Helper: emit a token if the range is valid ────────────────────────────────
static void emitRange(std::vector<RawToken> &out, const Range &r, int type)
{
    if (r.start.line < 0 || r.start.character < 0) return;
    int len = r.end.character - r.start.character;
    if (len <= 0) return;
    out.push_back({r.start.line, r.start.character, len, type, 0});
}

// ── Convert an Expression back to source text (for positioning) ─────────────
static std::string exprToText(const Expression &e)
{
    if (auto *v = std::get_if<Variable>(&e))
        return v->name;
    if (auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&e))
        return *fa ? exprToText((*fa)->base) + "." + (*fa)->field : std::string{};
    return {};
}

// ── Walk an Expression and emit property tokens for field accesses ────────────
// (Placeholder for future fine-grained highlighting; expression tokens are
//  currently covered by emitRange on the whole directive.)
[[maybe_unused]] static void emitExpression(std::vector<RawToken> &, const Expression &,
                                             int /*baseLine*/, int /*baseChar*/) {}

// ── Walk AST nodes recursively ────────────────────────────────────────────────
static void walkNodes(std::vector<RawToken> &out, const std::vector<ASTNode> &nodes);

// ── Emit sub-tokens for @case Tag[(binding)]@  ───────────────────────────────
static void emitCaseDirective(std::vector<RawToken> &out, const CaseNode &c)
{
    if (c.sourceRange.start.line < 0 || c.sourceRange.start.character < 0) return;
    const int ln  = c.sourceRange.start.line;
    const int col = c.sourceRange.start.character;
    const int end = c.sourceRange.end.character;

    out.push_back({ln, col, 6, TT_KEYWORD, 0}); // "@case "

    // tag name (variant tag → enumMember colour)
    if (!c.tag.empty())
        out.push_back({ln, col + 6, (int)c.tag.size(), TT_ENUM_MEMBER, 0});

    int afterTag = col + 6 + (int)c.tag.size();
    // optional binding: (varname)
    if (!c.bindingName.empty())
    {
        out.push_back({ln, afterTag, 1, TT_OPERATOR, 0}); // (
        out.push_back({ln, afterTag + 1, (int)c.bindingName.size(), TT_VARIABLE, 0});
        out.push_back({ln, afterTag + 1 + (int)c.bindingName.size(), 1, TT_OPERATOR, 0}); // )
        afterTag += 2 + (int)c.bindingName.size();
    }
    // closing "@"
    if (end > afterTag)
        out.push_back({ln, afterTag, end - afterTag, TT_KEYWORD, 0});
}

// ── Emit sub-tokens for a synthetic @render collExpr via func@ case ───────────
// These arise from point-free function calls inside @switch@ blocks.
// The CaseNode has bindingName="__payload" and its body[0] is a FunctionCallNode
// (with no sourceRange set) holding the function name.
static void emitSyntheticRenderCase(std::vector<RawToken> &out, const CaseNode &c)
{
    if (c.sourceRange.start.line < 0 || c.sourceRange.start.character < 0) return;
    const int ln    = c.sourceRange.start.line;
    const int start = c.sourceRange.start.character;
    const int end   = c.sourceRange.end.character;

    out.push_back({ln, start, 8, TT_KEYWORD, 0}); // "@render "
    out.push_back({ln, start + 8, (int)c.tag.size(), TT_VARIABLE, 0}); // collection expr
    const int viaStart = start + 8 + (int)c.tag.size();
    out.push_back({ln, viaStart, 5, TT_KEYWORD, 0}); // " via "

    std::string funcName;
    if (!c.body.empty())
        if (auto *fn = std::get_if<std::shared_ptr<FunctionCallNode>>(&c.body[0]))
            if (*fn) funcName = (*fn)->functionName;

    if (!funcName.empty())
        out.push_back({ln, viaStart + 5, (int)funcName.size(), TT_FUNCTION, 0});

    const int restStart = viaStart + 5 + (int)funcName.size();
    if (end > restStart)
        out.push_back({ln, restStart, end - restStart, TT_KEYWORD, 0}); // closing "@"
}

static void walkNode(std::vector<RawToken> &out, const ASTNode &node)
{
    std::visit([&](auto &&arg)
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, AlignmentCellNode>)
        {
            emitRange(out, arg.sourceRange, TT_OPERATOR);
        }
        else if constexpr (std::is_same_v<T, CommentNode>)
        {
            // Highlight every line of the block (opening, body, closing) as a comment
            for (int ln = arg.startRange.start.line; ln <= arg.endRange.end.line; ++ln)
                out.push_back({ln, 0, 9999, TT_COMMENT, 0});
        }
        else if constexpr (std::is_same_v<T, InterpolationNode>)
        {
            const int ln    = arg.sourceRange.start.line;
            const int start = arg.sourceRange.start.character;
            const int end   = arg.sourceRange.end.character;
            if (arg.policy.empty())
            {
                emitRange(out, arg.sourceRange, TT_VARIABLE);
            }
            else
            {
                // @expr | policy=...@ — expr as variable, tail as keyword
                const std::string exprText = exprToText(arg.expr);
                out.push_back({ln, start, 1 + (int)exprText.size(), TT_VARIABLE, 0});
                const int tailStart = start + 1 + (int)exprText.size();
                if (end > tailStart)
                    out.push_back({ln, tailStart, end - tailStart, TT_KEYWORD, 0});
            }
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>)
        {
            // @funcName(arg1, arg2)@ — @funcName as function, args as variable, )@ as function
            const int ln    = arg->sourceRange.start.line;
            const int start = arg->sourceRange.start.character;
            const int end   = arg->sourceRange.end.character;
            if (start < 0 || end <= start) return;
            const int nameLen = (int)arg->functionName.size();
            out.push_back({ln, start, 1 + nameLen, TT_FUNCTION, 0}); // @funcName
            out.push_back({ln, start + 1 + nameLen, 1, TT_OPERATOR, 0}); // (
            int argCol = start + 1 + nameLen + 1;
            bool firstArg = true;
            for (const auto &argExpr : arg->arguments)
            {
                if (!firstArg) { out.push_back({ln, argCol, 2, TT_OPERATOR, 0}); argCol += 2; }
                firstArg = false;
                const std::string argText = exprToText(argExpr);
                if (!argText.empty())
                { out.push_back({ln, argCol, (int)argText.size(), TT_VARIABLE, 0}); argCol += (int)argText.size(); }
            }
            if (end > argCol)
                out.push_back({ln, argCol, end - argCol, TT_FUNCTION, 0}); // )@
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>)
        {
            const int ln    = arg->sourceRange.start.line;
            const int start = arg->sourceRange.start.character;
            const int end   = arg->sourceRange.end.character;
            out.push_back({ln, start, 5, TT_KEYWORD, 0}); // "@for "
            out.push_back({ln, start + 5, (int)arg->varName.size(), TT_VARIABLE, 0}); // var
            const int inStart = start + 5 + (int)arg->varName.size();
            out.push_back({ln, inStart, 4, TT_KEYWORD, 0}); // " in "
            const std::string collText = exprToText(arg->collectionExpr);
            out.push_back({ln, inStart + 4, (int)collText.size(), TT_VARIABLE, 0}); // coll
            const int restStart = inStart + 4 + (int)collText.size();
            if (end > restStart)
                out.push_back({ln, restStart, end - restStart, TT_KEYWORD, 0}); // "| opts@"
            emitRange(out, arg->endRange, TT_KEYWORD);
            walkNodes(out, arg->body);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
        {
            const int ln    = arg->sourceRange.start.line;
            const int start = arg->sourceRange.start.character;
            const int end   = arg->sourceRange.end.character;
            const int prefixLen = arg->negated ? 8 : 4; // "@if not " or "@if "
            out.push_back({ln, start, prefixLen, TT_KEYWORD, 0});
            out.push_back({ln, start + prefixLen, (int)arg->condText.size(), TT_VARIABLE, 0});
            const int condEnd = start + prefixLen + (int)arg->condText.size();
            if (end > condEnd)
                out.push_back({ln, condEnd, end - condEnd, TT_KEYWORD, 0}); // closing "@"
            emitRange(out, arg->elseRange, TT_KEYWORD);
            emitRange(out, arg->endRange,  TT_KEYWORD);
            walkNodes(out, arg->thenBody);
            walkNodes(out, arg->elseBody);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
        {
            const int ln    = arg->sourceRange.start.line;
            const int start = arg->sourceRange.start.character;
            const int end   = arg->sourceRange.end.character;
            out.push_back({ln, start, 8, TT_KEYWORD, 0}); // "@switch "
            const std::string exprText = exprToText(arg->expr);
            out.push_back({ln, start + 8, (int)exprText.size(), TT_VARIABLE, 0});
            const int restStart = start + 8 + (int)exprText.size();
            if (end > restStart)
                out.push_back({ln, restStart, end - restStart, TT_KEYWORD, 0}); // "| opts@"
            emitRange(out, arg->endRange, TT_KEYWORD);
            for (const auto &c : arg->cases)
            {
                if (c.bindingName == "__payload")
                    emitSyntheticRenderCase(out, c); // @render collExpr via func@
                else
                {
                    emitCaseDirective(out, c);
                    emitRange(out, c.endRange, TT_KEYWORD);
                    walkNodes(out, c.body);
                }
            }
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
        {
            const int ln    = arg->sourceRange.start.line;
            const int start = arg->sourceRange.start.character;
            const int end   = arg->sourceRange.end.character;
            out.push_back({ln, start, 8, TT_KEYWORD, 0}); // "@render "
            const std::string collText = exprToText(arg->collectionExpr);
            out.push_back({ln, start + 8, (int)collText.size(), TT_VARIABLE, 0}); // coll
            const int viaStart = start + 8 + (int)collText.size();
            out.push_back({ln, viaStart, 5, TT_KEYWORD, 0}); // " via "
            out.push_back({ln, viaStart + 5, (int)arg->functionName.size(), TT_FUNCTION, 0});
            const int restStart = viaStart + 5 + (int)arg->functionName.size();
            if (end > restStart)
                out.push_back({ln, restStart, end - restStart, TT_KEYWORD, 0}); // "| opts@"
        }
    }, node);
}

static void walkNodes(std::vector<RawToken> &out, const std::vector<ASTNode> &nodes)
{
    for (const auto &n : nodes)
        walkNode(out, n);
}

// ── Helpers for template header tokenization ─────────────────────────────────
// Given a line like: template main(def: TypeDef, n: int)
// Emit: "template" as keyword, "main" as function, param names as parameter,
// type names as type, and punctuation as operator.
static void emitTemplateHeader(std::vector<RawToken> &out,
                                const TemplateFunction &func,
                                const std::string &headerText,
                                int line)
{
    // "template " keyword at col 0
    out.push_back({line, 0, 8, TT_KEYWORD, 0}); // "template"

    // Function name
    const int nameStart = 9;
    out.push_back({line, nameStart, (int)func.name.size(), TT_FUNCTION, 0});

    // Find '(' after name
    int col = (int)(nameStart + func.name.size());
    if (col < (int)headerText.size() && headerText[col] == '(')
    {
        out.push_back({line, col, 1, TT_OPERATOR, 0}); // '('
        ++col;
        bool first = true;
        for (const auto &param : func.params)
        {
            if (!first)
            {
                // find ','
                size_t comma = headerText.find(',', col);
                if (comma != std::string::npos)
                {
                    out.push_back({line, (int)comma, 1, TT_OPERATOR, 0});
                    col = (int)comma + 1;
                }
            }
            first = false;
            // find param name
            while (col < (int)headerText.size() && headerText[col] == ' ') ++col;
            out.push_back({line, col, (int)param.name.size(), TT_PARAMETER, 0});
            col += (int)param.name.size();
            // ':'
            size_t colon = headerText.find(':', col);
            if (colon != std::string::npos)
            {
                out.push_back({line, (int)colon, 1, TT_OPERATOR, 0});
                col = (int)colon + 1;
                while (col < (int)headerText.size() && headerText[col] == ' ') ++col;
                // type name (may be followed by '<...>')
                int typeStart = col;
                while (col < (int)headerText.size() &&
                       headerText[col] != ',' && headerText[col] != ')' && headerText[col] != '<')
                    ++col;
                if (col > typeStart)
                    out.push_back({line, (int)typeStart, (int)(col - typeStart), TT_TYPE, 0});
                // optional '<' type '>'
                if (col < (int)headerText.size() && headerText[col] == '<')
                {
                    out.push_back({line, col, 1, TT_OPERATOR, 0}); ++col;
                    int innerStart = col;
                    while (col < (int)headerText.size() && headerText[col] != '>') ++col;
                    if (col > innerStart)
                        out.push_back({line, (int)innerStart, (int)(col - innerStart), TT_TYPE, 0});
                    if (col < (int)headerText.size())
                        out.push_back({line, col, 1, TT_OPERATOR, 0}); // '>'
                    if (col < (int)headerText.size()) ++col;
                }
            }
        }
        // Emit any tail between last type and ')' as keyword (| policy="...")
        size_t rparen = headerText.find(')', col);
        if (rparen != std::string::npos)
        {
            if ((int)rparen > col)
                out.push_back({line, col, (int)rparen - col, TT_KEYWORD, 0});
            out.push_back({line, (int)rparen, 1, TT_OPERATOR, 0});
        }
    }
}



// ── Fill gaps between directives with TT_STRING for plain text ──────────────
static void fillGaps(std::vector<RawToken> &out, const std::string &src,
                     int bodyStart, int endLine)
{
    std::map<int, std::vector<std::pair<int,int>>> covered;
    for (const auto &[line, startChar, length, type, mods] : out)
        if (line >= bodyStart && line < endLine)
            covered[line].emplace_back(startChar, startChar + length);
    for (auto &[_, spans] : covered)
        std::sort(spans.begin(), spans.end());

    int curLine = 0;
    size_t pos = 0;
    while (pos < src.size() && curLine < bodyStart)
        if (src[pos++] == '\n') ++curLine;

    std::vector<RawToken> gapTokens;
    while (curLine < endLine && pos < src.size())
    {
        size_t nl = src.find('\n', pos);
        size_t lineEnd = (nl == std::string::npos) ? src.size() : nl;
        int lineLen = (int)(lineEnd - pos);
        const char *lineData = src.data() + pos;

        auto emitGap = [&](int gs, int ge)
        {
            if (ge <= gs || gs >= lineLen) return;
            ge = std::min(ge, lineLen);
            for (int i = gs; i < ge; ++i)
                if (lineData[i] != ' ' && lineData[i] != '\t' && lineData[i] != '\r')
                { gapTokens.push_back({curLine, gs, ge - gs, TT_STRING, 0}); return; }
        };

        auto &spans = covered[curLine];
        int cursor = 0;
        for (auto &[cs, ce] : spans) { emitGap(cursor, cs); cursor = std::max(cursor, ce); }
        emitGap(cursor, lineLen);

        pos = (nl == std::string::npos) ? src.size() : nl + 1;
        ++curLine;
    }
    for (auto &t : gapTokens) out.push_back(std::move(t));
}

// ── .tpp.types semantic tokens ────────────────────────────────────────────────
static void emitTypeTokens(std::vector<RawToken> &out, const std::string &src,
                            const TypeRegistry *reg)
{
    const auto tokens = tokenizeTypeSource(src);

    // State machine to distinguish struct/enum names, field names, tag names, type refs.
    bool inEnum = false;           // inside an enum { } body
    int  depth  = 0;               // brace depth
    bool expectDeclName = false;   // next Ident is a struct/enum type name
    bool expectMemberName = false; // next Ident at depth=1 is a field or tag name

    for (const auto &tok : tokens)
    {
        if (tok.kind == TypeSourceTokenKind::Eof) break;
        int len = (int)tok.text.size();
        if (len <= 0) continue;
        int type = -1;
        int line = tok.range.start.line;
        int col = tok.range.start.character;

        switch (tok.kind)
        {
        case TypeSourceTokenKind::Struct:
            inEnum = false;
            expectDeclName = true;
            expectMemberName = false;
            type = TT_KEYWORD;
            break;
        case TypeSourceTokenKind::Enum:
            inEnum = true;
            expectDeclName = true;
            expectMemberName = false;
            type = TT_KEYWORD;
            break;
        case TypeSourceTokenKind::KwOptional: case TypeSourceTokenKind::KwList:
        case TypeSourceTokenKind::KwString: case TypeSourceTokenKind::KwInt: case TypeSourceTokenKind::KwBool:
            type = TT_KEYWORD;
            break;
        case TypeSourceTokenKind::LBrace:
            depth++;
            if (depth == 1) expectMemberName = true;
            type = TT_OPERATOR;
            break;
        case TypeSourceTokenKind::RBrace:
            depth--;
            if (depth == 0) { inEnum = false; expectMemberName = false; }
            type = TT_OPERATOR;
            break;
        case TypeSourceTokenKind::Semi:
            if (depth == 1) { expectMemberName = true; }
            type = TT_OPERATOR;
            break;
        case TypeSourceTokenKind::LParen: case TypeSourceTokenKind::RParen:
            type = TT_OPERATOR;
            break;
        case TypeSourceTokenKind::Colon:
            type = TT_OPERATOR;
            break;
        case TypeSourceTokenKind::Comma:
            if (inEnum && depth == 1) { expectMemberName = true; }
            type = TT_OPERATOR;
            break;
        case TypeSourceTokenKind::LAngle: case TypeSourceTokenKind::RAngle:
            type = TT_OPERATOR;
            break;
        case TypeSourceTokenKind::Comment:
        case TypeSourceTokenKind::DocComment:
        {
            // Emit one TT_COMMENT token per source line (semantic tokens are single-line)
            int curLine = line;
            int curCol  = col;
            std::string remaining = tok.text;
            while (!remaining.empty())
            {
                size_t nl = remaining.find('\n');
                std::string lineText = (nl == std::string::npos) ? remaining : remaining.substr(0, nl);
                if (!lineText.empty())
                    out.push_back({curLine, curCol, (int)lineText.size(), TT_COMMENT, 0});
                if (nl == std::string::npos) break;
                remaining = remaining.substr(nl + 1);
                ++curLine;
                curCol = 0;
            }
            break;
        }
        case TypeSourceTokenKind::Ident:
            if (expectDeclName)
            {
                type = TT_TYPE;            // struct/enum declaration name
                expectDeclName = false;
            }
            else if (expectMemberName && depth == 1)
            {
                type = inEnum ? TT_ENUM_MEMBER : TT_PROPERTY;
                expectMemberName = false;
            }
            else
            {
                // Type reference (field type, tag payload, generic param)
                type = TT_TYPE;
            }
            break;
        default:
            break;
        }

        if (type >= 0)
            out.push_back({line, col, len, type, 0});
    }
}

static bool lineIsWithinTemplate(const std::vector<std::pair<int, int>> &templateLineRanges,
                                 int line)
{
    for (const auto &[startLine, endLine] : templateLineRanges)
        if (line >= startLine && line <= endLine)
            return true;
    return false;
}

static void emitTopLevelCommentTokens(std::vector<RawToken> &out, const std::string &src,
                                      const std::vector<std::pair<int, int>> &templateLineRanges)
{
    int line = 0;
    size_t pos = 0;
    while (pos < src.size())
    {
        size_t lineEnd = src.find('\n', pos);
        if (lineEnd == std::string::npos)
            lineEnd = src.size();

        std::string_view text(src.data() + pos, lineEnd - pos);
        const size_t firstNonWhitespace = text.find_first_not_of(" \t\r");
        if (firstNonWhitespace != std::string::npos && !lineIsWithinTemplate(templateLineRanges, line))
        {
            std::string_view trimmed = text.substr(firstNonWhitespace);
            if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/')
            {
                out.push_back({line, static_cast<int>(firstNonWhitespace),
                               static_cast<int>(text.size() - firstNonWhitespace), TT_COMMENT, 0});
            }
            else if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '*')
            {
                size_t blockPos = pos + firstNonWhitespace;
                size_t blockEnd = src.find("*/", blockPos + 2);
                if (blockEnd == std::string::npos)
                    blockEnd = src.size();
                else
                    blockEnd += 2;

                size_t segmentPos = blockPos;
                int blockLine = line;
                int blockColumn = static_cast<int>(firstNonWhitespace);
                while (segmentPos < blockEnd)
                {
                    size_t segmentEnd = src.find('\n', segmentPos);
                    if (segmentEnd == std::string::npos || segmentEnd > blockEnd)
                        segmentEnd = blockEnd;

                    if (!lineIsWithinTemplate(templateLineRanges, blockLine) && segmentEnd > segmentPos)
                    {
                        out.push_back({blockLine, blockColumn,
                                       static_cast<int>(segmentEnd - segmentPos), TT_COMMENT, 0});
                    }

                    if (segmentEnd == blockEnd)
                        break;

                    segmentPos = segmentEnd + 1;
                    ++blockLine;
                    blockColumn = 0;
                }

                while (pos < blockEnd)
                {
                    if (src[pos] == '\n')
                        ++line;
                    ++pos;
                }
                continue;
            }
        }

        pos = (lineEnd == src.size()) ? src.size() : lineEnd + 1;
        if (lineEnd != src.size())
            ++line;
    }
}

static nlohmann::json tokensForTypes(const std::string &src, const TypeRegistry *reg)
{
    if (src.empty()) return {{"data", nlohmann::json::array()}};
    std::vector<RawToken> out;
    emitTypeTokens(out, src, reg);
    return encodeTokens(out);
}

// ── .tpp template semantic tokens ────────────────────────────────────────────
// Parse the raw source text of a template file and emit tokens including:
// - "template" keyword, function name, param punctuation
// - @for@/@if@/@switch@ etc. keywords from AST
// - @expr@ as variable
// - END as keyword
static nlohmann::json tokensForTemplate(const std::string &src)
{
    std::vector<RawToken> out;
    std::vector<ParsedTemplateSource> templates;
    parseTemplateSource(src, templates);
    std::vector<std::pair<int, int>> templateLineRanges;
    for (const auto &tpl : templates)
    {
        int bodyLineCount = 0;
        for (char c : tpl.bodyText) if (c == '\n') ++bodyLineCount;
        int endLine = (int)tpl.bodyStartLine + 1 + bodyLineCount;
        templateLineRanges.emplace_back(tpl.function.sourceRange.start.line, endLine);

        emitTemplateHeader(out, tpl.function, tpl.headerText, tpl.function.sourceRange.start.line);
        walkNodes(out, tpl.function.body);

        out.push_back({endLine, 0, 3, TT_KEYWORD, 0});
        fillGaps(out, src, (int)tpl.bodyStartLine, endLine);
    }

    emitTopLevelCommentTokens(out, src, templateLineRanges);

    return encodeTokens(out);
}

// ── Entry points ──────────────────────────────────────────────────────────────
nlohmann::json computeSemanticTokens(const std::string &uri, const TppProject &project)
{
    if (project.isTypeUri(uri))
    {
        // Get the source for this specific file
        std::string src = project.getContent(uri);
        return tokensForTypes(src, nullptr);
    }
    else
    {
        std::string src = project.getContent(uri);
        return tokensForTemplate(src);
    }
}

// ── Standalone (no project) ───────────────────────────────────────────────────
nlohmann::json computeSemanticTokensFromText(const std::string &uri, const std::string &text)
{
    // Heuristic: if no "@" characters, likely a types file
    bool looksLikeTypes = text.find('@') == std::string::npos;
    // Also check extension
    bool hasTypesExt = uri.size() >= 10 && uri.substr(uri.size() - 10) == ".tpp.types";

    if (looksLikeTypes || hasTypesExt)
        return tokensForTypes(text, nullptr);
    else
        return tokensForTemplate(text);
}

} // namespace tpp
