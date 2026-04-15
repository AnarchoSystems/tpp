#pragma once

#include <tpp/IR.h>

namespace tpp
{
    // Assembles the generated public IR from already-converted schema,
    // function, and policy definitions plus raw typedef source.
    IR build_ir(std::vector<StructDef> structs,
                std::vector<EnumDef> enums,
                std::vector<FunctionDef> functions,
                std::vector<PolicyDef> policies,
                const std::string &rawTypedefs,
                bool includeSourceRanges = false);
}
