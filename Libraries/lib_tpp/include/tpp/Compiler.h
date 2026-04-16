#pragma once

#include <tpp/SemanticModel.h>
#include <tpp/Diagnostic.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace tpp
{
    struct IR;  // forward declaration — include <tpp/IR.h> for the full type

    class Compiler
    {
    public:
        void clear_types() noexcept;
        void add_types(const std::string &typedefs,
                       std::vector<Diagnostic> &diagnostics) noexcept;

        template <typename Arg, typename = std::void_t<decltype(Arg::tpp_typedefs())>>
        void add_type(std::vector<Diagnostic> &diagnostics) noexcept
        {
            using ActType = std::remove_const_t<std::remove_reference_t<Arg>>;
            add_types(ActType::tpp_typedefs(), diagnostics);
        }

        void clear_templates() noexcept;
        void add_templates(const std::string &templateString,
                           std::vector<Diagnostic> &diagnostics) noexcept;

        void clear_policies() noexcept;

        // Load a single policy from raw file text.
        // Parses JSON, validates structure, and appends diagnostics on failure.
        bool add_policy_text(const std::string &policyText,
                     std::vector<Diagnostic> &diagnostics) noexcept;

        [[nodiscard]]
        bool compile(IR &output, bool includeSourceRanges = false) noexcept;

        const compiler::SemanticModel &semantic_model() const noexcept { return semanticModel_; }

    private:
        struct PendingSource
        {
            std::string content;
            std::vector<Diagnostic> *diagnostics; // points to caller-owned vector
            bool isTypes;
        };
        std::vector<PendingSource> pendingSources_;
        compiler::SemanticModel semanticModel_;
    };
}