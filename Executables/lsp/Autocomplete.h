#pragma once

#include "TppProject.h"
#include <nlohmann/json.hpp>
#include <string>

namespace tpp
{
    // Returns a JSON array of LSP CompletionItem objects.
    nlohmann::json computeCompletions(const std::string &uri,
                                      int line, int character,
                                      const TppProject &project);
}
