#pragma once

#include <string>
#include <vector>

namespace tpp
{
    struct ToolingFoldingRange
    {
        int startLine = 0;
        int endLine = 0;
        std::string kind = "region";
    };

    std::vector<ToolingFoldingRange> computeTemplateFoldingRanges(const std::string &src);
}