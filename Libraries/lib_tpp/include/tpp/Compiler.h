#pragma once

#include <tpp/CompilerOutput.h>
#include <tpp/Policy.h>

namespace tpp
{
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

        // Load a single policy from its parsed JSON object.
        // Validates structure and checks tag uniqueness. Appends to error on failure.
        bool add_policy(const nlohmann::json &policyJson, std::string &error) noexcept;

        [[nodiscard]]
        bool compile(CompilerOutput &output) noexcept;

        TypeRegistry types;

    private:
        struct PendingSource
        {
            std::string content;
            std::vector<Diagnostic> *diagnostics; // points to caller-owned vector
            bool isTypes;
        };
        std::vector<PendingSource> pendingSources_;
        PolicyRegistry policies_;
    };
}