#pragma once

#include <tpp/Diagnostic.h>
#include <tpp/IR.h>
#include <string>
#include <type_traits>
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

    struct CompileResult
    {
        IR ir;
        std::vector<DiagnosticLSPMessage> diagnostics;
        bool success = false;

        explicit operator bool() const noexcept { return success; }
    };

    [[nodiscard]] CompileResult compile(const TppProject &project,
                                        CompileOptions options = {}) noexcept;
}