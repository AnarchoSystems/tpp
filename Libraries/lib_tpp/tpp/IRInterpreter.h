#pragma once

#include <tpp/IR.h>
#include <tpp/RenderMapping.h>

#include <nlohmann/json.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tpp
{
    class IRInterpreter
    {
    public:
        explicit IRInterpreter(const IR &ir);
        ~IRInterpreter();

        IRInterpreter(const IRInterpreter &) = delete;
        IRInterpreter &operator=(const IRInterpreter &) = delete;

        bool render(const FunctionDef &function,
                    std::map<std::string, nlohmann::json> bindings);
        std::string take_output(std::vector<RenderMapping> *tracking = nullptr);
        const std::string &error() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace tpp