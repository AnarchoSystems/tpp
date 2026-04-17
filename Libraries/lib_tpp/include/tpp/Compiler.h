#pragma once

#include <tpp/Diagnostic.h>
#include <tpp/IR.h>
#include <tpp/SemanticModel.h>
#include <tpp/Tooling.h>
#include <nlohmann/json.hpp>
#include <string>
#include <type_traits>
#include <optional>
#include <utility>
#include <vector>

namespace tpp
{
    struct ProjectTextSource
    {
        std::string uri;
        std::string content;
    };

    class TppProject
    {
    public:
        void clear() noexcept;
        void clear_type_sources() noexcept;
        void clear_template_sources() noexcept;
        void clear_policy_sources() noexcept;

        void add_type_source(std::string content, std::string uri = {}) noexcept;

        template <typename Arg, typename = std::void_t<decltype(Arg::tpp_typedefs())>>
        void add_type(std::string uri = {}) noexcept
        {
            using ActualType = std::remove_const_t<std::remove_reference_t<Arg>>;
            add_type_source(ActualType::tpp_typedefs(), std::move(uri));
        }

        void add_template_source(std::string content, std::string uri = {}) noexcept;
        void add_policy_source(std::string content, std::string uri = {}) noexcept;

        const std::vector<ProjectTextSource> &type_sources() const noexcept { return typeSources_; }
        const std::vector<ProjectTextSource> &template_sources() const noexcept { return templateSources_; }
        const std::vector<ProjectTextSource> &policy_sources() const noexcept { return policySources_; }

    private:
        static std::string make_default_uri(const std::string &prefix, std::size_t index);

        std::vector<ProjectTextSource> typeSources_;
        std::vector<ProjectTextSource> templateSources_;
        std::vector<ProjectTextSource> policySources_;
        std::size_t nextTypeSourceIndex_ = 0;
        std::size_t nextTemplateSourceIndex_ = 0;
        std::size_t nextPolicySourceIndex_ = 0;
    };

    struct CompileOptions
    {
        bool includeSourceRanges = false;
        bool includeRawTypedefs = true;
    };

    struct LexedTypeSource
    {
    public:
        LexedTypeSource() = default;
        LexedTypeSource(ProjectTextSource source, std::vector<TypeSourceToken> tokens)
            : source_(std::move(source)), tokens_(std::move(tokens)) {}

        const ProjectTextSource &source() const noexcept { return source_; }
        ProjectTextSource &mutable_source() noexcept { return source_; }
        const std::vector<TypeSourceToken> &tokens() const noexcept { return tokens_; }
        std::vector<TypeSourceToken> &mutable_tokens() noexcept { return tokens_; }

    private:
        ProjectTextSource source_;
        std::vector<TypeSourceToken> tokens_;
    };

    struct LexedTemplateSource
    {
    public:
        LexedTemplateSource() = default;
        LexedTemplateSource(ProjectTextSource source, std::vector<TemplateToken> tokens)
            : source_(std::move(source)), tokens_(std::move(tokens)) {}

        const ProjectTextSource &source() const noexcept { return source_; }
        ProjectTextSource &mutable_source() noexcept { return source_; }
        const std::vector<TemplateToken> &tokens() const noexcept { return tokens_; }
        std::vector<TemplateToken> &mutable_tokens() noexcept { return tokens_; }

    private:
        ProjectTextSource source_;
        std::vector<TemplateToken> tokens_;
    };

    struct LexedProject
    {
    public:
        const std::vector<LexedTypeSource> &type_sources() const noexcept { return typeSources_; }
        std::vector<LexedTypeSource> &mutable_type_sources() noexcept { return typeSources_; }
        const std::vector<LexedTemplateSource> &template_sources() const noexcept { return templateSources_; }
        std::vector<LexedTemplateSource> &mutable_template_sources() noexcept { return templateSources_; }
        const std::vector<ProjectTextSource> &policy_sources() const noexcept { return policySources_; }
        std::vector<ProjectTextSource> &mutable_policy_sources() noexcept { return policySources_; }

        void add_type_source(LexedTypeSource source) { typeSources_.push_back(std::move(source)); }
        void add_template_source(LexedTemplateSource source) { templateSources_.push_back(std::move(source)); }
        void add_policy_source(ProjectTextSource source) { policySources_.push_back(std::move(source)); }

    private:
        std::vector<LexedTypeSource> typeSources_;
        std::vector<LexedTemplateSource> templateSources_;
        std::vector<ProjectTextSource> policySources_;
    };

