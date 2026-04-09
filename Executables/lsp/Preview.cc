#include "Preview.h"
#include <tpp/IR.h>
#include <tpp/RenderMapping.h>
#include <fstream>
#include <sstream>

namespace tpp
{

nlohmann::json renderPreview(const nlohmann::json &params, TppProject *project)
{
    if (!project)
        return {{"error", "No tpp project found for the requested config path"}};

    // ── Read the tpp-config.json previews array ───────────────────────────────
    std::ifstream cfgFile(project->configPath());
    if (!cfgFile)
        return {{"error", "Cannot open tpp-config.json"}};

    nlohmann::json cfg;
    try { cfg = nlohmann::json::parse(cfgFile); }
    catch (const std::exception &e) { return {{"error", std::string("Config parse error: ") + e.what()}}; }

    if (!cfg.contains("previews") || !cfg["previews"].is_array())
        return {{"error", "No 'previews' array in tpp-config.json"}};

    int previewIndex = params.value("previewIndex", 0);
    const auto &previews = cfg["previews"];
    if (previewIndex < 0 || previewIndex >= (int)previews.size())
        return {{"error", "previewIndex out of range"}};

    const auto &preview = previews[previewIndex];
    std::string templateName = preview.value("template", "");
    if (templateName.empty())
        return {{"error", "Preview entry has no 'template' key"}};

    // ── Resolve input JSON ─────────────────────────────────────────────────────
    nlohmann::json inputJson;
    if (preview.contains("input"))
    {
        inputJson = preview["input"];
    }
    else
    {
        inputJson = nlohmann::json::array();
    }

    // ── Render with tracking ──────────────────────────────────────────────────
    std::vector<RenderMapping> mappings;
    std::string output;
    try
    {
        output = project->output().renderTracked(templateName, inputJson, mappings);
    }
    catch (const std::exception &e)
    {
        return {{"error", std::string("Render error: ") + e.what()}};
    }

    // ── Serialise mappings ────────────────────────────────────────────────────
    nlohmann::json mappingsJson = nlohmann::json::array();
    for (const auto &m : mappings)
    {
        mappingsJson.push_back({
            {"sourceRange", {
                {"start", {{"line", m.sourceRange.start.line}, {"character", m.sourceRange.start.character}}},
                {"end",   {{"line", m.sourceRange.end.line},   {"character", m.sourceRange.end.character}}}
            }},
            {"outStart", m.outStart},
            {"outEnd",   m.outEnd}
        });
    }

    return {{"output", output}, {"mappings", mappingsJson}};
}

} // namespace tpp
