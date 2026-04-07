#include <tpp/FunctionSymbol.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <regex>

namespace tpp
{

    // ═══════════════════════════════════════════════════════════════════
    // Rendering — types and forward declarations
    // ═══════════════════════════════════════════════════════════════════

    struct RenderContext
    {
        const TypeRegistry &types;
        const std::vector<TemplateFunction> &allFunctions;
        std::vector<std::pair<std::string, nlohmann::json>> bindings;
        std::string activePolicy;
        const PolicyRegistry *policyRegistry = nullptr;
        std::string renderError;

        void pushBinding(const std::string &name, const nlohmann::json &val)
        {
            bindings.push_back({name, val});
        }
        void popBinding()
        {
            bindings.pop_back();
        }

        nlohmann::json resolve(const Expression &expr) const
        {
            return std::visit([&](auto &&arg) -> nlohmann::json
                              {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Variable>) {
                for (int i = (int)bindings.size() - 1; i >= 0; --i) {
                    if (bindings[i].first == arg.name) {
                        return bindings[i].second;
                    }
                }
                return nullptr;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<FieldAccess>>) {
                auto base = resolve(arg->base);
                if (base.is_object() && base.contains(arg->field)) {
                    return base[arg->field];
                }
                return nullptr;
            }
            return nullptr; }, expr);
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
    };

    // renderNodes and renderBlockBody are mutually recursive — forward declare both
    static std::string renderNodes(const std::vector<ASTNode> &nodes, RenderContext &ctx);
    static std::string renderBlockBody(const std::vector<ASTNode> &nodes, RenderContext &ctx, int insertCol);

    // ═══════════════════════════════════════════════════════════════════
    // Policy pipeline
    // ═══════════════════════════════════════════════════════════════════

