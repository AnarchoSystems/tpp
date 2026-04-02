#pragma once

#include <nlohmann/json.hpp>
#include <tpp/Program.h>
#include <tpp/Diagnostic.h>

namespace tpp
{
    class FunctionSymbol
    {
    public:
        [[nodiscard]]
        bool bind(const nlohmann::json &input,
                  Program &program,
                  std::vector<Diagnostic> &diagnostics) const noexcept;
    };
}