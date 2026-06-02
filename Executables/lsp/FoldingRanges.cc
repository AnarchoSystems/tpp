#include "FoldingRanges.h"
#include <tpp/Tooling.h>
#include "tpp/ToolingFolding.h"

namespace tpp
{

static nlohmann::json makeRange(int startLine, int endLine, std::string kind = "region")
{
    return {{"startLine", startLine}, {"endLine", endLine}, {"kind", kind}};
}

// ── .tpp.types fold ranges ────────────────────────────────────────────────────
static nlohmann::json foldsForTypes(const WorkspaceProject &project)
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
static nlohmann::json foldsForTemplate(const std::string &src, const WorkspaceProject &project)
{
    nlohmann::json result = nlohmann::json::array();
    for (const auto &fold : computeTemplateFoldingRanges(src))
        result.push_back(makeRange(fold.startLine, fold.endLine, fold.kind));
    return result;
}

// ── Entry point ───────────────────────────────────────────────────────────────
nlohmann::json computeFoldingRanges(const std::string &uri, const WorkspaceProject &project)
{
    return project.isTypeUri(uri) ? foldsForTypes(project) : foldsForTemplate(project.getContent(uri), project);
}

} // namespace tpp
