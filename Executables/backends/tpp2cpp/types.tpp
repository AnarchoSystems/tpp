template cpp_ir_type(t: TypeKind)
@switch t@@case Str@std::string@end case@@case Int@int@end case@@case Bool@bool@end case@@case Named(n)@@n@@end case@@case List(e)@std::vector<@cpp_ir_type(e)@>@end case@@case Optional(inner)@std::optional<@cpp_ir_type(inner)@>@end case@@end switch@
END

template cpp_ir_inner_type(t: TypeKind)
@switch t@@case Optional(inner)@@cpp_ir_type(inner)@@end case@@case Str@std::string@end case@@case Int@int@end case@@case Bool@bool@end case@@case Named(n)@@n@@end case@@case List(e)@std::vector<@cpp_ir_type(e)@>@end case@@end switch@
END

template cpp_struct_field_decl(field: FieldDef)
@if field.recursive@
std::unique_ptr<@cpp_ir_inner_type(field.type)@> @field.name@;
@else@
@cpp_ir_type(field.type)@ @field.name@;
@end if@
END

template cpp_read_field(field: FieldDef)
@if field.recursive@
@switch field.type@
@case Optional(inner)@
if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = std::make_unique<@cpp_ir_type(inner)@>(j.at("@field.name@").get<@cpp_ir_type(inner)@>());
@end case@
@case Str@
v.@field.name@ = std::make_unique<std::string>(j.at("@field.name@").get<std::string>());
@end case@
@case Int@
v.@field.name@ = std::make_unique<int>(j.at("@field.name@").get<int>());
@end case@
@case Bool@
v.@field.name@ = std::make_unique<bool>(j.at("@field.name@").get<bool>());
@end case@
@case Named(n)@
v.@field.name@ = std::make_unique<@n@>(j.at("@field.name@").get<@n@>());
@end case@
@case List(e)@
v.@field.name@ = std::make_unique<std::vector<@cpp_ir_type(e)@>>(j.at("@field.name@").get<std::vector<@cpp_ir_type(e)@>>());
@end case@
@end switch@
@else@
@switch field.type@
@case Optional(inner)@
if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = j.at("@field.name@").get<@cpp_ir_type(inner)@>();
@end case@
@case Str@
v.@field.name@ = j.at("@field.name@").get<std::string>();
@end case@
@case Int@
v.@field.name@ = j.at("@field.name@").get<int>();
@end case@
@case Bool@
v.@field.name@ = j.at("@field.name@").get<bool>();
@end case@
@case Named(n)@
v.@field.name@ = j.at("@field.name@").get<@n@>();
@end case@
@case List(e)@
v.@field.name@ = j.at("@field.name@").get<std::vector<@cpp_ir_type(e)@>>();
@end case@
@end switch@
@end if@
END

template cpp_write_field(field: FieldDef)
@if field.recursive@
@switch field.type@
@case Optional(inner)@
if (v.@field.name@) j["@field.name@"] = *v.@field.name@;
@end case@
@case Str@
j["@field.name@"] = *v.@field.name@;
@end case@
@case Int@
j["@field.name@"] = *v.@field.name@;
@end case@
@case Bool@
j["@field.name@"] = *v.@field.name@;
@end case@
@case Named(n)@
j["@field.name@"] = *v.@field.name@;
@end case@
@case List(e)@
j["@field.name@"] = *v.@field.name@;
@end case@
@end switch@
@else@
@switch field.type@
@case Optional(inner)@
if (v.@field.name@.has_value()) j["@field.name@"] = *v.@field.name@;
@end case@
@case Str@
j["@field.name@"] = v.@field.name@;
@end case@
@case Int@
j["@field.name@"] = v.@field.name@;
@end case@
@case Bool@
j["@field.name@"] = v.@field.name@;
@end case@
@case Named(n)@
j["@field.name@"] = v.@field.name@;
@end case@
@case List(e)@
j["@field.name@"] = v.@field.name@;
@end case@
@end switch@
@end if@
END

