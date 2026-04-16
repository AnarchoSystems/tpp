#include "FoldingRanges.h"
#include <tpp/Tooling.h>

namespace tpp
{

// ── Compiler type aliases ─────────────────────────────────────────────────────
using ASTNode          = compiler::ASTNode;
using CommentNode      = compiler::CommentNode;
using ForNode          = compiler::ForNode;
using IfNode           = compiler::IfNode;
using SwitchNode       = compiler::SwitchNode;
static nlohmann::json makeRange(int startLine, int endLine, const char *kind = "region")
{
    return {{"startLine", startLine}, {"endLine", endLine}, {"kind", kind}};
}

// ── Walk AST nodes recursively and collect fold ranges ────────────────────────
static void collectFolds(std::vector<nlohmann::json> &out, const std::vector<ASTNode> &nodes);

static void collectFoldNode(std::vector<nlohmann::json> &out, const ASTNode &node)
{
    std::visit([&](auto &&arg)
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, CommentNode>)
        {
            if (arg.endRange.start.line > arg.startRange.start.line)
                out.push_back(makeRange(arg.startRange.start.line,
                                        arg.endRange.start.line, "comment"));
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>)
        {
            if (arg->sourceRange.start.line >= 0 &&
                arg->endRange.start.line > arg->sourceRange.start.line)
            {
                out.push_back(makeRange(arg->sourceRange.start.line,
                                        arg->endRange.start.line));
            }
            collectFolds(out, arg->body);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
        {
            const int ifLine   = arg->sourceRange.start.line;
            const int elseLine = arg->elseRange.start.line;
            const int endLine  = arg->endRange.start.line;

            if (arg->elseBody.empty())
            {
                // No else: fold @if@ → @end if@
                if (ifLine >= 0 && endLine > ifLine)
                    out.push_back(makeRange(ifLine, endLine));
            }
            else
            {
                // Fold @if@ → @else@ and @else@ → @end if@ separately
                if (ifLine >= 0 && elseLine > ifLine)
                    out.push_back(makeRange(ifLine, elseLine));
                if (elseLine >= 0 && endLine > elseLine)
                    out.push_back(makeRange(elseLine, endLine));
            }
            collectFolds(out, arg->thenBody);
            collectFolds(out, arg->elseBody);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
        {
            if (arg->sourceRange.start.line >= 0 &&
                arg->endRange.start.line > arg->sourceRange.start.line)
            {
                out.push_back(makeRange(arg->sourceRange.start.line,
                                        arg->endRange.start.line));
            }
            // Fold each case body between consecutive case directives
            std::vector<const compiler::CaseNode *> branches;
            branches.reserve(arg->cases.size() + (arg->defaultCase ? 1 : 0));
            for (const auto &c : arg->cases)
                branches.push_back(&c);
            if (arg->defaultCase)
                branches.push_back(&*arg->defaultCase);

            for (size_t i = 0; i < branches.size(); ++i)
            {
                const auto &c = *branches[i];
                int caseStart = c.sourceRange.start.line;
                int caseEnd   = -1;
                if (i + 1 < branches.size())
                    caseEnd = branches[i + 1]->sourceRange.start.line - 1;
                else if (arg->endRange.start.line > 0)
                    caseEnd = arg->endRange.start.line - 1;

                if (caseStart >= 0 && caseEnd > caseStart)
                    out.push_back(makeRange(caseStart, caseEnd));

                collectFolds(out, c.body);
            }
        }
        // Other node types (Text, Interpolation, etc.) don't produce folds.
    }, node);
}

static void collectFolds(std::vector<nlohmann::json> &out, const std::vector<ASTNode> &nodes)
{
    for (const auto &n : nodes)
        collectFoldNode(out, n);
}

static bool lineIsWithinTemplate(const std::vector<std::pair<int, int>> &templateLineRanges,
                                 int line)
{
    for (const auto &[startLine, endLine] : templateLineRanges)
        if (line >= startLine && line <= endLine)
            return true;
    return false;
}

static void collectTopLevelCommentFolds(std::vector<nlohmann::json> &out, const std::string &src,
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
            if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '*')
            {
                size_t blockStart = pos + firstNonWhitespace;
                size_t blockEnd = src.find("*/", blockStart + 2);
                if (blockEnd == std::string::npos)
                    blockEnd = src.size();
                else
                    blockEnd += 2;

                int endLine = line;
                for (size_t i = pos; i < blockEnd; ++i)
                    if (src[i] == '\n')
                        ++endLine;

                if (endLine > line)
                    out.push_back(makeRange(line, endLine, "comment"));

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

// ── .tpp.types fold ranges ────────────────────────────────────────────────────
static nlohmann::json foldsForTypes(const TppProject &project)
{
    nlohmann::json result = nlohmann::json::array();
    for (const auto &sd : project.output().structs)
    {
        if (sd.fields.empty() || !sd.sourceRange) continue;
        int startLine = sd.sourceRange->start.line;
        int endLine = sd.fields.back().sourceRange
            ? sd.fields.back().sourceRange->start.line
            : startLine;
        if (endLine > startLine)
            result.push_back(makeRange(startLine, endLine));
    }
    for (const auto &ed : project.output().enums)
    {
        if (ed.variants.empty() || !ed.sourceRange) continue;
        int startLine = ed.sourceRange->start.line;
        int endLine = ed.variants.back().sourceRange
            ? ed.variants.back().sourceRange->start.line
            : startLine;
        if (endLine > startLine)
            result.push_back(makeRange(startLine, endLine));
    }
    return result;
}

// ── .tpp fold ranges ──────────────────────────────────────────────────────────
static nlohmann::json foldsForTemplate(const std::string &src, const TppProject &project)
{
    std::vector<nlohmann::json> folds;
    std::vector<ParsedTemplateSource> templates;
    parseTemplateSource(src, templates);
    std::vector<std::pair<int, int>> templateLineRanges;
    for (const auto &tpl : templates)
    {
        const int headerLine = tpl.sourceRange.start.line;
        int bodyLineCount = 0;
        for (char c : tpl.bodyText) if (c == '\n') ++bodyLineCount;
        const int endLine = (int)tpl.bodyStartLine + 1 + bodyLineCount;
        templateLineRanges.emplace_back(headerLine, endLine);
        if (endLine > headerLine)
            folds.push_back(makeRange(headerLine, endLine));

        collectFolds(folds, tpl.body);
    }

    collectTopLevelCommentFolds(folds, src, templateLineRanges);

    nlohmann::json result = nlohmann::json::array();
    for (auto &f : folds) result.push_back(std::move(f));
    return result;
}

// ── Entry point ───────────────────────────────────────────────────────────────
nlohmann::json computeFoldingRanges(const std::string &uri, const TppProject &project)
{
    return project.isTypeUri(uri) ? foldsForTypes(project) : foldsForTemplate(project.getContent(uri), project);
}

} // namespace tpp
