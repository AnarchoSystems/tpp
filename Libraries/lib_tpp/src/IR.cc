#include <tpp/IR.h>
#include <tpp/FunctionSymbol.h>

namespace tpp
{

    // ═══════════════════════════════════════════════════════════════════
    // Function Lookup  (get_function)
    // ═══════════════════════════════════════════════════════════════════

    bool IR::get_function(const std::string &functionName,
                                      FunctionSymbol &functionSymbol,
                                      std::string &error) const noexcept
    {
        try
        {
            for (auto &f : functions)
            {
                if (f.name == functionName)
                {
                    functionSymbol.function = f;
                    functionSymbol.types = types;
                    functionSymbol.allFunctions = functions;
                    functionSymbol.policies = policies;
                    return true;
                }
            }
            error = "function '" + functionName + "' not found";
            return false;
        }
        catch (...)
        {
            return false;
        }
    }

} // namespace tpp
