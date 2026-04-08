#pragma once

#include "TppProject.h"
#include <nlohmann/json.hpp>
#include <string>

namespace tpp
{
    // Returns a JSON array of LSP FoldingRange objects.
    nlohmann::json computeFoldingRanges(const std::string &uri, const TppProject &project);
}
