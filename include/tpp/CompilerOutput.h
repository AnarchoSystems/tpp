#pragma once

#include <tpp/FunctionSymbol.h>

namespace tpp
{
    class CompilerOutput
    {
    public:
        [[nodiscard]]
        bool get_function(const std::string &functionName,
                          FunctionSymbol &functionSymbol,
                          std::vector<Diagnostic> &diagnostics) const noexcept;

        std::vector<TemplateFunction> functions;
        TypeRegistry types;
    };
}