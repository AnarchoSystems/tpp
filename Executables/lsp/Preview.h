#pragma once

#include "TppProject.h"
#include <nlohmann/json.hpp>
#include <string>

namespace tpp
{
    // Handle the custom tpp/renderPreview request.
    // params: { "configPath": string, "previewIndex": number, "templateText"?: string }
    // Returns: { "output": string, "mappings": [RenderMapping...] }
    //       or { "error": string } on failure.
    // project may be null if no project owns the config path.
    nlohmann::json renderPreview(const nlohmann::json &params,
                                 WorkspaceProject *project);
}
