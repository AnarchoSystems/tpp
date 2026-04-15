#include "tpp/PublicIRConverter.h"

namespace tpp
{

std::optional<SourceRange> to_public_range(const Range &range, bool include)
{
    if (!include)
        return std::nullopt;

    SourceRange sourceRange;
    sourceRange.start.line = range.start.line;
    sourceRange.start.character = range.start.character;
    sourceRange.end.line = range.end.line;
    sourceRange.end.character = range.end.character;
    return sourceRange;
}

TypeKind to_public_type(const compiler::TypeRef &type)
{
    return std::visit([](auto &&arg) -> TypeKind
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, compiler::StringType>)
            return TypeKind{TypeKind::Value{std::in_place_index<0>}};
        else if constexpr (std::is_same_v<T, compiler::IntType>)
            return TypeKind{TypeKind::Value{std::in_place_index<1>}};
        else if constexpr (std::is_same_v<T, compiler::BoolType>)
            return TypeKind{TypeKind::Value{std::in_place_index<2>}};
        else if constexpr (std::is_same_v<T, compiler::NamedType>)
            return TypeKind{TypeKind::Value{std::in_place_index<3>, arg.name}};
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::ListType>>)
            return TypeKind{TypeKind::Value{std::in_place_index<4>,
                std::make_unique<TypeKind>(to_public_type(arg->elementType))}};
        else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::OptionalType>>)
            return TypeKind{TypeKind::Value{std::in_place_index<5>,
                std::make_unique<TypeKind>(to_public_type(arg->innerType))}};
        else
            return TypeKind{TypeKind::Value{std::in_place_index<0>}};
    }, type);
}

std::vector<StructDef> to_public_structs(const std::vector<compiler::StructDef> &defs,
                                         bool includeRanges)
{
    std::vector<StructDef> out;
    out.reserve(defs.size());
    for (const auto &def : defs)
        out.push_back(to_public_struct(def, includeRanges));
    return out;
}

std::vector<EnumDef> to_public_enums(const std::vector<compiler::EnumDef> &defs,
                                     bool includeRanges)
{
    std::vector<EnumDef> out;
    out.reserve(defs.size());
    for (const auto &def : defs)
        out.push_back(to_public_enum(def, includeRanges));
    return out;
}

std::vector<PolicyDef> to_public_policies(const compiler::PolicyRegistry &policies)
{
    std::vector<PolicyDef> out;
    for (const auto &[tag, policy] : policies.all())
        out.push_back(to_public_policy(policy));
    return out;
}

StructDef to_public_struct(const compiler::StructDef &def, bool includeRanges)
{
    StructDef out;
    out.name = def.name;
    for (const auto &field : def.fields)
    {
        FieldDef fieldDef;
        fieldDef.name = field.name;
        fieldDef.type = std::make_unique<TypeKind>(to_public_type(field.type));
        fieldDef.recursive = field.recursive;
        fieldDef.doc = field.doc;
        fieldDef.sourceRange = to_public_range(field.sourceRange, includeRanges);
        out.fields.push_back(std::move(fieldDef));
    }
    out.doc = def.doc;
    out.sourceRange = to_public_range(def.sourceRange, includeRanges);
    return out;
}

EnumDef to_public_enum(const compiler::EnumDef &def, bool includeRanges)
{
    EnumDef out;
    out.name = def.name;
    for (const auto &variant : def.variants)
    {
        VariantDef variantDef;
        variantDef.tag = variant.tag;
        if (variant.payload.has_value())
            variantDef.payload = std::make_unique<TypeKind>(to_public_type(*variant.payload));
        variantDef.recursive = variant.recursive;
        variantDef.doc = variant.doc;
        variantDef.sourceRange = to_public_range(variant.sourceRange, includeRanges);
        out.variants.push_back(std::move(variantDef));
    }
    out.doc = def.doc;
    out.sourceRange = to_public_range(def.sourceRange, includeRanges);
    return out;
}

PolicyDef to_public_policy(const compiler::Policy &policy)
{
    PolicyDef out;
    out.tag = policy.tag;
    if (policy.length.has_value())
    {
        PolicyLength length;
        length.min = policy.length->min;
        length.max = policy.length->max;
        out.length = std::move(length);
    }
    if (policy.rejectIf.has_value())
    {
        PolicyRejectIf rejectIf;
        rejectIf.regex = policy.rejectIf->regex;
        rejectIf.message = policy.rejectIf->message;
        out.rejectIf = std::move(rejectIf);
    }
    for (const auto &require : policy.require)
    {
        PolicyRequire requireDef;
        requireDef.regex = require.regex;
        requireDef.replace = require.replace;
        requireDef.compiledReplace = require.compiled_replace;
        out.require.push_back(std::move(requireDef));
    }
    for (const auto &replacement : policy.replacements)
    {
        PolicyReplacement replacementDef;
        replacementDef.find = replacement.find;
        replacementDef.replace = replacement.replace;
        out.replacements.push_back(std::move(replacementDef));
    }
    for (const auto &filter : policy.outputFilter)
    {
        PolicyOutputFilter filterDef;
        filterDef.regex = filter.regex;
        out.outputFilter.push_back(std::move(filterDef));
    }
    return out;
}

} // namespace tpp