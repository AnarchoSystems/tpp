#pragma once

#include <tpp/CompilerOutput.h>

namespace tpp
{
    class Compiler
    {
    public:
        void clear_types() noexcept;
        void add_types(const std::string &typedefs,
                       std::vector<Diagnostic> &diagnostics) noexcept;

        void clear_templates() noexcept;
        void add_templates(const std::string &templateString,
                           std::vector<Diagnostic> &diagnostics) noexcept;
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
    };
}