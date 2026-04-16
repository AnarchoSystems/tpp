#pragma once

#include <tpp/IR.h>

namespace tpp
{
    // Assembles the generated public IR from already-converted schema,
    // function, and policy definitions.
    IR assemble_ir(std::vector<StructDef> structs,
                   std::vector<EnumDef> enums,
                   std::vector<FunctionDef> functions,
                   std::vector<PolicyDef> policies,
                   bool includeSourceRanges = false);
}