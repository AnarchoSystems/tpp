template render_templates(types: string, template: string)
#pragma once

constexpr char types_content[] = @types@;
constexpr char template_content[] =  @template@;
END

template render_cpp_types(input: CppTypesInput)
#pragma once
@for inc in input.includes@
#include "@inc@"
@end for@
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>
@if input.namespaceName@
namespace @input.namespaceName@ {
@end if@
template<typename _TppT>
inline nlohmann::json _tpp_j(const _TppT& v) { return nlohmann::json(v); }
template<typename _TppT>
inline nlohmann::json _tpp_j(const std::unique_ptr<_TppT>& v) { return v ? nlohmann::json(*v) : nlohmann::json(); }

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
struct @s.name@;
@end case@
@case Enum(e)@
struct @e.name@;
@end case@
@end switch@
@end for@

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
@if s.doc@
/// @s.doc@
@end if@
struct @s.name@
{
    @for field in s.fields@
    @if field.doc@
    /// @field.doc@
    @end if@
    @field.type@ @field.name@;
    @end for@
    static std::string tpp_typedefs() noexcept
    {
        return @s.definition@;
    }
};
@end case@
@case Enum(e)@
@if e.doc@
/// @e.doc@
@end if@
@for variant in e.variants@
@if not variant.payloadType@
@if variant.doc@
/// @variant.doc@
@end if@
struct @e.name@_@variant.tag@
{
    friend void to_json(nlohmann::json& j, const @e.name@_@variant.tag@&) { j = nlohmann::json::object(); }
    friend void from_json(const nlohmann::json&, @e.name@_@variant.tag@&) {}
};
@end if@
@end for@
struct @e.name@
{
    using Value = std::variant<@for variant in e.variants | sep=", "@@if not variant.payloadType@@e.name@_@variant.tag@@else@@if variant.isRecursivePayload@std::unique_ptr<@variant.payloadType@>@else@@variant.payloadType@@end if@@end if@@end for@>;
    Value value;
    static std::string tpp_typedefs() noexcept
    {
        return @e.definition@;
    }
};
@end case@
@end switch@
@end for@

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
inline void from_json(const nlohmann::json& j, @s.name@& v);
inline void to_json(nlohmann::json& j, const @s.name@& v);
@end case@
@case Enum(e)@
inline void from_json(const nlohmann::json& j, @e.name@& v);
inline void to_json(nlohmann::json& j, const @e.name@& v);
@end case@
@end switch@
@end for@

@for typeDef in input.types@
@switch typeDef@
@case Struct(s)@
inline void from_json(const nlohmann::json& j, @s.name@& v)
{
    @for field in s.fields@
    @if field.recursiveInnerType@
    @if field.isOptional@
    if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = std::make_unique<@field.recursiveInnerType@>(j.at("@field.name@").get<@field.recursiveInnerType@>());
    @else@
    v.@field.name@ = std::make_unique<@field.recursiveInnerType@>(j.at("@field.name@").get<@field.recursiveInnerType@>());
    @end if@
    @else@
    @if field.innerType@
    if (j.contains("@field.name@") && !j.at("@field.name@").is_null()) v.@field.name@ = j.at("@field.name@").get<@field.innerType@>();
    @else@
    j.at("@field.name@").get_to(v.@field.name@);
    @end if@
    @end if@
    @end for@
}
inline void to_json(nlohmann::json& j, const @s.name@& v)
{
    j = nlohmann::json{};
    @for field in s.fields@
    @if field.recursiveInnerType@
    @if field.isOptional@
    if (v.@field.name@) j["@field.name@"] = *v.@field.name@;
    @else@
    j["@field.name@"] = *v.@field.name@;
    @end if@
    @else@
    @if field.isOptional@
    if (v.@field.name@.has_value()) j["@field.name@"] = *v.@field.name@;
    @else@
    j["@field.name@"] = v.@field.name@;
    @end if@
    @end if@
    @end for@
}
@end case@
@case Enum(e)@
inline void from_json(const nlohmann::json& j, @e.name@& v)
{
    @for variant in e.variants | sep="\n    else "@
    if (j.contains("@variant.tag@")) @if not variant.payloadType@v.value.emplace<@variant.index@>();@else@@if variant.isRecursivePayload@v.value.emplace<@variant.index@>(std::make_unique<@variant.payloadType@>(j["@variant.tag@"].get<@variant.payloadType@>()));@else@v.value.emplace<@variant.index@>(j["@variant.tag@"].get<@variant.payloadType@>());@end if@@end if@
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
@end case@
@end switch@
@end for@
@if input.namespaceName@
} // namespace @input.namespaceName@
@end if@
END

template render_cpp_functions(input: CppFunctionsInput)
#pragma once
#include <tpp/ArgType.h>
@for inc in input.includes@
#include "@inc@"
@end for@
#include <string>
@if input.namespaceName@
namespace @input.namespaceName@ {
@end if@
@for function in input.functions@
@if function.doc@
/// @function.doc@
@end if@
std::string @function.name@(@for param in function.params | sep=", "@typename tpp::ArgType<@param.type@>::type @param.name@@end for@);
@end for@
@if input.namespaceName@
} // namespace @input.namespaceName@
@end if@
END

template render_cpp_implementation(input: CppFunctionsInput)
@for inc in input.includes@
#include "@inc@"
@end for@
#include <tpp/Compiler.h>
#include <nlohmann/json.hpp>
@if input.namespaceName@
namespace @input.namespaceName@ {
@end if@
static const tpp::CompilerOutput& _getCompilerOutput()
{
    static const tpp::CompilerOutput co =
        nlohmann::json::parse(@input.compilerOutputJson@).get<tpp::CompilerOutput>();
    return co;
}
@for function in input.functions@
std::string @function.name@(@for param in function.params | sep=", "@typename tpp::ArgType<@param.type@>::type @param.name@@end for@)
{
    const auto& co = _getCompilerOutput();
    tpp::FunctionSymbol fs;
    std::string error;
    if (!co.get_function("@function.name@", fs, error)) return {};
    nlohmann::json _tpp_input = nlohmann::json::array({@for param in function.params | sep=", "@nlohmann::json(@param.name@)@end for@});
    std::string output;
    if (!fs.render(_tpp_input, output, error)) return {};
    return output;
}
@end for@
@if input.namespaceName@
} // namespace @input.namespaceName@
@end if@
END
