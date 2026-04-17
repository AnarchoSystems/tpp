#pragma once

#include <tpp/Tooling.h>
#include <tpp/AST.h>
#include <tpp/Policy.h>
#include <tpp/Types.h>
#include <string>
#include <map>
#include <optional>
#include <vector>

namespace tpp::compiler
{
    class SemanticModel
    {
    public:
        void clear_types() noexcept
        {
            structs.clear();
            enums.clear();
            nameIndex.clear();
        }

        const std::vector<StructDef> &structs_view() const noexcept { return structs; }
        const std::vector<EnumDef> &enums_view() const noexcept { return enums; }

        const std::vector<TemplateFunction> &functions() const noexcept { return functions_; }
        std::vector<TemplateFunction> &mutable_functions() noexcept { return functions_; }

        const std::vector<std::vector<tpp::TypeSourceSemanticSpan>> &type_source_files() const noexcept
        {
            return typeSourceFiles_;
        }

        std::vector<std::vector<tpp::TypeSourceSemanticSpan>> &mutable_type_source_files() noexcept
        {
            return typeSourceFiles_;
        }

        const PolicyRegistry &policies() const noexcept { return policies_; }
        PolicyRegistry &mutable_policies() noexcept { return policies_; }

        const TypeEntry *find_type_entry(std::string_view name) const noexcept
        {
            auto it = nameIndex.find(std::string(name));
            return it == nameIndex.end() ? nullptr : &it->second;
        }

        const StructDef *find_struct(std::string_view name) const noexcept
        {
            const auto *entry = find_type_entry(name);
            if (!entry || entry->kind != TypeKind::Struct)
                return nullptr;
            return &structs[entry->index];
        }

        const EnumDef *find_enum(std::string_view name) const noexcept
        {
            const auto *entry = find_type_entry(name);
            if (!entry || entry->kind != TypeKind::Enum)
                return nullptr;
            return &enums[entry->index];
        }

        bool has_named_type(std::string_view name) const noexcept
        {
            return find_type_entry(name) != nullptr;
        }

        const StructDef *resolve_struct(const TypeRef &type) const noexcept
        {
            if (auto named = std::get_if<NamedType>(&type))
                return find_struct(named->name);
            if (auto optionalType = std::get_if<std::shared_ptr<OptionalType>>(&type))
                return resolve_struct((*optionalType)->innerType);
            return nullptr;
        }

        const EnumDef *resolve_enum(const TypeRef &type) const noexcept
        {
            if (auto named = std::get_if<NamedType>(&type))
                return find_enum(named->name);
            if (auto optionalType = std::get_if<std::shared_ptr<OptionalType>>(&type))
                return resolve_enum((*optionalType)->innerType);
            return nullptr;
        }

        const FieldDef *find_field(const StructDef *structDef, std::string_view fieldName) const noexcept
        {
            if (!structDef)
                return nullptr;
            for (const auto &field : structDef->fields)
                if (field.name == fieldName)
                    return &field;
            return nullptr;
        }

        const FieldDef *find_field(const TypeRef &type, std::string_view fieldName) const noexcept
        {
            return find_field(resolve_struct(type), fieldName);
        }

        const VariantDef *find_variant(const EnumDef *enumDef, std::string_view tag) const noexcept
        {
            if (!enumDef)
                return nullptr;
            for (const auto &variant : enumDef->variants)
                if (variant.tag == tag)
                    return &variant;
            return nullptr;
        }

        std::optional<std::string> find_undefined_named_type(const TypeRef &type) const noexcept
        {
            if (auto named = std::get_if<NamedType>(&type))
                return !has_named_type(named->name)
                    ? std::make_optional(named->name)
                    : std::nullopt;
            if (auto optionalType = std::get_if<std::shared_ptr<OptionalType>>(&type))
                return find_undefined_named_type((*optionalType)->innerType);
            if (auto listType = std::get_if<std::shared_ptr<ListType>>(&type))
                return find_undefined_named_type((*listType)->elementType);
            return std::nullopt;
        }

        static std::vector<const TemplateFunction *> find_template_overloads(const std::vector<TemplateFunction> &functions,
                                                                             std::string_view name)
        {
            std::vector<const TemplateFunction *> overloads;
            for (const auto &function : functions)
                if (function.name == name)
                    overloads.push_back(&function);
            return overloads;
        }

        std::vector<const TemplateFunction *> find_template_overloads(std::string_view name) const
        {
            return find_template_overloads(functions_, name);
        }

        std::vector<StructDef> structs;
        std::vector<EnumDef> enums;
        std::map<std::string, TypeEntry> nameIndex;

    private:
        std::vector<TemplateFunction> functions_;
        std::vector<std::vector<tpp::TypeSourceSemanticSpan>> typeSourceFiles_;
        PolicyRegistry policies_;
    };

}