    static bool applyPolicy(const std::string &tag, const Policy &pol, std::string &value, std::string &outError)
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
            std::regex rx(pol.rejectIf->regex);
            if (std::regex_search(value, rx))
            {
                outError = "[policy " + tag + "] " + pol.rejectIf->message;
                return false;
            }
        }

        // 3. require[] — each step MUST match; if it does, apply optional replacement
        for (auto &step : pol.require)
        {
            std::regex rx(step.regex);
            std::smatch m;
            if (!std::regex_search(value, m, rx))
            {
                outError = "[policy " + tag + "] value does not match required pattern";
                return false;
            }
            if (!step.replace.empty())
            {
                // Convert @subexpr_N@ → $N for std::regex_replace
                std::string repl;
                size_t p = 0;
                while (p < step.replace.size())
                {
                    if (step.replace[p] == '@')
                    {
                        size_t end = step.replace.find('@', p + 1);
                        if (end != std::string::npos)
                        {
                            std::string inner = step.replace.substr(p + 1, end - p - 1);
                            if (inner.substr(0, 8) == "subexpr_")
                            {
                                repl += "$" + inner.substr(8);
                                p = end + 1;
                                continue;
                            }
                        }
                    }
                    repl += step.replace[p];
                    ++p;
                }
                value = std::regex_replace(value, rx, repl);
            }
        }

        // 4. replacements[] — literal find+replace (all occurrences)
        for (auto &rep : pol.replacements)
        {
            if (rep.find.empty())
                continue;
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

        // 5. output-filter[] — the final value must match every regex
        for (auto &step : pol.outputFilter)
        {
            std::regex rx(step.regex);
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

    static bool isOptionalType(const TypeRef &t)
    {
        return std::holds_alternative<std::shared_ptr<OptionalType>>(t);
    }

    // ═══════════════════════════════════════════════════════════════════
    // JSON Binding  (bind)
    // ═══════════════════════════════════════════════════════════════════

    bool FunctionSymbol::render(const nlohmann::json &input,
                               std::string &output,
                               std::string &error) const noexcept
    {
        try
        {
            // ── Bind JSON input to function parameters ──────────────────────
            nlohmann::json boundArgs;
            if (function.params.empty())
            {
                boundArgs = input;
            }
            else if (function.params.size() == 1)
            {
                const auto &p = function.params[0];
                bool isList = std::holds_alternative<std::shared_ptr<ListType>>(p.type);
                nlohmann::json value;
                if (isList)
                {
                    // list param: if input is a length-1 array whose single element is
                    // also an array, treat that inner array as the list (unary wrap).
                    // Otherwise treat input itself as the list.
                    if (input.is_array() && input.size() == 1 && input[0].is_array())
                        value = input[0];
                    else
                        value = input;
                }
                else
                {
                    // non-list param: plain value, or unwrap a length-1 array
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
                    {
                        value = input;
                    }
                }
                boundArgs = nlohmann::json::object();
                boundArgs[p.name] = value;
            }
            else
            {
                // multi-param: input must be a positional array of exactly params.size() elements
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

            // ── Validate required struct fields are present in the JSON ─────
            std::function<const StructDef *(const TypeRef &)> resolveStruct = [&](const TypeRef &tr) -> const StructDef *
            {
                if (std::holds_alternative<NamedType>(tr))
                {
                    auto &nt = std::get<NamedType>(tr);
                    auto it = types.nameIndex.find(nt.name);
                    if (it != types.nameIndex.end() && it->second.kind == TypeKind::Struct)
                        return &types.structs[it->second.index];
                    return nullptr;
                }
                if (auto spOpt = std::get_if<std::shared_ptr<OptionalType>>(&tr))
                {
                    return resolveStruct((*spOpt)->innerType);
                }
                return nullptr;
            };

            if (function.params.size() == 1)
            {
                const auto &p = function.params[0];
                const StructDef *sd = resolveStruct(p.type);
                if (sd)
                {
                    // boundArgs should contain the param name mapping to the object
                    if (!boundArgs.contains(p.name) || !boundArgs[p.name].is_object())
                    {
                        error = "Missing field '" + sd->fields[0].name + "' in input data for function '" + function.name + "'";
                        return false;
                    }
                    const auto &obj = boundArgs[p.name];
                    for (auto &fd : sd->fields)
                    {
                        if (isOptionalType(fd.type))
                            continue;
                        if (!obj.contains(fd.name) || obj[fd.name].is_null())
                        {
                            error = "Missing field '" + fd.name + "' in input data for function '" + function.name + "'";
                            return false;
                        }
                    }
                }
            }
            else if (function.params.size() > 1)
            {
                for (auto &p : function.params)
                {
                    const StructDef *sd = resolveStruct(p.type);
                    if (!sd)
                        continue;
                    if (!boundArgs.contains(p.name) || !boundArgs[p.name].is_object())
                    {
                        error = "Missing field '" + sd->fields[0].name + "' in input data for function '" + function.name + "'";
                        return false;
                    }
                    const auto &obj = boundArgs[p.name];
                    for (auto &fd : sd->fields)
                    {
                        if (isOptionalType(fd.type))
                            continue;
                        if (!obj.contains(fd.name) || obj[fd.name].is_null())
                        {
                            error = "Missing field '" + fd.name + "' in input data for function '" + function.name + "'";
                            return false;
                        }
                    }
                }
            }
            RenderContext ctx{types, allFunctions};
            ctx.activePolicy = function.policy;
            ctx.policyRegistry = &policies;
            for (auto &p : function.params)
            {
                if (boundArgs.contains(p.name))
                    ctx.pushBinding(p.name, boundArgs[p.name]);
            }
            std::string result = renderNodes(function.body, ctx);
            if (!ctx.renderError.empty())
            {
                error = std::move(ctx.renderError);
                return false;
            }
            // Strip trailing newline — the template body naturally adds one for
            // the last line, but expected outputs don't include it
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

    // ═══════════════════════════════════════════════════════════════════
    // Rendering — implementations
    // ═══════════════════════════════════════════════════════════════════

    static std::string renderNodes(const std::vector<ASTNode> &nodes, RenderContext &ctx)
    {
        std::string result;
        for (auto &node : nodes)
        {
            std::visit([&](auto &&arg)
                       {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, TextNode>) {
                result += arg.text;
            } else if constexpr (std::is_same_v<T, InterpolationNode>) {
                if (!ctx.renderError.empty()) return;
                auto val = ctx.resolve(arg.expr);
                auto str = ctx.jsonToString(val);
                // Apply policy
                std::string effPol = arg.policy.empty() ? ctx.activePolicy : arg.policy;
                if (effPol != "none" && !effPol.empty() && ctx.policyRegistry)
                {
                    const Policy *pol = ctx.policyRegistry->find(effPol);
                    if (pol && !applyPolicy(effPol, *pol, str, ctx.renderError))
                        return;
                }
                auto lastNl = result.rfind('\n');
                size_t col = (lastNl == std::string::npos) ? result.size() : result.size() - lastNl - 1;
                if (col > 0 && str.find('\n') != std::string::npos) {
                    std::string pad(col, ' ');
                    std::string out;
                    size_t start = 0;
                    while (true) {
                        auto end = str.find('\n', start);
                        if (start > 0) out += pad;
                        out += str.substr(start, end == std::string::npos ? end : end - start);
                        if (end == std::string::npos) break;
                        out += '\n';
                        start = end + 1;
                    }
                    result += out;
                } else {
                    result += str;
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>) {
                auto collection = ctx.resolve(arg->collectionExpr);
                if (collection.is_array()) {
                    std::string savedPolicy = ctx.activePolicy;
                    if (!arg->policy.empty()) ctx.activePolicy = arg->policy;
                    for (size_t i = 0; i < collection.size(); ++i) {
                        ctx.pushBinding(arg->varName, collection[i]);
                        if (!arg->iteratorVarName.empty()) {
                            ctx.pushBinding(arg->iteratorVarName, (int64_t)i);
                        }
                        std::string iterResult;
                        if (arg->isBlock) {
                            iterResult = renderBlockBody(arg->body, ctx, arg->insertCol);
                        } else {
                            iterResult = renderNodes(arg->body, ctx);
                        }
                        if (!arg->iteratorVarName.empty()) {
                            ctx.popBinding();
                        }
                        ctx.popBinding();
                        // For block for-loops with sep: if the body is a single
                        // line (trailing \n is the only newline), strip it so
                        // sep replaces the line boundary. Multi-line bodies keep
                        // their trailing \n and sep is added on top.
                        bool strippedNl = false;
                        if (arg->isBlock && !arg->sep.empty() &&
                            !iterResult.empty() && iterResult.back() == '\n') {
                            // Count newlines — single trailing \n means single-line body
                            auto nlCount = std::count(iterResult.begin(), iterResult.end(), '\n');
                            if (nlCount == 1) {
                                iterResult.pop_back();
                                strippedNl = true;
                            }
                        }
                        result += arg->precededBy;
                        result += iterResult;
                        if (i + 1 < collection.size()) {
                            result += arg->sep;
                        } else {
                            if (!arg->followedBy.empty() && !collection.empty()) {
                                result += arg->followedBy;
                            }
                            // Restore trailing newline for last iteration
                            if (strippedNl) {
                                result += "\n";
                            }
                        }
                    }
                    ctx.activePolicy = savedPolicy;
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>) {
                auto val = ctx.resolve(arg->condExpr);
                bool cond = false;
                if (arg->negated) {
                    cond = val.is_boolean() ? !val.template get<bool>() : val.is_null();
                } else if (val.is_boolean()) {
                    cond = val.template get<bool>();
                } else {
                    cond = !val.is_null();
                }
                if (cond) {
                    if (arg->isBlock) {
                        result += renderBlockBody(arg->thenBody, ctx, arg->insertCol);
                    } else {
                        result += renderNodes(arg->thenBody, ctx);
                    }
                } else {
                    if (arg->isBlock) {
                        result += renderBlockBody(arg->elseBody, ctx, arg->insertCol);
                    } else {
                        result += renderNodes(arg->elseBody, ctx);
                    }
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>) {
                auto val = ctx.resolve(arg->expr);
                if (val.is_object()) {
                    std::string savedPolicy = ctx.activePolicy;
                    if (!arg->policy.empty()) ctx.activePolicy = arg->policy;
                    for (auto &c : arg->cases) {
                        if (val.contains(c.tag)) {
                            if (!c.bindingName.empty()) {
                                ctx.pushBinding(c.bindingName, val[c.tag]);
                            }
                            if (arg->isBlock) {
                                result += renderBlockBody(c.body, ctx, arg->insertCol);
                            } else {
                                result += renderNodes(c.body, ctx);
                            }
                            if (!c.bindingName.empty()) {
                                ctx.popBinding();
                            }
                            break;
                        }
                    }
                    ctx.activePolicy = savedPolicy;
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>) {
                // Find the function by name
                const TemplateFunction *targetFunc = nullptr;
                for (auto &f : ctx.allFunctions) {
                    if (f.name == arg->functionName) {
                        targetFunc = &f;
                        break;
                    }
                }
                if (targetFunc) {
                    // Evaluate arguments and push positional bindings
                    for (size_t i = 0; i < arg->arguments.size() && i < targetFunc->params.size(); ++i) {
                        auto val = ctx.resolve(arg->arguments[i]);
                        ctx.pushBinding(targetFunc->params[i].name, val);
                    }
                    // Propagate policy: function's own policy takes precedence; otherwise inherit caller's
                    std::string savedPolicy = ctx.activePolicy;
                    if (!targetFunc->policy.empty()) ctx.activePolicy = targetFunc->policy;
                    std::string callResult = renderNodes(targetFunc->body, ctx);
                    ctx.activePolicy = savedPolicy;
                    // Strip trailing newline from function result
                    if (!callResult.empty() && callResult.back() == '\n')
                        callResult.pop_back();
                    result += callResult;
                    // Pop bindings in reverse
                    for (size_t i = 0; i < arg->arguments.size() && i < targetFunc->params.size(); ++i) {
                        ctx.popBinding();
                    }
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>) {
                auto collection = ctx.resolve(arg->collectionExpr);
                if (collection.is_array()) {
                    std::string savedPolicy = ctx.activePolicy;
                    if (!arg->policy.empty()) ctx.activePolicy = arg->policy;
                    for (size_t i = 0; i < collection.size(); ++i) {
                        // Find the right function overload
                        const TemplateFunction *targetFunc = nullptr;
                        std::vector<const TemplateFunction *> overloads;
                        for (auto &f : ctx.allFunctions) {
                            if (f.name == arg->functionName)
                                overloads.push_back(&f);
                        }
                        if (overloads.size() == 1) {
                            targetFunc = overloads[0];
                        } else if (overloads.size() > 1 && collection[i].is_object()) {
                            // Variant dispatch: find which overload matches the payload type
                            for (auto it = collection[i].begin(); it != collection[i].end(); ++it) {
                                const auto &payload = it.value();
                                for (auto *ovl : overloads) {
                                    if (ovl->params.size() == 1) {
                                        const auto &ptype = ovl->params[0].type;
                                        bool matches = false;
                                        if (std::holds_alternative<StringType>(ptype) && payload.is_string())
                                            matches = true;
                                        else if (std::holds_alternative<IntType>(ptype) && payload.is_number())
                                            matches = true;
                                        else if (std::holds_alternative<BoolType>(ptype) && payload.is_boolean())
                                            matches = true;
                                        if (matches) {
                                            targetFunc = ovl;
                                            break;
                                        }
                                    }
                                }
                                if (targetFunc) break;
                            }
                        }
                        if (targetFunc && !targetFunc->params.empty()) {
                            nlohmann::json elemVal = collection[i];
                            // For variant dispatch with overloads, extract the payload value
                            if (overloads.size() > 1 && elemVal.is_object()) {
                                for (auto it = elemVal.begin(); it != elemVal.end(); ++it) {
                                    elemVal = it.value();
                                    break;
                                }
                            }
                            ctx.pushBinding(targetFunc->params[0].name, elemVal);
                            // Function's own policy takes precedence; otherwise inherit the render-via policy
                            std::string funcSavedPolicy = ctx.activePolicy;
                            if (!targetFunc->policy.empty()) ctx.activePolicy = targetFunc->policy;
                            std::string iterResult = renderNodes(targetFunc->body, ctx);
                            ctx.activePolicy = funcSavedPolicy;
                            if (!iterResult.empty() && iterResult.back() == '\n')
                                iterResult.pop_back();
                            ctx.popBinding();
                            result += arg->precededBy;
                            result += iterResult;
                            if (i + 1 < collection.size()) {
                                result += arg->sep;
                            } else if (!arg->followedBy.empty()) {
                                result += arg->followedBy;
                            }
                        }
                    }
                    ctx.activePolicy = savedPolicy;
                }
            } }, node);
        }
        return result;
    }

    static std::string renderBlockBody(const std::vector<ASTNode> &nodes, RenderContext &ctx, int insertCol)
    {
        std::string raw = renderNodes(nodes, ctx);

        if (raw.empty())
            return "";

        std::vector<std::string> lines;
        std::istringstream iss(raw);
        std::string line;
        while (std::getline(iss, line))
        {
            lines.push_back(line);
        }
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
            // De-indent by zero-marker
            if (!l.empty() && !zeroMarker.empty() && l.size() >= zeroMarker.size() &&
                l.compare(0, zeroMarker.size(), zeroMarker) == 0)
            {
                l = l.substr(zeroMarker.size());
            }
            // Re-indent
            if (!l.empty())
            {
                result += indent + l;
            }
            if (i + 1 < lines.size() || trailingNl)
            {
                result += "\n";
            }
        }

        return result;
    }

} // namespace tpp