    struct ParsedTypeArtifact
    {
    public:
        const ProjectTextSource &source() const noexcept { return source_; }
        ProjectTextSource &mutable_source() noexcept { return source_; }
        const std::vector<TypeSourceToken> &tokens() const noexcept { return tokens_; }
        std::vector<TypeSourceToken> &mutable_tokens() noexcept { return tokens_; }
        const std::vector<compiler::StructDef> &structs() const noexcept { return structs_; }
        std::vector<compiler::StructDef> &mutable_structs() noexcept { return structs_; }
        const std::vector<compiler::EnumDef> &enums() const noexcept { return enums_; }
        std::vector<compiler::EnumDef> &mutable_enums() noexcept { return enums_; }

    private:
        ProjectTextSource source_;
        std::vector<TypeSourceToken> tokens_;
        std::vector<compiler::StructDef> structs_;
        std::vector<compiler::EnumDef> enums_;
    };

    struct ParsedTemplateArtifact
    {
    public:
        const ProjectTextSource &source() const noexcept { return source_; }
        ProjectTextSource &mutable_source() noexcept { return source_; }
        const std::vector<TemplateToken> &tokens() const noexcept { return tokens_; }
        std::vector<TemplateToken> &mutable_tokens() noexcept { return tokens_; }
        const ParsedTemplateSource &value() const noexcept { return value_; }
        ParsedTemplateSource &mutable_value() noexcept { return value_; }

    private:
        ProjectTextSource source_;
        std::vector<TemplateToken> tokens_;
        ParsedTemplateSource value_;
    };

    struct ParsedPolicyArtifact
    {
    public:
        const ProjectTextSource &source() const noexcept { return source_; }
        ProjectTextSource &mutable_source() noexcept { return source_; }
        const std::optional<nlohmann::json> &value() const noexcept { return value_; }
        std::optional<nlohmann::json> &mutable_value() noexcept { return value_; }

    private:
        ProjectTextSource source_;
        std::optional<nlohmann::json> value_;
    };

    struct ParsedProject
    {
    public:
        const std::vector<ParsedTypeArtifact> &type_sources() const noexcept { return typeSources_; }
        std::vector<ParsedTypeArtifact> &mutable_type_sources() noexcept { return typeSources_; }
        const std::vector<ParsedTemplateArtifact> &template_sources() const noexcept { return templateSources_; }
        std::vector<ParsedTemplateArtifact> &mutable_template_sources() noexcept { return templateSources_; }
        const std::vector<ParsedPolicyArtifact> &policy_sources() const noexcept { return policySources_; }
        std::vector<ParsedPolicyArtifact> &mutable_policy_sources() noexcept { return policySources_; }

        void add_type_source(ParsedTypeArtifact artifact) { typeSources_.push_back(std::move(artifact)); }
        void add_template_source(ParsedTemplateArtifact artifact) { templateSources_.push_back(std::move(artifact)); }
        void add_policy_source(ParsedPolicyArtifact artifact) { policySources_.push_back(std::move(artifact)); }

    private:
        std::vector<ParsedTypeArtifact> typeSources_;
        std::vector<ParsedTemplateArtifact> templateSources_;
        std::vector<ParsedPolicyArtifact> policySources_;
    };

    struct AnalyzedProject
    {
    public:
        const compiler::SemanticModel &semantic_model() const noexcept { return semanticModel_; }
        compiler::SemanticModel &mutable_semantic_model() noexcept { return semanticModel_; }

    private:
        compiler::SemanticModel semanticModel_;
    };

    [[nodiscard]] bool lex(const TppProject &project,
                           LexedProject &output,
                           std::vector<DiagnosticLSPMessage> &diagnostics,
                           CompileOptions options = {}) noexcept;

    [[nodiscard]] bool parse(const LexedProject &project,
                             ParsedProject &output,
                             std::vector<DiagnosticLSPMessage> &diagnostics,
                             CompileOptions options = {}) noexcept;

    [[nodiscard]] bool analyze(const ParsedProject &project,
                               AnalyzedProject &output,
                               std::vector<DiagnosticLSPMessage> &diagnostics,
                               CompileOptions options = {}) noexcept;

    [[nodiscard]] bool compile(const AnalyzedProject &project,
                               IR &output,
                               std::vector<DiagnosticLSPMessage> &diagnostics,
                               CompileOptions options = {}) noexcept;
}