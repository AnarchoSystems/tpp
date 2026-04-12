#pragma once

#include <tpp/AST.h>
#include <tpp/Instruction.h>
#include <tpp/Types.h>
#include <string>
#include <vector>

namespace tpp::compiler
{
    // Lower AST-based TemplateFunctions to type-resolved InstructionFunctions.
    // Returns true on success.  On failure, sets `error` and returns false.
    bool lowerToInstructions(const std::vector<TemplateFunction> &functions,
                             const TypeRegistry &types,
                             std::vector<InstructionFunction> &out,
                             std::string &error);
}
