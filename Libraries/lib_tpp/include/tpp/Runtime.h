#pragma once

#include <tpp/ArgType.h>
#include <tpp/IR.h>
#include <tpp/RenderMapping.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // Dynamic-mode entry points (used by render-tpp, tests, LSP preview)
    //
    // The Runner interprets IR instructions (control flow, variable binding)
    // and delegates output to the VirtualMachine (scalar emission, policies,
    // alignment).  Each function gets a pre-allocated slot array; the Runner
    // indexes directly by function index — no frame stack, no heap alloc
    // during execution.
    // ════════════════════════════════════════════════════════════════════════════

    [[nodiscard]] bool get_function(const IR &ir, const std::string &name,
                                    const FunctionDef *&out, std::string &error);

    [[nodiscard]] bool get_function(const IR &ir, const std::string &name,
                                    const std::vector<std::string> &signature,
                                    const FunctionDef *&out, std::string &error);

    [[nodiscard]] bool render_function(const IR &ir, const FunctionDef &fn,
                                       const nlohmann::json &input,
                                       std::string &output, std::string &error,
                                       std::vector<RenderMapping> *tracking = nullptr);

    [[nodiscard]] std::string renderTracked(const IR &ir, const std::string &functionName,
                                            const nlohmann::json &input,
                                            std::vector<RenderMapping> &mappings);

    [[nodiscard]] std::string renderTracked(const IR &ir, const std::string &functionName,
                                            const std::vector<std::string> &signature,
                                            const nlohmann::json &input,
                                            std::vector<RenderMapping> &mappings);

    namespace detail
    {
        template <typename T>
        struct is_std_vector : std::false_type {};

        template <typename T, typename Alloc>
        struct is_std_vector<std::vector<T, Alloc>> : std::true_type {};

        template <typename T>
        struct is_std_optional : std::false_type {};

        template <typename T>
        struct is_std_optional<std::optional<T>> : std::true_type {};

        template <typename T>
        std::optional<std::string> typed_signature_name()
        {
            using D = std::decay_t<T>;

            if constexpr (std::is_same_v<D, std::string> ||
                          std::is_same_v<D, const char *> ||
                          std::is_same_v<D, char *>)
            {
                return std::string{"string"};
            }
            else if constexpr (std::is_same_v<D, bool>)
            {
                return std::string{"bool"};
            }
            else if constexpr (std::is_arithmetic_v<D> || std::is_enum_v<D>)
            {
                return std::string{"int"};
            }
            else if constexpr (is_std_vector<D>::value)
            {
                using E = typename D::value_type;
                auto inner = typed_signature_name<E>();
                if (!inner.has_value())
                    return std::nullopt;
                return std::string{"list<"} + *inner + ">";
            }
            else if constexpr (is_std_optional<D>::value)
            {
                using E = typename D::value_type;
                auto inner = typed_signature_name<E>();
                if (!inner.has_value())
                    return std::nullopt;
                return std::string{"optional<"} + *inner + ">";
            }
            else
            {
                return std::nullopt;
            }
        }
    }

    template <typename... Args>
    using TypedRenderFunction = std::function<std::string(typename ArgType<Args>::type...)>;

    // Typed lookup: returns a callable that marshals typed args to JSON and
    // executes dynamic rendering through render_function().
    template <typename... Args>
    [[nodiscard]] bool get_function(const IR &ir, const std::string &name,
                                    TypedRenderFunction<Args...> &out,
                                    std::string &error)
    {
        std::vector<std::string> signature;
        signature.reserve(sizeof...(Args));
        bool unsupportedType = false;

        auto pushSig = [&](const std::optional<std::string> &sig)
        {
            if (!sig.has_value())
            {
                unsupportedType = true;
                return;
            }
            signature.push_back(*sig);
        };

        (pushSig(detail::typed_signature_name<Args>()), ...);

        if (unsupportedType)
        {
            error = "Unable to deduce runtime signature for typed lookup of function '" + name + "'";
            return false;
        }

        const FunctionDef *fn = nullptr;
        if (!get_function(ir, name, signature, fn, error))
            return false;

        if (fn->params.size() != sizeof...(Args))
        {
            error = "Expected " + std::to_string(fn->params.size()) +
                    " argument" + (fn->params.size() == 1 ? "" : "s") +
                    " for function '" + name + "', got " +
                    std::to_string(sizeof...(Args));
            return false;
        }

        out = [&ir, fn, name](typename ArgType<Args>::type... args) -> std::string {
            nlohmann::json input = nlohmann::json::array({nlohmann::json(args)...});
            std::string output;
            std::string renderError;
            if (!render_function(ir, *fn, input, output, renderError))
                throw std::runtime_error("render_function('" + name + "') failed: " + renderError);
            return output;
        };
        return true;
    }

} // namespace tpp
