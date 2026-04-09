#include "FoldingRanges.h"
#include <tpp/Tokenizer.h>
#include <tpp/TemplateParser.h>

namespace tpp
{

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
            for (size_t i = 0; i < arg->cases.size(); ++i)
            {
                const auto &c = arg->cases[i];
                int caseStart = c.sourceRange.start.line;
                int caseEnd   = -1;
                if (i + 1 < arg->cases.size())
                    caseEnd = arg->cases[i + 1].sourceRange.start.line - 1;
                else if (arg->endRange.start.line > 0)
                    caseEnd = arg->endRange.start.line - 1;

                if (caseStart >= 0 && caseEnd > caseStart)
                    out.push_back(makeRange(caseStart, caseEnd));

                collectFolds(out, c.body);
            }
        }
    }, node);
}

static void collectFolds(std::vector<nlohmann::json> &out, const std::vector<ASTNode> &nodes)
{
    for (const auto &n : nodes)
        collectFoldNode(out, n);
}

// ── .tpp.types fold ranges ────────────────────────────────────────────────────
static nlohmann::json foldsForTypes(const TppProject &project)
{
    nlohmann::json result = nlohmann::json::array();
    for (const auto &sd : project.output().types.structs)
    {
        if (sd.fields.empty()) continue;
        int startLine = sd.sourceRange.start.line;
        int endLine   = sd.fields.back().sourceRange.start.line;
        if (endLine > startLine)
            result.push_back(makeRange(startLine, endLine));
    }
    for (const auto &ed : project.output().types.enums)
    {
        if (ed.variants.empty()) continue;
        int startLine = ed.sourceRange.start.line;
        int endLine   = ed.variants.back().sourceRange.start.line;
        if (endLine > startLine)
            result.push_back(makeRange(startLine, endLine));
    }
    return result;
}

// ── .tpp fold ranges ──────────────────────────────────────────────────────────
static nlohmann::json foldsForTemplate(const std::string &src, const TppProject &project)
{
    std::vector<nlohmann::json> folds;
    size_t pos = 0;
    int curLine = 0;

    while (pos < src.size())
    {
        // Skip to next "template " header line, collecting block comment folds en route
        while (pos < src.size())
        {
            size_t nl = src.find('\n', pos);
            size_t lineEnd = (nl == std::string::npos) ? src.size() : nl;
            std::string_view line(src.data() + pos, lineEnd - pos);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
                line.remove_suffix(1);
            if (line.size() >= 9 && line.substr(0, 9) == "template ")
                break;

            if (line.size() >= 2 && line[0] == '/' && line[1] == '*')
            {
                int startLine = curLine;
                size_t closePos = src.find("*/", pos + 2);
                size_t blockEnd = (closePos != std::string::npos) ? closePos + 2 : src.size();
                // Count lines inside the block
                for (size_t i = pos; i < blockEnd; ++i)
                    if (src[i] == '\n') ++curLine;
                if (curLine > startLine)
                    folds.push_back(makeRange(startLine, curLine, "comment"));
                pos = blockEnd;
                continue;
            }

            pos = (nl == std::string::npos) ? src.size() : nl + 1;
            if (nl != std::string::npos) ++curLine;
        }
        if (pos >= src.size()) break;

        TemplateFunction func;
        size_t bodyStartLine = 0;
        std::string bodyText;
        if (!parseOneTemplate(src, pos, func, &bodyStartLine, &bodyText))
            break;

        // Template-level fold: header line → END line
        const int headerLine = func.sourceRange.start.line;
        int bodyLineCount = 0;
        for (char c : bodyText) if (c == '\n') ++bodyLineCount;
        const int endLine = (int)bodyStartLine + 1 + bodyLineCount;
        if (endLine > headerLine)
            folds.push_back(makeRange(headerLine, endLine));

        // Body-level folds (for/if/switch blocks)
        collectFolds(folds, func.body);
    }

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
