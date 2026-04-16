#pragma once

#include <tpp/Diagnostic.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <optional>
#include <string_view>

namespace tpp::compiler
{
    class SemanticModel;

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
        bool recursive = false;
        Range sourceRange{};
        std::string doc; // doc comment text attached to this field (/// or /** */)
    };

    struct StructDef
    {
        std::string name;
        std::vector<FieldDef> fields;
        Range sourceRange{};
        std::string doc; // doc comment text attached to this struct (/// or /** */)
        std::string rawTypedefs;
    };

    struct VariantDef
    {
        std::string tag;
        std::optional<TypeRef> payload;
        bool recursive = false;
        Range sourceRange{};
        std::string doc; // doc comment text attached to this variant tag (/// or /** */)
    };

    struct EnumDef
    {
        std::string name;
        std::vector<VariantDef> variants;
        Range sourceRange{};
        std::string doc; // doc comment text attached to this enum (/// or /** */)
        std::string rawTypedefs;
    };

    // ── Type lookup ──

    enum class TypeKind { Struct, Enum };

    struct TypeEntry
    {
        TypeKind kind;
        std::size_t index;
    };

    // ─────────────────────────────────────────────────────────────────
    // Equality
    // ─────────────────────────────────────────────────────────────────

    inline bool operator==(const StringType &, const StringType &) { return true; }
    inline bool operator==(const IntType &, const IntType &)       { return true; }
    inline bool operator==(const BoolType &, const BoolType &)     { return true; }
    inline bool operator==(const NamedType &a, const NamedType &b) { return a.name == b.name; }

    // Forward declaration so ListType/OptionalType can use it
    inline bool operator==(const TypeRef &a, const TypeRef &b);

    inline bool operator==(const ListType &a, const ListType &b)
    { return a.elementType == b.elementType; }
    inline bool operator==(const OptionalType &a, const OptionalType &b)
    { return a.innerType == b.innerType; }

    inline bool operator==(const TypeRef &a, const TypeRef &b)
    {
        if (a.index() != b.index()) return false;
        return std::visit([&](auto &&av) -> bool
        {
            using T = std::decay_t<decltype(av)>;
            const auto &bv = std::get<T>(b);
            if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
                return av && bv && *av == *bv;
            else if constexpr (std::is_same_v<T, std::shared_ptr<OptionalType>>)
                return av && bv && *av == *bv;
            else
                return av == bv;
        }, a);
    }

    inline bool operator==(const FieldDef &a, const FieldDef &b)
    { return a.name == b.name && a.type == b.type; }
    inline bool operator==(const StructDef &a, const StructDef &b)
    { return a.name == b.name && a.fields == b.fields; }
    inline bool operator==(const VariantDef &a, const VariantDef &b)
    { return a.tag == b.tag && a.payload == b.payload; }
    inline bool operator==(const EnumDef &a, const EnumDef &b)
    { return a.name == b.name && a.variants == b.variants; }
}

