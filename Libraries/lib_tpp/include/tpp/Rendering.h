#pragma once

#include <tpp/IR.h>
#include <tpp/RenderMapping.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace tpp
{
    /// Render a function from the IR against JSON input.
    bool render_function(const IR &ir, const FunctionDef &function,
                         const nlohmann::json &input,
                         std::string &output, std::string &error,
                         std::vector<RenderMapping> *tracking = nullptr);

    /// Look up a function by name in the IR.
    bool get_function(const IR &ir, const std::string &functionName,
                      const FunctionDef *&function, std::string &error);

    /// Render a function with source-mapping tracking (for LSP preview).
    std::string renderTracked(const IR &ir, const std::string &functionName,
                              const nlohmann::json &input,
                              std::vector<RenderMapping> &mappings);
} // namespace tpp
