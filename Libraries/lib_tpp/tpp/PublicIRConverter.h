#pragma once

#include <tpp/IR.h>
#include <tpp/Policy.h>
#include <tpp/Types.h>

namespace tpp
{
    std::optional<SourceRange> to_public_range(const Range &range, bool include);
    TypeKind to_public_type(const compiler::TypeRef &type);
    std::vector<StructDef> to_public_structs(const std::vector<compiler::StructDef> &defs,
                                             bool includeRanges,
                                             bool includeRawTypedefs);
    std::vector<EnumDef> to_public_enums(const std::vector<compiler::EnumDef> &defs,
                                         bool includeRanges,
                                         bool includeRawTypedefs);
    std::vector<PolicyDef> to_public_policies(const compiler::PolicyRegistry &policies);
    StructDef to_public_struct(const compiler::StructDef &def,
                               bool includeRanges,
                               bool includeRawTypedefs);
    EnumDef to_public_enum(const compiler::EnumDef &def,
                           bool includeRanges,
                           bool includeRawTypedefs);
    PolicyDef to_public_policy(const compiler::Policy &policy);
}