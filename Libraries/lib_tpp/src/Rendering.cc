#include <tpp/IR.h>
#include <tpp/Runtime.h>
#include <tpp/RenderMapping.h>
#include <tpp/VM.h>
#include <tpp/Layout.h>
#include <tpp/DataLoader.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>

namespace tpp
{

    // ═══════════════════════════════════════════════════════════════════
    // Helpers
    // ═══════════════════════════════════════════════════════════════════

    static bool isOptionalType(const TypeKind &tk)
    {
        return tk.value.index() == 5; // Optional variant
    }

    static std::string type_signature_name(const TypeKind &tk)
    {
        switch (tk.value.index())
        {
        case 0: return "string";
        case 1: return "int";
        case 2: return "bool";
        case 3: return std::get<3>(tk.value);
        case 4:
        {
            const auto &inner = std::get<4>(tk.value);
            return inner ? ("list<" + type_signature_name(*inner) + ">") : "list<?>";
        }
        case 5:
        {
            const auto &inner = std::get<5>(tk.value);
            return inner ? ("optional<" + type_signature_name(*inner) + ">") : "optional<?>";
        }
        default:
            return "?";
        }
    }

    static std::string function_signature_text(const FunctionDef &f)
    {
        std::string text = f.name + "(";
        for (std::size_t i = 0; i < f.params.size(); ++i)
        {
            if (i > 0)
                text += ", ";
            text += type_signature_name(*f.params[i].type);
        }
        text += ")";
        return text;
    }