template render_cpp_enum(e: EnumDef)
@for variant in e.variants@
@if not variant.payload@
struct @e.name@_@variant.tag@
{
    friend void to_json(nlohmann::json& j, const @e.name@_@variant.tag@&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, @e.name@_@variant.tag@&) {}
};
@end if@
@end for@
struct @e.name@
{
    using Value = std::variant<@for variant in e.variants | enumerator=variantIndex sep=", "@@if not variant.payload@@e.name@_@variant.tag@@else@@if variant.recursive@std::unique_ptr<@cpp_ir_type(variant.payload)@>@else@@cpp_ir_type(variant.payload)@@end if@@end if@@end for@>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return @e.rawTypedefs@;
    }
};
END

template render_cpp_struct(s: StructDef)
struct @s.name@
{
    @for field in s.fields@
    @cpp_struct_field_decl(field)@
    @end for@
    static std::string tpp_typedefs() noexcept
    {
        return @s.rawTypedefs@;
    }
};
END

template render_cpp_types(preStructEnums: list<EnumDef>, structs: list<StructDef>, postStructEnums: list<EnumDef>, includes: list<string>, namespaceName: optional<string>)
#pragma once
@for inc in includes@
#include "@inc@"
@end for@
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>
@if namespaceName@
namespace @namespaceName@ {
@end if@
template<typename _TppT>
inline nlohmann::json _tpp_j(const _TppT& v) { return nlohmann::json(v); }
template<typename _TppT>
inline nlohmann::json _tpp_j(const std::unique_ptr<_TppT>& v) { return v ? nlohmann::json(*v) : nlohmann::json(); }

@for e in preStructEnums@
struct @e.name@;
@end for@
@for s in structs@
struct @s.name@;
@end for@
@for e in postStructEnums@
struct @e.name@;
@end for@

@for e in preStructEnums@
@render_cpp_enum(e)@
@end for@
@for s in structs@
@render_cpp_struct(s)@
@end for@
@for e in postStructEnums@
@render_cpp_enum(e)@
@end for@

@for e in preStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v);
inline void to_json(nlohmann::json& j, const @e.name@& v);
@end for@
@for e in postStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v);
inline void to_json(nlohmann::json& j, const @e.name@& v);
@end for@
@for s in structs@
inline void from_json(const nlohmann::json& j, @s.name@& v);
inline void to_json(nlohmann::json& j, const @s.name@& v);
@end for@

@for e in preStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v)
{
    @for variant in e.variants | enumerator=variantIndex sep="\n    else "@
    if (j.contains("@variant.tag@")) @if not variant.payload@v.value.emplace<@variantIndex@>();@else@@if variant.recursive@v.value.emplace<@variantIndex@>(std::make_unique<@cpp_ir_type(variant.payload)@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>()));@else@v.value.emplace<@variantIndex@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>());@end if@@end if@
    @end for@
}
inline void to_json(nlohmann::json& j, const @e.name@& v)
{
    const char* _tags[] = {@for variant in e.variants | sep=", "@"@variant.tag@"@end for@};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
@end for@
@for e in postStructEnums@
inline void from_json(const nlohmann::json& j, @e.name@& v)
{
    @for variant in e.variants | enumerator=variantIndex sep="\n    else "@
    if (j.contains("@variant.tag@")) @if not variant.payload@v.value.emplace<@variantIndex@>();@else@@if variant.recursive@v.value.emplace<@variantIndex@>(std::make_unique<@cpp_ir_type(variant.payload)@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>()));@else@v.value.emplace<@variantIndex@>(j["@variant.tag@"].get<@cpp_ir_type(variant.payload)@>());@end if@@end if@
    @end for@
}
inline void to_json(nlohmann::json& j, const @e.name@& v)
{
    const char* _tags[] = {@for variant in e.variants | sep=", "@"@variant.tag@"@end for@};
    std::visit([&](const auto& arg) {
        j = nlohmann::json::object();
        j[_tags[v.value.index()]] = _tpp_j(arg);
    }, v.value);
}
@end for@
@for s in structs@
inline void from_json(const nlohmann::json& j, @s.name@& v)
{
    @for field in s.fields@
    @cpp_read_field(field)@
    @end for@
}
inline void to_json(nlohmann::json& j, const @s.name@& v)
{
    j = nlohmann::json{};
    @for field in s.fields@
    @cpp_write_field(field)@
    @end for@
}
@end for@
@if namespaceName@
} // namespace @namespaceName@
@end if@
END
