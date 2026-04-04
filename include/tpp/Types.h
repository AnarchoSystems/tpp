#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <optional>

namespace tpp
{
    // ── Primitive types ──

    struct StringType  {};
    struct IntType     {};
    struct BoolType    {};
    struct ListType;
    struct OptionalType;
    struct NamedType { std::string name; };

    using TypeRef = std::variant<StringType, IntType, BoolType,
                                 std::shared_ptr<ListType>,
                                 std::shared_ptr<OptionalType>,
                                 NamedType>;

    struct ListType     { TypeRef elementType; };
    struct OptionalType { TypeRef innerType; };

    // ── Schema definitions ──

    struct FieldDef
    {
        std::string name;
        TypeRef type;
    };

    struct StructDef
    {
        std::string name;
        std::vector<FieldDef> fields;
    };

    struct VariantDef
    {
        std::string tag;
        std::optional<TypeRef> payload;
    };

    struct EnumDef
    {
        std::string name;
        std::vector<VariantDef> variants;
    };

    // ── Type registry ──

    enum class TypeKind { Struct, Enum };

    struct TypeEntry
    {
        TypeKind kind;
        std::size_t index;
    };

    struct TypeRegistry
    {
        std::vector<StructDef> structs;
        std::vector<EnumDef> enums;
        std::map<std::string, TypeEntry> nameIndex;
    };
}
