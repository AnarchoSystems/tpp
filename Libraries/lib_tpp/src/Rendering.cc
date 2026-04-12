#include <tpp/IR.h>
#include <tpp/Rendering.h>
#include <tpp/RenderMapping.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <regex>
#include <unordered_map>

namespace tpp
{

    // ═══════════════════════════════════════════════════════════════════
    // Compiled policy — built from PolicyDef at load time
    // ═══════════════════════════════════════════════════════════════════

    struct CompiledRegexStep
    {
        std::regex rx;
        std::string replace;
        std::string compiled_replace; // @subexpr_N@ → $N
    };

    struct CompiledPolicy
    {
        std::string tag;
        std::optional<PolicyLength> length;
        struct RejectIf { std::regex rx; std::string message; };
        std::optional<RejectIf> rejectIf;
        std::vector<CompiledRegexStep> require;
        std::vector<PolicyReplacement> replacements;
        std::vector<std::regex> outputFilter;
    };

    static std::string precompile_replace(const std::string &replace)
    {
        std::string out;
        out.reserve(replace.size());
        size_t p = 0;
        while (p < replace.size())
        {
            if (replace[p] == '@')
            {
                size_t end = replace.find('@', p + 1);
                if (end != std::string::npos)
                {
                    std::string inner = replace.substr(p + 1, end - p - 1);
                    if (inner.size() > 8 && inner.substr(0, 8) == "subexpr_")
                    {
                        out += '$';
                        out += inner.substr(8);
                        p = end + 1;
                        continue;
                    }
                }
            }
            out += replace[p];
            ++p;
        }
        return out;
    }

    static CompiledPolicy compilePolicy(const PolicyDef &def)
    {
        CompiledPolicy cp;
        cp.tag = def.tag;
        cp.length = def.length;
        if (def.rejectIf.has_value())
        {
            cp.rejectIf = CompiledPolicy::RejectIf{
                std::regex(def.rejectIf->regex),
                def.rejectIf->message};
        }
        for (const auto &r : def.require)
        {
            CompiledRegexStep step;
            step.rx = std::regex(r.regex);
            step.replace = r.replace;
            step.compiled_replace = r.compiledReplace.empty() && !r.replace.empty()
                ? precompile_replace(r.replace) : r.compiledReplace;
            cp.require.push_back(std::move(step));
        }
        cp.replacements = def.replacements;
        for (const auto &f : def.outputFilter)
            cp.outputFilter.push_back(std::regex(f.regex));
        return cp;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Render context
    // ═══════════════════════════════════════════════════════════════════

    struct RenderContext
    {
        const std::vector<FunctionDef> &functions;
        std::vector<std::pair<std::string, nlohmann::json>> bindings;
        std::string activePolicy;
        const CompiledPolicy *activePolicyPtr = nullptr;
        std::map<std::string, CompiledPolicy> compiledPolicies;
        std::unordered_map<std::string, std::vector<size_t>> functionIndex;
        std::string renderError;
        std::vector<RenderMapping> *trackingOut = nullptr;

        void pushBinding(const std::string &name, const nlohmann::json &val)
        {
            bindings.push_back({name, val});
        }
        void popBinding()
        {
            bindings.pop_back();
        }

        // Resolve a dotted path expression against the binding stack.
        nlohmann::json resolve(const std::string &path) const
        {
            auto dot = path.find('.');
            std::string head = (dot == std::string::npos) ? path : path.substr(0, dot);

            // Look up head in bindings (reverse for lexical scoping)
            nlohmann::json val;
            bool found = false;
            for (int i = (int)bindings.size() - 1; i >= 0; --i)
            {
                if (bindings[i].first == head)
                {
                    val = bindings[i].second;
                    found = true;
                    break;
                }
            }
            if (!found) return nullptr;

            // Walk remaining path
            if (dot != std::string::npos)
            {
                std::string rest = path.substr(dot + 1);
                while (!rest.empty())
                {
                    auto d = rest.find('.');
                    std::string seg = (d == std::string::npos) ? rest : rest.substr(0, d);
                    if (val.is_object() && val.contains(seg))
                        val = val[seg];
                    else
                        return nullptr;
                    rest = (d == std::string::npos) ? "" : rest.substr(d + 1);
                }
            }
            return val;
        }

        nlohmann::json resolve(const ExprInfo &expr) const
        {
            return resolve(expr.path);
        }

        std::string jsonToString(const nlohmann::json &val) const
        {
            if (val.is_string())
                return val.get<std::string>();
            if (val.is_number_integer())
                return std::to_string(val.get<int64_t>());
            if (val.is_number_float())
            {
                double d = val.get<double>();
                if (d == std::floor(d))
                    return std::to_string((int64_t)d);
                return std::to_string(d);
            }
            if (val.is_boolean())
                return val.get<bool>() ? "true" : "false";
            if (val.is_null())
                return "";
            return val.dump();
        }

        const CompiledPolicy *findPolicy(const std::string &tag)
        {
            auto it = compiledPolicies.find(tag);
            if (it != compiledPolicies.end())
                return &it->second;
            return nullptr;
        }
    };

    // Forward declarations
    static std::string renderInstructions(const std::vector<Instruction> &instrs, RenderContext &ctx);
    static std::string renderBlockBody(const std::vector<Instruction> &instrs, RenderContext &ctx, int insertCol);

    // Render instructions, splitting output at AlignCell markers.
    // Within an aligned for-loop body, each cell typically contains only
    // Emit/EmitExpr instructions. We accumulate text per cell directly
    // using the same visitor logic as renderInstructions, avoiding the
    // need to copy non-copyable Instruction values.
    static std::vector<std::string> renderInstructionsCells(const std::vector<Instruction> &instrs, RenderContext &ctx)
    {
        std::vector<std::string> cells;
        std::string current;
        for (const auto &instr : instrs)
        {
            if (std::holds_alternative<Instruction_AlignCell>(instr.value))
            {
                cells.push_back(std::move(current));
                current.clear();
            }
            else
            {
                std::visit([&](auto &&arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, EmitInstr>)
                        current += arg.text;
                    else if constexpr (std::is_same_v<T, EmitExprInstr>)
                    {
                        if (!ctx.renderError.empty()) return;
                        auto val = ctx.resolve(arg.expr);
                        current += ctx.jsonToString(val);
                    }
                    // Other instruction types (For, If, Switch, etc.) are not
                    // expected inside aligned cell bodies — they are handled by
                    // the full renderInstructions visitor.
                }, instr.value);
            }
        }
        cells.push_back(std::move(current));
        return cells;
    }