    static bool function_matches_signature(const FunctionDef &f,
                                           const std::vector<std::string> &signature)
    {
        if (f.params.size() != signature.size())
            return false;
        for (std::size_t i = 0; i < signature.size(); ++i)
        {
            if (type_signature_name(*f.params[i].type) != signature[i])
                return false;
        }
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // VM-based rendering
    // ═══════════════════════════════════════════════════════════════════

    bool render_function(const IR &ir, const FunctionDef &function,
                         const nlohmann::json &input,
                         std::string &output, std::string &error,
                         std::vector<RenderMapping> *tracking)
    {
        (void)tracking; // TODO: re-enable render mapping tracking

        try
        {
            // ── Bind JSON input to function parameters ──
            nlohmann::json boundArgs;
            if (function.params.empty())
            {
                boundArgs = input;
            }
            else if (function.params.size() == 1)
            {
                const auto &p = function.params[0];
                bool isList = (p.type->value.index() == 4); // List variant
                nlohmann::json value;
                if (isList)
                {
                    if (input.is_array() && input.size() == 1 && input[0].is_array())
                        value = input[0];
                    else if (input.is_object() && input.contains(p.name))
                        value = input[p.name];
                    else
                        value = input;
                }
                else
                {
                    if (input.is_array())
                    {
                        if (input.size() == 1)
                            value = input[0];
                        else
                        {
                            error = "Expected 1 argument for function '" + function.name +
                                    "', got " + std::to_string(input.size());
                            return false;
                        }
                    }
                    else if (input.is_object() && input.contains(p.name))
                        value = input[p.name];
                    else
                        value = input;
                }
                boundArgs = nlohmann::json::object();
                boundArgs[p.name] = value;
            }
            else
            {
                if (!input.is_array() || input.size() != function.params.size())
                {
                    const std::size_t got = input.is_array() ? input.size() : 0;
                    const std::size_t expected = function.params.size();
                    error = "Expected " + std::to_string(expected) +
                            " argument" + (expected == 1 ? "" : "s") +
                            " for function '" + function.name +
                            "', got " + std::to_string(got);
                    return false;
                }
                boundArgs = nlohmann::json::object();
                for (std::size_t i = 0; i < function.params.size(); ++i)
                    boundArgs[function.params[i].name] = input[i];
            }

            // ── Validate required struct fields ──
            std::function<const StructDef *(const TypeKind &)> resolveStruct =
                [&](const TypeKind &tk) -> const StructDef *
            {
                if (tk.value.index() == 3)
                {
                    const auto &name = std::get<3>(tk.value);
                    for (const auto &s : ir.structs)
                        if (s.name == name) return &s;
                    return nullptr;
                }
                if (tk.value.index() == 5)
                {
                    const auto &inner = std::get<5>(tk.value);
                    if (inner) return resolveStruct(*inner);
                }
                return nullptr;
            };

            for (const auto &p : function.params)
            {
                const StructDef *sd = resolveStruct(*p.type);
                if (!sd)
                    continue;
                if (!boundArgs.contains(p.name) || !boundArgs[p.name].is_object())
                {
                    error = "Missing field '" + sd->fields[0].name +
                            "' in input data for function '" + function.name + "'";
                    return false;
                }
                const auto &obj = boundArgs[p.name];
                for (const auto &fd : sd->fields)
                {
                    if (isOptionalType(*fd.type))
                        continue;
                    if (!obj.contains(fd.name) || obj[fd.name].is_null())
                    {
                        error = "Missing field '" + fd.name +
                                "' in input data for function '" + function.name + "'";
                        return false;
                    }
                }
            }

            // ── Build layout table and load data into slots ──
            LayoutTable layouts = LayoutTable::build(ir);
            DataLoader loader(layouts);

            std::vector<Slot> frame(static_cast<size_t>(function.frameSlotCount));
            for (const auto &p : function.params)
            {
                if (!boundArgs.contains(p.name))
                    continue;
                loader.load_field(frame, p.slotOffset, *p.type, false, boundArgs[p.name]);
            }

            // ── Create VM and execute ──
            auto policies = VM::compilePolicies(ir);
            VM vm(layouts, std::move(policies));
            vm.setFunctions(&ir.functions);
            vm.pushFrame(frame);

            if (!function.policy.empty())
                vm.pushPolicy(function.policy);

            bool ok = vm.execBody(*function.body);

            if (!function.policy.empty())
                vm.popPolicy();

            vm.popFrame();

            if (!ok)
            {
                error = vm.error();
                return false;
            }

            std::string result = vm.takeOutput();
            if (!result.empty() && result.back() == '\n')
                result.pop_back();
            output = std::move(result);
            return true;
        }
        catch (const std::exception &e)
        {
            error = e.what();
            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Function lookup
    // ═══════════════════════════════════════════════════════════════════

    bool get_function(const IR &ir, const std::string &functionName,
                      const FunctionDef *&function, std::string &error)
    {
        const FunctionDef *match = nullptr;
        int matchCount = 0;

        for (const auto &f : ir.functions)
        {
            if (f.name == functionName)
            {
                match = &f;
                ++matchCount;
            }
        }

        if (matchCount == 1)
        {
            function = match;
            return true;
        }

        if (matchCount > 1)
        {
            error = "function '" + functionName + "' is ambiguous; matching overloads: ";
            bool first = true;
            for (const auto &f : ir.functions)
            {
                if (f.name != functionName)
                    continue;
                if (!first)
                    error += ", ";
                first = false;
                error += function_signature_text(f);
            }
            return false;
        }

        error = "function '" + functionName + "' not found";
        return false;
    }

    bool get_function(const IR &ir, const std::string &functionName,
                      const std::vector<std::string> &signature,
                      const FunctionDef *&function, std::string &error)
    {
        if (signature.empty())
            return get_function(ir, functionName, function, error);

        const FunctionDef *match = nullptr;
        int matchCount = 0;

        for (const auto &f : ir.functions)
        {
            if (f.name != functionName)
                continue;
            if (!function_matches_signature(f, signature))
                continue;
            match = &f;
            ++matchCount;
        }

        if (matchCount == 1)
        {
            function = match;
            return true;
        }

        std::string sigText = functionName + "(";
        for (std::size_t i = 0; i < signature.size(); ++i)
        {
            if (i > 0)
                sigText += ", ";
            sigText += signature[i];
        }
        sigText += ")";

        if (matchCount > 1)
        {
            error = "function signature '" + sigText + "' is ambiguous";
            return false;
        }

        error = "function '" + functionName + "' with signature '" + sigText + "' not found";
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Tracked rendering
    // ═══════════════════════════════════════════════════════════════════

    std::string renderTracked(const IR &ir, const std::string &functionName,
                              const nlohmann::json &input,
                              std::vector<RenderMapping> &mappings)
    {
        return renderTracked(ir, functionName, {}, input, mappings);
    }

    std::string renderTracked(const IR &ir, const std::string &functionName,
                              const std::vector<std::string> &signature,
                              const nlohmann::json &input,
                              std::vector<RenderMapping> &mappings)
    {
        const FunctionDef *fn = nullptr;
        std::string error;
        if (!get_function(ir, functionName, signature, fn, error))
            throw std::runtime_error(error);
        std::string output;
        mappings.clear();
        if (!render_function(ir, *fn, input, output, error, &mappings))
            throw std::runtime_error(error);
        return output;
    }

} // namespace tpp
