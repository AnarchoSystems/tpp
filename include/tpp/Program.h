#pragma once

#include <tpp/AST.h>
#include <nlohmann/json.hpp>

namespace tpp
{
    // ── Program ──

    class Program
    {
    public:
        std::string run() const noexcept;

        TemplateFunction function;
        nlohmann::json boundArgs;
        TypeRegistry types;
        std::vector<TemplateFunction> allFunctions;
    };
}