    static std::string padCell(const std::string &s, int width, char spec)
    {
        int len = (int)s.size();
        if (len >= width) return s;
        int pad = width - len;
        if (spec == 'r')
            return std::string(pad, ' ') + s;
        if (spec == 'c')
        {
            int left = pad / 2, right = pad - left;
            return std::string(left, ' ') + s + std::string(right, ' ');
        }
        return s + std::string(pad, ' ');
    }

    // ═══════════════════════════════════════════════════════════════════
    // Policy pipeline
    // ═══════════════════════════════════════════════════════════════════

    static bool applyPolicy(const std::string &tag, const CompiledPolicy &pol, std::string &value, std::string &outError)
    {
        // 1. Length check
        if (pol.length.has_value())
        {
            int len = (int)value.size();
            if (pol.length->min.has_value() && len < *pol.length->min)
            {
                outError = "[policy " + tag + "] value is below minimum length of " + std::to_string(*pol.length->min);
                return false;
            }
            if (pol.length->max.has_value() && len > *pol.length->max)
            {
                outError = "[policy " + tag + "] value exceeds maximum length of " + std::to_string(*pol.length->max);
                return false;
            }
        }

        // 2. reject-if
        if (pol.rejectIf.has_value())
        {
            if (std::regex_search(value, pol.rejectIf->rx))
            {
                outError = "[policy " + tag + "] " + pol.rejectIf->message;
                return false;
            }
        }

        // 3. require[]
        for (auto &step : pol.require)
        {
            std::smatch m;
            if (!std::regex_search(value, m, step.rx))
            {
                outError = "[policy " + tag + "] value does not match required pattern";
                return false;
            }
            if (!step.replace.empty())
            {
                value = std::regex_replace(value, step.rx, step.compiled_replace);
            }
        }

        // 4. replacements[]
        for (auto &rep : pol.replacements)
        {
            if (rep.find.empty()) continue;
            std::string out;
            size_t pos = 0;
            while (true)
            {
                size_t found = value.find(rep.find, pos);
                if (found == std::string::npos)
                {
                    out += value.substr(pos);
                    break;
                }
                out += value.substr(pos, found - pos);
                out += rep.replace;
                pos = found + rep.find.size();
            }
            value = std::move(out);
        }

        // 5. output-filter[]
        for (auto &rx : pol.outputFilter)
        {
            if (!std::regex_match(value, rx))
            {
                outError = "[policy " + tag + "] output does not match required filter";
                return false;
            }
        }

        return true;
    }

