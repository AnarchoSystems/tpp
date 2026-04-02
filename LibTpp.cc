#include <tpp/Compiler.h>

namespace tpp
{
    std::string Program::run() const noexcept
    {
        return "Hello, World!";
    }
    bool FunctionSymbol::bind(const nlohmann::json &input,
                              Program &program,
                              std::vector<Diagnostic> &diagnostics) const noexcept
    {
        return false;
    }

    bool CompilerOutput::get_function(const std::string &functionName,
                                      FunctionSymbol &functionSymbol,
                                      std::vector<Diagnostic> &diagnostics) const noexcept
    {
        return false;
    }

    bool Compiler::add_types(const std::string &typedefs, std::vector<Diagnostic> &diagnostics) noexcept
    {
        return false;
    }

    bool Compiler::compile(const std::string &templateString, CompilerOutput &output, std::vector<Diagnostic> &diagnostics) const noexcept
    {
        return false;
    }
}