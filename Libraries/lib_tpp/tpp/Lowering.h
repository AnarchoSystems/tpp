#pragma once

#include <tpp/AST.h>
#include <tpp/IR.h>
#include <tpp/SemanticModel.h>
#include <vector>

namespace tpp::compiler
{
    // Lower AST-based TemplateFunctions directly to public IR FunctionDefs.
    bool lowerToFunctions(const std::vector<TemplateFunction> &functions,
                          const SemanticModel &semanticModel,
                          std::vector<tpp::FunctionDef> &out,
                          bool includeSourceRanges);
}