    static std::string trim(const std::string &s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static bool isOptionalType(const TypeKind &tk)
    {
        return tk.value.index() == 5; // Optional variant
    }

    // ═══════════════════════════════════════════════════════════════════
    // Instruction rendering
    // ═══════════════════════════════════════════════════════════════════

    static std::string renderInstructions(const std::vector<Instruction> &instrs, RenderContext &ctx)
    {
        std::string result;
        for (const auto &instr : instrs)
        {
            std::visit([&](auto &&arg)
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, EmitInstr>)
                {
                    result += arg.text;
                }
                else if constexpr (std::is_same_v<T, EmitExprInstr>)
                {
                    if (!ctx.renderError.empty()) return;
                    auto val = ctx.resolve(arg.expr);
                    auto str = ctx.jsonToString(val);
                    // Apply policy
                    const std::string &effPol = arg.policy.empty() ? ctx.activePolicy : arg.policy;
                    if (effPol != "none" && !effPol.empty())
                    {
                        const CompiledPolicy *pol = arg.policy.empty()
                            ? ctx.activePolicyPtr
                            : ctx.findPolicy(effPol);
                        if (pol && !applyPolicy(effPol, *pol, str, ctx.renderError))
                            return;
                    }
                    // Multi-line indent
                    auto lastNl = result.rfind('\n');
                    size_t col = (lastNl == std::string::npos) ? result.size() : result.size() - lastNl - 1;
                    if (col > 0 && str.find('\n') != std::string::npos)
                    {
                        std::string pad(col, ' ');
                        std::string out;
                        size_t start = 0;
                        while (true)
                        {
                            auto end = str.find('\n', start);
                            if (start > 0) out += pad;
                            out += str.substr(start, end == std::string::npos ? end : end - start);
                            if (end == std::string::npos) break;
                            out += '\n';
                            start = end + 1;
                        }
                        result += out;
                    }
                    else
                    {
                        result += str;
                    }
                }
                else if constexpr (std::is_same_v<T, Instruction_AlignCell>)
                {
                    // No output — only meaningful via renderInstructionsCells
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<ForInstr>>)
                {
                    auto collection = ctx.resolve(arg->collection);
                    if (collection.is_array())
                    {
                        std::string savedPolicy = ctx.activePolicy;
                        const CompiledPolicy *savedPolicyPtr = ctx.activePolicyPtr;
                        if (!arg->policy.empty())
                        {
                            ctx.activePolicy = arg->policy;
                            ctx.activePolicyPtr = ctx.findPolicy(arg->policy);
                        }

                        if (arg->hasAlign)
                        {
                            // ── Alignment rendering ──
                            std::vector<std::vector<std::string>> rows;
                            rows.reserve(collection.size());
                            for (size_t i = 0; i < collection.size(); ++i)
                            {
                                ctx.pushBinding(arg->varName, collection[i]);
                                if (!arg->enumeratorName.empty())
                                    ctx.pushBinding(arg->enumeratorName, (int64_t)i);
                                rows.push_back(renderInstructionsCells(*arg->body, ctx));
                                if (!arg->enumeratorName.empty())
                                    ctx.popBinding();
                                ctx.popBinding();
                            }
                            size_t numCols = 0;
                            for (const auto &row : rows)
                                numCols = std::max(numCols, row.size());
                            std::vector<char> colSpec(numCols, 'l');
                            if (!arg->alignSpec.empty())
                            {
                                const std::string &s = arg->alignSpec;
                                if (s.size() == 1)
                                    std::fill(colSpec.begin(), colSpec.end(), s[0]);
                                else
                                    for (size_t ci = 0; ci < colSpec.size() && ci < s.size(); ++ci)
                                        colSpec[ci] = s[ci];
                            }
                            std::vector<int> colWidth(numCols, 0);
                            for (const auto &row : rows)
                                for (size_t ci = 0; ci < row.size(); ++ci)
                                    colWidth[ci] = std::max(colWidth[ci], (int)row[ci].size());
                            for (size_t i = 0; i < rows.size(); ++i)
                            {
                                const auto &row = rows[i];
                                std::string iterResult;
                                for (size_t ci = 0; ci < row.size(); ++ci)
                                {
                                    if (ci + 1 < numCols)
                                        iterResult += padCell(row[ci], colWidth[ci], colSpec[ci]);
                                    else
                                    {
                                        char spec = colSpec[ci];
                                        int pad = colWidth[ci] - (int)row[ci].size();
                                        if (pad > 0 && spec != 'l')
                                        {
                                            int left = (spec == 'c') ? pad / 2 : pad;
                                            iterResult += std::string(left, ' ');
                                        }
                                        iterResult += row[ci];
                                    }
                                }
                                result += arg->precededBy;
                                result += iterResult;
                                if (i + 1 < rows.size())
                                    result += arg->sep;
                                else if (!arg->followedBy.empty())
                                    result += arg->followedBy;
                            }
                        }
                        else
                        {
                            for (size_t i = 0; i < collection.size(); ++i)
                            {
                                ctx.pushBinding(arg->varName, collection[i]);
                                if (!arg->enumeratorName.empty())
                                    ctx.pushBinding(arg->enumeratorName, (int64_t)i);
                                std::string iterResult;
                                if (arg->isBlock)
                                    iterResult = renderBlockBody(*arg->body, ctx, arg->insertCol);
                                else
                                    iterResult = renderInstructions(*arg->body, ctx);
                                if (!arg->enumeratorName.empty())
                                    ctx.popBinding();
                                ctx.popBinding();
                                bool strippedNl = false;
                                if (arg->isBlock && !arg->sep.empty() &&
                                    !iterResult.empty() && iterResult.back() == '\n')
                                {
                                    auto nlCount = std::count(iterResult.begin(), iterResult.end(), '\n');
                                    if (nlCount == 1)
                                    {
                                        iterResult.pop_back();
                                        strippedNl = true;
                                    }
                                }
                                result += arg->precededBy;
                                result += iterResult;
                                if (i + 1 < collection.size())
                                    result += arg->sep;
                                else
                                {
                                    if (!arg->followedBy.empty() && !collection.empty())
                                        result += arg->followedBy;
                                    if (strippedNl)
                                        result += "\n";
                                }
                            }
                        }
                        ctx.activePolicy = savedPolicy;
                        ctx.activePolicyPtr = savedPolicyPtr;
                    }
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<IfInstr>>)
                {
                    auto val = ctx.resolve(arg->condExpr);
                    bool cond = false;
                    if (arg->negated)
                        cond = val.is_boolean() ? !val.template get<bool>() : val.is_null();
                    else if (val.is_boolean())
                        cond = val.template get<bool>();
                    else
                        cond = !val.is_null();

                    if (cond)
                    {
                        if (arg->isBlock)
                            result += renderBlockBody(*arg->thenBody, ctx, arg->insertCol);
                        else
                            result += renderInstructions(*arg->thenBody, ctx);
                    }
                    else
                    {
                        if (arg->isBlock)
                            result += renderBlockBody(*arg->elseBody, ctx, arg->insertCol);
                        else
                            result += renderInstructions(*arg->elseBody, ctx);
                    }
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<SwitchInstr>>)
                {
                    auto val = ctx.resolve(arg->expr);
                    if (val.is_object())
                    {
                        std::string savedPolicy = ctx.activePolicy;
                        const CompiledPolicy *savedPolicyPtr = ctx.activePolicyPtr;
                        if (!arg->policy.empty())
                        {
                            ctx.activePolicy = arg->policy;
                            ctx.activePolicyPtr = ctx.findPolicy(arg->policy);
                        }
                        for (const auto &c : *arg->cases)
                        {
                            if (val.contains(c.tag))
                            {
                                if (!c.bindingName.empty())
                                    ctx.pushBinding(c.bindingName, val[c.tag]);
                                if (arg->isBlock)
                                    result += renderBlockBody(*c.body, ctx, arg->insertCol);
                                else
                                    result += renderInstructions(*c.body, ctx);
                                if (!c.bindingName.empty())
                                    ctx.popBinding();
                                break;
                            }
                        }
                        ctx.activePolicy = savedPolicy;
                        ctx.activePolicyPtr = savedPolicyPtr;
                    }
                }
                else if constexpr (std::is_same_v<T, CallInstr>)
                {
                    const FunctionDef *targetFunc = nullptr;
                    auto fcIt = ctx.functionIndex.find(arg.functionName);
                    if (fcIt != ctx.functionIndex.end() && !fcIt->second.empty())
                        targetFunc = &ctx.functions[fcIt->second[0]];
                    if (targetFunc)
                    {
                        for (size_t i = 0; i < arg.arguments.size() && i < targetFunc->params.size(); ++i)
                        {
                            auto val = ctx.resolve(arg.arguments[i]);
                            ctx.pushBinding(targetFunc->params[i].name, val);
                        }
                        std::string savedPolicy = ctx.activePolicy;
                        const CompiledPolicy *savedPolicyPtr = ctx.activePolicyPtr;
                        if (!targetFunc->policy.empty())
                        {
                            ctx.activePolicy = targetFunc->policy;
                            ctx.activePolicyPtr = ctx.findPolicy(targetFunc->policy);
                        }
                        std::string callResult = renderInstructions(*targetFunc->body, ctx);
                        ctx.activePolicy = savedPolicy;
                        ctx.activePolicyPtr = savedPolicyPtr;
                        if (!callResult.empty() && callResult.back() == '\n')
                            callResult.pop_back();
                        result += callResult;
                        for (size_t i = 0; i < arg.arguments.size() && i < targetFunc->params.size(); ++i)
                            ctx.popBinding();
                    }
                }
                else if constexpr (std::is_same_v<T, RenderViaInstr>)
                {
                    auto collection = ctx.resolve(arg.collection);
                    if (collection.is_array())
                    {
                        std::string savedPolicy = ctx.activePolicy;
                        const CompiledPolicy *savedPolicyPtr = ctx.activePolicyPtr;
                        if (!arg.policy.empty())
                        {
                            ctx.activePolicy = arg.policy;
                            ctx.activePolicyPtr = ctx.findPolicy(arg.policy);
                        }
                        std::vector<const FunctionDef *> overloads;
                        auto rvIt = ctx.functionIndex.find(arg.functionName);
                        if (rvIt != ctx.functionIndex.end())
                            for (size_t idx : rvIt->second)
                                overloads.push_back(&ctx.functions[idx]);

                        for (size_t i = 0; i < collection.size(); ++i)
                        {
                            const FunctionDef *targetFunc = nullptr;
                            if (overloads.size() == 1)
                            {
                                targetFunc = overloads[0];
                            }
                            else if (overloads.size() > 1 && collection[i].is_object())
                            {
                                for (auto it = collection[i].begin(); it != collection[i].end(); ++it)
                                {
                                    const auto &payload = it.value();
                                    for (auto *ovl : overloads)
                                    {
                                        if (ovl->params.size() == 1)
                                        {
                                            const auto &ptype = *ovl->params[0].type;
                                            bool matches = false;
                                            if (ptype.value.index() == 0 && payload.is_string()) matches = true;
                                            else if (ptype.value.index() == 1 && payload.is_number()) matches = true;
                                            else if (ptype.value.index() == 2 && payload.is_boolean()) matches = true;
                                            if (matches) { targetFunc = ovl; break; }
                                        }
                                    }
                                    if (targetFunc) break;
                                }
                            }
                            if (targetFunc && !targetFunc->params.empty())
                            {
                                nlohmann::json elemVal = collection[i];
                                if (overloads.size() > 1 && elemVal.is_object())
                                {
                                    for (auto it = elemVal.begin(); it != elemVal.end(); ++it)
                                    {
                                        elemVal = it.value();
                                        break;
                                    }
                                }
                                ctx.pushBinding(targetFunc->params[0].name, elemVal);
                                std::string funcSavedPolicy = ctx.activePolicy;
                                const CompiledPolicy *funcSavedPolicyPtr = ctx.activePolicyPtr;
                                if (!targetFunc->policy.empty())
                                {
                                    ctx.activePolicy = targetFunc->policy;
                                    ctx.activePolicyPtr = ctx.findPolicy(targetFunc->policy);
                                }
                                std::string iterResult = renderInstructions(*targetFunc->body, ctx);
                                ctx.activePolicy = funcSavedPolicy;
                                ctx.activePolicyPtr = funcSavedPolicyPtr;
                                if (!iterResult.empty() && iterResult.back() == '\n')
                                    iterResult.pop_back();
                                ctx.popBinding();
                                result += arg.precededBy;
                                result += iterResult;
                                if (i + 1 < collection.size())
                                    result += arg.sep;
                                else if (!arg.followedBy.empty())
                                    result += arg.followedBy;
                            }
                        }
                        ctx.activePolicy = savedPolicy;
                        ctx.activePolicyPtr = savedPolicyPtr;
                    }
                }
            }, instr.value);
        }
        return result;
    }

    static std::string renderBlockBody(const std::vector<Instruction> &instrs, RenderContext &ctx, int insertCol)
    {
        std::string raw = renderInstructions(instrs, ctx);

        if (raw.empty()) return "";

        std::vector<std::string> lines;
        std::istringstream iss(raw);
        std::string line;
        while (std::getline(iss, line))
            lines.push_back(line);
        bool trailingNl = !raw.empty() && raw.back() == '\n';

        // Find zero-marker
        std::string zeroMarker;
        for (auto &l : lines)
        {
            if (!trim(l).empty())
            {
                size_t ws = 0;
                while (ws < l.size() && (l[ws] == ' ' || l[ws] == '\t'))
                    ws++;
                zeroMarker = l.substr(0, ws);
                break;
            }
        }

        std::string indent(insertCol, ' ');
        std::string result;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            std::string l = lines[i];
            if (!l.empty() && !zeroMarker.empty() && l.size() >= zeroMarker.size() &&
                l.compare(0, zeroMarker.size(), zeroMarker) == 0)
                l = l.substr(zeroMarker.size());
            if (!l.empty())
                result += indent + l;
            if (i + 1 < lines.size() || trailingNl)
                result += "\n";
        }

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Public rendering API
    // ═══════════════════════════════════════════════════════════════════

    bool render_function(const IR &ir, const FunctionDef &function,
                         const nlohmann::json &input,
                         std::string &output, std::string &error,
                         std::vector<RenderMapping> *tracking)
    {
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

            // ── Build render context ──
            RenderContext ctx{ir.functions};
            ctx.activePolicy = function.policy;
            for (const auto &pd : ir.policies)
                ctx.compiledPolicies[pd.tag] = compilePolicy(pd);
            if (!ctx.activePolicy.empty() && ctx.activePolicy != "none")
                ctx.activePolicyPtr = ctx.findPolicy(ctx.activePolicy);
            for (size_t fi = 0; fi < ir.functions.size(); ++fi)
                ctx.functionIndex[ir.functions[fi].name].push_back(fi);
            ctx.trackingOut = tracking;

            for (const auto &p : function.params)
                if (boundArgs.contains(p.name))
                    ctx.pushBinding(p.name, boundArgs[p.name]);

            std::string result = renderInstructions(*function.body, ctx);
            if (!ctx.renderError.empty())
            {
                error = std::move(ctx.renderError);
                return false;
            }
            if (!result.empty() && result.back() == '\n')
                result.pop_back();
            output = std::move(result);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool get_function(const IR &ir, const std::string &functionName,
                      const FunctionDef *&function, std::string &error)
    {
        for (const auto &f : ir.functions)
        {
            if (f.name == functionName)
            {
                function = &f;
                return true;
            }
        }
        error = "function '" + functionName + "' not found";
        return false;
    }

    std::string renderTracked(const IR &ir, const std::string &functionName,
                              const nlohmann::json &input,
                              std::vector<RenderMapping> &mappings)
    {
        const FunctionDef *fn = nullptr;
        std::string error;
        if (!get_function(ir, functionName, fn, error))
            throw std::runtime_error(error);
        std::string output;
        mappings.clear();
        if (!render_function(ir, *fn, input, output, error, &mappings))
            throw std::runtime_error(error);
        return output;
    }

} // namespace tpp
