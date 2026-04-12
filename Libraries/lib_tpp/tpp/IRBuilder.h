#pragma once

#include <tpp/IR.h>
#include <tpp/Types.h>
#include <tpp/Instruction.h>
#include <tpp/Policy.h>

namespace tpp
{
    // Converts internal compiler types to the generated IR.
    // When includeSourceRanges is true, the IR will contain source location
    // information for structs, enums, fields, variants, and functions.
    IR build_ir(const compiler::TypeRegistry &types,
                const std::vector<compiler::InstructionFunction> &functions,
                const compiler::PolicyRegistry &policies,
                const std::string &rawTypedefs,
                bool includeSourceRanges = false);
}
