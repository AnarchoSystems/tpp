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
                          std::string &error) const noexcept;

        bool operator==(const CompilerOutput &other) const
        { return functions == other.functions && types == other.types; }

        std::vector<TemplateFunction> functions;
        TypeRegistry types;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(CompilerOutput, functions, types)
    };
}