#pragma once

#include <tpp/Compiler.h>

#include <map>
#include <string>
#include <vector>

namespace tpp
{
    struct ToolingSourceLocation
    {
        std::string uri;
        Range range;
    };

    struct ToolingSymbolIndex
    {
        std::map<std::string, ToolingSourceLocation> namedTypeLocations;
        std::map<std::string, std::map<std::string, ToolingSourceLocation>> structFieldLocations;
    };

    [[nodiscard]] bool compile_for_tooling(const TppProject &project,
                                           IR &output,
                                           std::vector<DiagnosticLSPMessage> &diagnostics,
                                           ToolingSymbolIndex &symbolIndex,
                                           CompileOptions options = {}) noexcept;
}