#pragma once

#include "TppProject.h"
#include <nlohmann/json.hpp>
#include <string>

namespace tpp
{
    // Returns an LSP Location (or null JSON) for jump-to-definition.
    nlohmann::json jumpToDefinition(const std::string &uri,
                                    int line, int character,
                                    const TppProject &project);
}
