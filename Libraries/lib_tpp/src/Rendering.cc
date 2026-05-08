#include <tpp/IR.h>
#include <tpp/Runtime.h>
#include <tpp/RenderMapping.h>
#include <tpp/Writer.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <functional>
#include <map>
#include <regex>
#include <string>
#include <utility>
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

    struct BodyIndentInfo
    {
        std::optional<int> blockIndent;
        size_t firstIndex = 0;
        size_t pastLastIndex = 0;
    };

    static BodyIndentInfo analyze_indented_body(const std::vector<Instruction> &body)
    {
        BodyIndentInfo info;
        info.pastLastIndex = body.size();

        if (body.size() >= 2 &&
            std::holds_alternative<PushIndentInstr>(body.front().value) &&
            std::holds_alternative<Instruction_PopIndent>(body.back().value))
        {
            info.blockIndent = std::get<PushIndentInstr>(body.front().value).amount;
            info.firstIndex = 1;
            info.pastLastIndex = body.size() - 1;
        }

        return info;
    }

    static std::vector<std::vector<const Instruction *>> split_cells(const std::vector<Instruction> &body)
    {
        std::vector<std::vector<const Instruction *>> cells;
        cells.emplace_back();

        const auto indentInfo = analyze_indented_body(body);
        for (size_t index = indentInfo.firstIndex; index < indentInfo.pastLastIndex; ++index)
        {
            const auto &instr = body[index];
            if (std::holds_alternative<Instruction_AlignCell>(instr.value))
                cells.emplace_back();
            else
                cells.back().push_back(&instr);
        }

        return cells;
    }

    static std::string json_scalar_to_string(const nlohmann::json &value)
    {
        if (value.is_null())
            return "";
        if (value.is_string())
            return value.get<std::string>();
        if (value.is_boolean())
            return value.get<bool>() ? "true" : "false";
        if (value.is_number_integer())
            return std::to_string(value.get<long long>());
        if (value.is_number_unsigned())
            return std::to_string(value.get<unsigned long long>());
        return value.dump();
    }

    static bool extract_enum_case(const nlohmann::json &value,
                                  std::string &tag,
                                  const nlohmann::json *&payload)
    {
        payload = nullptr;

        if (value.is_string())
        {
            tag = value.get<std::string>();
            return true;
        }

        if (!value.is_object())
            return false;

        if (value.contains("tag") && value["tag"].is_string())
        {
            tag = value["tag"].get<std::string>();
            if (value.contains("value") && !value["value"].is_null())
                payload = &value["value"];
            return true;
        }

        if (value.size() != 1)
            return false;

        const auto item = value.begin();
        tag = item.key();
        if (!item.value().is_null())
            payload = &item.value();
        return true;
    }

    static std::map<std::string, TppPolicy> compile_runtime_policies(const IR &ir)
    {
        std::map<std::string, TppPolicy> result;
        for (const auto &def : ir.policies)
        {
            TppPolicy policy;
            policy.tag = def.tag;
            if (def.length.has_value())
            {
                policy.minLength = def.length->min;
                policy.maxLength = def.length->max;
            }
            if (def.rejectIf.has_value())
            {
                policy.rejectIf = TppPolicy::RejectRule{
                    std::regex(def.rejectIf->regex),
                    def.rejectIf->message};
            }
            for (const auto &step : def.require)
            {
                TppPolicy::RequireStep requireStep;
                requireStep.pattern = std::regex(step.regex);
                if (!step.replace.empty())
                    requireStep.replace = precompile_replace(step.replace);
                policy.require.push_back(std::move(requireStep));
            }
            for (const auto &replacement : def.replacements)
                policy.replacements.emplace_back(replacement.find, replacement.replace);
            for (const auto &filter : def.outputFilter)
                policy.outputFilter.push_back(std::regex(filter.regex));

            result.emplace(def.tag, std::move(policy));
        }
        return result;
    }

    class IRInterpreter
    {
    public:
        explicit IRInterpreter(const IR &ir)
            : ir_(ir), writer_(compile_runtime_policies(ir))
        {
        }

        bool render(const FunctionDef &function, std::map<std::string, nlohmann::json> bindings)
        {
            scopes_.push_back(std::move(bindings));

            const bool hasPolicy = !function.policy.empty();
            if (hasPolicy && !writer_.pushPolicy(function.policy))
            {
                scopes_.pop_back();
                return false;
            }

            const bool ok = exec_body(*function.body);

            if (hasPolicy && !writer_.popPolicy())
            {
                scopes_.pop_back();
                return false;
            }

            scopes_.pop_back();
            return ok;
        }

        std::string take_output()
        {
            return writer_.takeOutput();
        }

        const std::string &error() const
        {
            return writer_.error();
        }

    private:
        const nlohmann::json *resolve_binding(const std::string &name) const
        {
            for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
            {
                const auto found = it->find(name);
                if (found != it->end())
                    return &found->second;
            }
            return nullptr;
        }

        const nlohmann::json *resolve_expr(const ExprInfo &expr) const
        {
            const nlohmann::json *value = resolve_binding(expr.bindingName);
            if (!value || value->is_null())
                return nullptr;

            for (const auto &segment : expr.segments)
            {
                if (!value->is_object())
                    return nullptr;
                const auto it = value->find(segment.field);
                if (it == value->end() || it->is_null())
                    return nullptr;
                value = &*it;
            }

            return value->is_null() ? nullptr : value;
        }

        bool expr_truthy(const ExprInfo &expr) const
        {
            const nlohmann::json *value = resolve_expr(expr);
            if (!value)
                return false;
            if (value->is_boolean())
                return value->get<bool>();
            return !value->is_null();
        }

        bool exec_body(const std::vector<Instruction> &body)
        {
            for (const auto &instr : body)
            {
                if (!exec_instruction(instr))
                    return false;
            }
            return true;
        }

        bool exec_instruction(const Instruction &instr)
        {
            return std::visit([&](auto &&arg) -> bool
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, EmitInstr>)
                {
                    return writer_.emit(arg.text);
                }
                else if constexpr (std::is_same_v<T, EmitExprInstr>)
                {
                    const nlohmann::json *value = resolve_expr(arg.expr);
                    if (!value)
                        return true;
                    return writer_.emitValue(json_scalar_to_string(*value), arg.policy);
                }
                else if constexpr (std::is_same_v<T, Instruction_AlignCell>)
                {
                    return writer_.emit("");
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<ForInstr>>)
                {
                    return exec_for(*arg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<IfInstr>>)
                {
                    return exec_if(*arg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<SwitchInstr>>)
                {
                    return exec_switch(*arg);
                }
                else if constexpr (std::is_same_v<T, CallInstr>)
                {
                    return exec_call(arg);
                }
                else if constexpr (std::is_same_v<T, PushIndentInstr>)
                {
                    return writer_.pushIndent(arg.amount);
                }
                else if constexpr (std::is_same_v<T, Instruction_PopIndent>)
                {
                    return writer_.popIndent();
                }
            }, instr.value);
        }

        bool exec_for(const ForInstr &instr)
        {
            const bool hasPolicy = !instr.policy.empty();
            if (hasPolicy && !writer_.pushPolicy(instr.policy))
                return false;

            const nlohmann::json *collection = resolve_expr(instr.collection);
            if (!collection || !collection->is_array())
            {
                if (hasPolicy)
                    return writer_.popPolicy();
                return true;
            }

            const bool ok = instr.alignSpec.has_value()
                ? exec_for_aligned(instr, *collection)
                : exec_for_normal(instr, *collection);

            if (hasPolicy && !writer_.popPolicy())
                return false;
            return ok;
        }

        bool exec_for_normal(const ForInstr &instr, const nlohmann::json &collection)
        {
            const bool bodyIsIndented = analyze_indented_body(*instr.body).blockIndent.has_value();
            const auto &items = collection;

            for (size_t index = 0; index < items.size(); ++index)
            {
                std::map<std::string, nlohmann::json> bindings;
                bindings.emplace(instr.varName, items[index]);
                if (instr.enumeratorName.has_value())
                    bindings.emplace(*instr.enumeratorName, static_cast<int>(index));

                scopes_.push_back(std::move(bindings));
                writer_.beginCapture();
                const bool ok = exec_body(*instr.body);
                std::string iterResult = writer_.endCapture();
                scopes_.pop_back();
                if (!ok)
                    return false;

                bool strippedNl = false;
                if (bodyIsIndented && instr.sep.has_value() && !instr.sep->empty())
                    strippedNl = Writer::stripSingleTrailingNewline(iterResult);

                if (instr.precededBy.has_value())
                    writer_.emit(*instr.precededBy);

                writer_.emit(iterResult);

                if (index + 1 < items.size())
                {
                    if (instr.sep.has_value())
                        writer_.emit(*instr.sep);
                }
                else
                {
                    if (instr.followedBy.has_value() && !items.empty())
                        writer_.emit(*instr.followedBy);
                    if (strippedNl)
                        writer_.emit("\n");
                }
            }

            return true;
        }

        bool exec_for_aligned(const ForInstr &instr, const nlohmann::json &collection)
        {
            const auto cells = split_cells(*instr.body);
            std::vector<std::vector<std::string>> rows;
            rows.reserve(collection.size());

            for (size_t index = 0; index < collection.size(); ++index)
            {
                std::map<std::string, nlohmann::json> bindings;
                bindings.emplace(instr.varName, collection[index]);
                if (instr.enumeratorName.has_value())
                    bindings.emplace(*instr.enumeratorName, static_cast<int>(index));

                scopes_.push_back(std::move(bindings));

                std::vector<std::string> row;
                row.reserve(cells.size());
                for (const auto &cell : cells)
                {
                    writer_.beginCapture();
                    bool ok = true;
                    for (const auto *cellInstr : cell)
                    {
                        if (!exec_instruction(*cellInstr))
                        {
                            ok = false;
                            break;
                        }
                    }
                    std::string cellText = writer_.endCapture();
                    if (!ok)
                    {
                        scopes_.pop_back();
                        return false;
                    }
                    if (Writer::containsLineBreak(cellText))
                    {
                        writer_.error() = "aligned cells must be single-line";
                        scopes_.pop_back();
                        return false;
                    }
                    row.push_back(std::move(cellText));
                }

                scopes_.pop_back();
                rows.push_back(std::move(row));
            }

            size_t numCols = 0;
            for (const auto &row : rows)
                numCols = std::max(numCols, row.size());

            std::vector<char> colSpec(numCols, 'l');
            if (instr.alignSpec.has_value() && !instr.alignSpec->empty())
            {
                const std::string &spec = *instr.alignSpec;
                if (spec.size() == 1)
                {
                    std::fill(colSpec.begin(), colSpec.end(), spec[0]);
                }
                else
                {
                    for (size_t column = 0; column < colSpec.size() && column < spec.size(); ++column)
                        colSpec[column] = spec[column];
                }
            }

            std::vector<int> colWidth(numCols, 0);
            for (const auto &row : rows)
                for (size_t column = 0; column < row.size(); ++column)
                    colWidth[column] = std::max(colWidth[column], static_cast<int>(row[column].size()));

            for (size_t index = 0; index < rows.size(); ++index)
            {
                const std::string iterResult = Writer::renderAlignedRow(rows[index], colWidth, colSpec, numCols);
                if (instr.precededBy.has_value())
                    writer_.emit(*instr.precededBy);
                writer_.emit(iterResult);
                if (index + 1 < rows.size())
                {
                    if (instr.sep.has_value())
                        writer_.emit(*instr.sep);
                }
                else if (instr.followedBy.has_value())
                {
                    writer_.emit(*instr.followedBy);
                }
            }

            return true;
        }

        bool exec_if(const IfInstr &instr)
        {
            bool cond = expr_truthy(instr.condExpr);
            if (instr.negated)
                cond = !cond;

            const auto &branch = cond ? *instr.thenBody : *instr.elseBody;
            return exec_body(branch);
        }

        bool exec_switch(const SwitchInstr &instr)
        {
            const bool hasPolicy = !instr.policy.empty();
            if (hasPolicy && !writer_.pushPolicy(instr.policy))
                return false;

            const nlohmann::json *value = resolve_expr(instr.expr);
            if (!value)
            {
                if (hasPolicy)
                    return writer_.popPolicy();
                return true;
            }

            std::string tag;
            const nlohmann::json *payload = nullptr;
            if (!extract_enum_case(*value, tag, payload))
            {
                if (hasPolicy)
                    return writer_.popPolicy();
                return true;
            }

            for (const auto &caseInstr : *instr.cases)
            {
                if (caseInstr.tag != tag)
                    continue;

                std::map<std::string, nlohmann::json> bindings;
                if (caseInstr.bindingName.has_value() && payload)
                    bindings.emplace(caseInstr.bindingName->name, *payload);

                scopes_.push_back(std::move(bindings));
                const bool ok = exec_body(*caseInstr.body);
                scopes_.pop_back();

                if (hasPolicy && !writer_.popPolicy())
                    return false;
                return ok;
            }

            if (hasPolicy && !writer_.popPolicy())
                return false;
            return true;
        }

        bool exec_call(const CallInstr &instr)
        {
            if (instr.functionIndex < 0 ||
                instr.functionIndex >= static_cast<int>(ir_.functions.size()))
            {
                writer_.error() = "render call: invalid function index for '" + instr.functionName + "'";
                return false;
            }

            const FunctionDef &callee = ir_.functions[static_cast<size_t>(instr.functionIndex)];
            std::map<std::string, nlohmann::json> bindings;
            for (size_t index = 0; index < instr.arguments.size() && index < callee.params.size(); ++index)
            {
                const nlohmann::json *value = resolve_expr(instr.arguments[index]);
                bindings.emplace(callee.params[index].name, value ? *value : nlohmann::json());
            }

            scopes_.push_back(std::move(bindings));
            writer_.beginCapture();

            const bool hasPolicy = !callee.policy.empty();
            if (hasPolicy && !writer_.pushPolicy(callee.policy))
            {
                writer_.endCapture();
                scopes_.pop_back();
                return false;
            }

            const bool ok = exec_body(*callee.body);
            std::string callResult = writer_.endCapture();

            if (hasPolicy && !writer_.popPolicy())
            {
                scopes_.pop_back();
                return false;
            }

            scopes_.pop_back();
            if (!ok)
                return false;

            if (!callResult.empty() && callResult.back() == '\n')
                callResult.pop_back();
            writer_.emit(callResult);
            return true;
        }

        const IR &ir_;
        Writer writer_;
        std::vector<std::map<std::string, nlohmann::json>> scopes_;
    };

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
    // Direct IR rendering
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

            std::map<std::string, nlohmann::json> bindings;
            if (boundArgs.is_object())
            {
                for (auto it = boundArgs.begin(); it != boundArgs.end(); ++it)
                    bindings.emplace(it.key(), it.value());
            }

            IRInterpreter interpreter(ir);
            if (!interpreter.render(function, std::move(bindings)))
            {
                error = interpreter.error();
                return false;
            }

            std::string result = interpreter.take_output();
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
