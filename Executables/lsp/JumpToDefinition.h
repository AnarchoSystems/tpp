#pragma once

#include "TppProject.h"
#include <nlohmann/json.hpp>
#include <string>

namespace tpp
{
    // Returns an LSP Location (or null JSON) for jump-to-definition.
    nlohmann::json jumpToDefinition(const std::string &uri,
                                    int line, int character,
                                    const WorkspaceProject &project);

    // Returns an LSP Hover object (or null JSON) for the symbol at the cursor.
    nlohmann::json hoverAtPosition(const std::string &uri,
                                   int line, int character,
                                   const WorkspaceProject &project);
}
