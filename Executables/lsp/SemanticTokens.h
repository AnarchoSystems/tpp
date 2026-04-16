#pragma once

#include "TppProject.h"
#include <nlohmann/json.hpp>
#include <string>

namespace tpp
{
    // Returns {"data": [...]} — the LSP delta-encoded semantic token array.
    nlohmann::json computeSemanticTokens(const std::string &uri, const WorkspaceProject &project);

    // Standalone variant — parses text directly, no project/type info needed.
    nlohmann::json computeSemanticTokensFromText(const std::string &uri, const std::string &text);
}
