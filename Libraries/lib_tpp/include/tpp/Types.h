#pragma once

#include <tpp/Diagnostic.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace tpp::compiler
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
    inline bool operator==(const TypeRegistry &a, const TypeRegistry &b)
    { return a.structs == b.structs && a.enums == b.enums; }

    // ─────────────────────────────────────────────────────────────────
    // JSON serialization
    // ─────────────────────────────────────────────────────────────────

    inline void to_json(nlohmann::json &j, const TypeRef &t)
    {
        std::visit([&](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, StringType>)
                j = {{"kind", "string"}};
            else if constexpr (std::is_same_v<T, IntType>)
                j = {{"kind", "int"}};
            else if constexpr (std::is_same_v<T, BoolType>)
                j = {{"kind", "bool"}};
            else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
            {
                nlohmann::json elem;
                to_json(elem, arg->elementType);
                j = {{"kind", "list"}, {"elementType", elem}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<OptionalType>>)
            {
                nlohmann::json inner;
                to_json(inner, arg->innerType);
                j = {{"kind", "optional"}, {"innerType", inner}};
            }
            else if constexpr (std::is_same_v<T, NamedType>)
                j = {{"kind", "named"}, {"name", arg.name}};
        }, t);
    }

    inline void from_json(const nlohmann::json &j, TypeRef &t)
    {
        std::string kind = j.at("kind").get<std::string>();
        if (kind == "string")
            t = StringType{};
        else if (kind == "int")
            t = IntType{};
        else if (kind == "bool")
            t = BoolType{};
        else if (kind == "list")
            t = std::make_shared<ListType>(ListType{j.at("elementType").get<TypeRef>()});
        else if (kind == "optional")
            t = std::make_shared<OptionalType>(OptionalType{j.at("innerType").get<TypeRef>()});
        else if (kind == "named")
            t = NamedType{j.at("name").get<std::string>()};
    }

    inline void to_json(nlohmann::json &j, const FieldDef &f)
    {
        nlohmann::json typeJson;
        to_json(typeJson, f.type);
        j = {{"name", f.name}, {"type", typeJson}};
        if (f.recursive) j["recursive"] = true;
        if (!f.doc.empty()) j["doc"] = f.doc;
    }
    inline void from_json(const nlohmann::json &j, FieldDef &f)
    {
        j.at("name").get_to(f.name);
        f.type = j.at("type").get<TypeRef>();
        f.recursive = j.value("recursive", false);
        f.doc = j.value("doc", std::string{});
    }

    inline void to_json(nlohmann::json &j, const StructDef &s)
    {
        j = {{"name", s.name}, {"fields", s.fields}};
        if (!s.doc.empty()) j["doc"] = s.doc;
    }
    inline void from_json(const nlohmann::json &j, StructDef &s)
    {
        j.at("name").get_to(s.name);
        s.fields = j.at("fields").get<std::vector<FieldDef>>();
        s.doc = j.value("doc", std::string{});
    }

    inline void to_json(nlohmann::json &j, const VariantDef &v)
    {
        j = {{"tag", v.tag}};
        if (v.payload.has_value())
        {
            nlohmann::json payloadJson;
            to_json(payloadJson, v.payload.value());
            j["payload"] = payloadJson;
        }
        if (v.recursive) j["recursive"] = true;
        if (!v.doc.empty()) j["doc"] = v.doc;
    }
    inline void from_json(const nlohmann::json &j, VariantDef &v)
    {
        j.at("tag").get_to(v.tag);
        if (j.contains("payload"))
            v.payload = j.at("payload").get<TypeRef>();
        else
            v.payload = std::nullopt;
        v.recursive = j.value("recursive", false);
        v.doc = j.value("doc", std::string{});
    }

    inline void to_json(nlohmann::json &j, const EnumDef &e)
    {
        j = {{"name", e.name}, {"variants", e.variants}};
        if (!e.doc.empty()) j["doc"] = e.doc;
    }
    inline void from_json(const nlohmann::json &j, EnumDef &e)
    {
        j.at("name").get_to(e.name);
        e.variants = j.at("variants").get<std::vector<VariantDef>>();
        e.doc = j.value("doc", std::string{});
    }

    inline void to_json(nlohmann::json &j, const TypeRegistry &r)
    { j = {{"structs", r.structs}, {"enums", r.enums}}; }
    inline void from_json(const nlohmann::json &j, TypeRegistry &r)
    {
        r.structs = j.at("structs").get<std::vector<StructDef>>();
        r.enums   = j.at("enums").get<std::vector<EnumDef>>();
        r.nameIndex.clear();
        for (std::size_t i = 0; i < r.structs.size(); ++i)
            r.nameIndex[r.structs[i].name] = {TypeKind::Struct, i};
        for (std::size_t i = 0; i < r.enums.size(); ++i)
            r.nameIndex[r.enums[i].name] = {TypeKind::Enum, i};
    }

}

