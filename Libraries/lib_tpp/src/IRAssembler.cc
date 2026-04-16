#include <tpp/IRAssembler.h>
#include "tpp/Version.h"

namespace tpp
{
IR assemble_ir(std::vector<StructDef> structs,
               std::vector<EnumDef> enums,
               std::vector<FunctionDef> functions,
               std::vector<PolicyDef> policies,
               bool)
{
    IR out;
    out.versionMajor = TPP_VERSION_MAJOR;
    out.versionMinor = TPP_VERSION_MINOR;
    out.versionPatch = TPP_VERSION_PATCH;

    out.structs = std::move(structs);
    out.enums = std::move(enums);
    out.functions = std::move(functions);
    out.policies = std::move(policies);

    return out;
}

} // namespace tpp