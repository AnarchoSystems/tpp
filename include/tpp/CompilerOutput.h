#pragma once

#include <tpp/FunctionSymbol.h>
#include <tpp/Policy.h>
#include <tpp/ArgType.h>
#include <tpp/RenderMapping.h>

namespace tpp
{
    class CompilerOutput
    {
    public:
        [[nodiscard]]
        bool get_function(const std::string &functionName,
                          FunctionSymbol &functionSymbol,
                          std::string &error) const noexcept;

        template <typename... Args>
        [[nodiscard]]
        std::function<std::string(typename ArgType<Args>::type...)> get_function(const std::string &functionName) const
        {
            FunctionSymbol fs;
            std::string error;
            if (!get_function(functionName, fs, error))
            {
                throw std::runtime_error(error);
            }
            return [fs](typename ArgType<Args>::type... args) -> std::string
            {
                nlohmann::json input = nlohmann::json::array({args...});
                std::string output;
                std::string error;
                if (!fs.render(input, output, error))
                {
                    throw std::runtime_error(error);
                }
                return output;
            };
        }

        // Render a named function against the given JSON input and collect source->output mappings
        // for every ForNode, IfNode, and SwitchNode encountered during rendering.
        // Throws std::runtime_error on any render error.
        [[nodiscard]]
        std::string renderTracked(const std::string &functionName,
                                  const nlohmann::json &input,
                                  std::vector<RenderMapping> &mappings) const;

        bool operator==(const CompilerOutput &other) const
        {
            return functions == other.functions && types == other.types && policies == other.policies
                   && raw_typedefs == other.raw_typedefs;
        }

        std::vector<TemplateFunction> functions;
        TypeRegistry types;
        PolicyRegistry policies;
        std::string raw_typedefs;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CompilerOutput, functions, types, policies, raw_typedefs)
    };
}