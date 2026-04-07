#pragma once

#include <nlohmann/json.hpp>
#include <tpp/AST.h>
#include <tpp/Diagnostic.h>
#include <tpp/Policy.h>

namespace tpp
{
    class FunctionSymbol
    {
    public:
        [[nodiscard]]
        bool render(const nlohmann::json &input,
                    std::string &output,
                    std::string &error) const noexcept;

        TemplateFunction function;
        TypeRegistry types;
        std::vector<TemplateFunction> allFunctions;
        PolicyRegistry policies;
    };
}