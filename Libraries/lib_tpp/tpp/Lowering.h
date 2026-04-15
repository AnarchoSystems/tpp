#pragma once

#include <tpp/AST.h>
#include <tpp/IR.h>
#include <tpp/Types.h>
#include <string>
#include <vector>

namespace tpp::compiler
{
    // Lower AST-based TemplateFunctions directly to public IR FunctionDefs.
    // Returns true on success. On failure, sets `error` and returns false.
    bool lowerToFunctions(const std::vector<TemplateFunction> &functions,
                          const TypeRegistry &types,
                          std::vector<tpp::FunctionDef> &out,
                          bool includeSourceRanges,
                          std::string &error);
}
