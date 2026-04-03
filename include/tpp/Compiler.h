#pragma once

#include <tpp/CompilerOutput.h>

namespace tpp
{
    class Compiler
    {
    public:
        void clear_types() noexcept;
        [[nodiscard]]
        bool add_types(const std::string &typedefs,
                       std::vector<Diagnostic> &diagnostics) noexcept;
        [[nodiscard]]
        bool compile(const std::string &templateString,
                     CompilerOutput &output,
                     std::vector<Diagnostic> &diagnostics) const noexcept;

        TypeRegistry types;
    };
}