#include <tpp/Policy.h>
#include <tpp/Compiler.h>
#include <string>

namespace tpp::compiler
{

    static bool parsePolicy(const nlohmann::json &j,
                            std::map<std::string, Policy> &policies,
                            std::string &error)
    {
        if (!j.is_object())
        {
            error = "policy entry must be a JSON object";
            return false;
        }
        if (!j.contains("tag") || !j.at("tag").is_string())
        {
            error = "policy entry is missing required string field 'tag'";
            return false;
        }
        std::string tag = j.at("tag").get<std::string>();
        if (tag.empty())
        {
            error = "policy 'tag' must not be empty";
            return false;
        }
        if (policies.count(tag))
        {
            error = "duplicate policy tag '" + tag + "'";
            return false;
        }

        Policy pol;
        pol.tag = tag;

        if (j.contains("length"))
        {
            const auto &lj = j.at("length");
            if (!lj.is_object())
            {
                error = "[policy " + tag + "] 'length' must be an object";
                return false;
            }
            PolicyLength len;
            if (lj.contains("min"))
            {
                if (!lj.at("min").is_number_integer())
                { error = "[policy " + tag + "] 'length.min' must be an integer"; return false; }
                len.min = lj.at("min").get<int>();
            }
            if (lj.contains("max"))
            {
                if (!lj.at("max").is_number_integer())
                { error = "[policy " + tag + "] 'length.max' must be an integer"; return false; }
                len.max = lj.at("max").get<int>();
            }
            pol.length = len;
        }

        if (j.contains("reject-if"))
        {
            const auto &rj = j.at("reject-if");
            if (!rj.is_object() || !rj.contains("regex"))
            {
                error = "[policy " + tag + "] 'reject-if' must be an object with a 'regex' field";
                return false;
            }
            PolicyRejectIf ri;
            ri.regex   = rj.at("regex").get<std::string>();
            if (rj.contains("message")) ri.message = rj.at("message").get<std::string>();
            ri.compiled_rx = std::regex(ri.regex);
            pol.rejectIf = ri;
        }

        auto parseRegexSteps = [&](const std::string &fieldName,
                                   std::vector<PolicyRegexStep> &out) -> bool
        {
            if (!j.contains(fieldName)) return true;
            const auto &arr = j.at(fieldName);
            if (!arr.is_array())
            {
                error = "[policy " + tag + "] '" + fieldName + "' must be an array";
                return false;
            }
            for (const auto &item : arr)
            {
                if (!item.is_object() || !item.contains("regex"))
                {
                    error = "[policy " + tag + "] each entry in '" + fieldName +
                            "' must be an object with a 'regex' field";
                    return false;
                }
                PolicyRegexStep step;
                step.regex = item.at("regex").get<std::string>();
                if (item.contains("replace")) step.replace = item.at("replace").get<std::string>();
                step.compiled_rx = std::regex(step.regex);
                if (!step.replace.empty()) step.compiled_replace = precompile_replace(step.replace);
                out.push_back(std::move(step));
            }
            return true;
        };

        if (!parseRegexSteps("require", pol.require))            return false;
        if (!parseRegexSteps("output-filter", pol.outputFilter)) return false;

        if (j.contains("replacements"))
        {
            const auto &arr = j.at("replacements");
            if (!arr.is_array())
            {
                error = "[policy " + tag + "] 'replacements' must be an array";
                return false;
            }
            for (const auto &item : arr)
            {
                if (!item.is_object() || !item.contains("find") || !item.contains("replace"))
                {
                    error = "[policy " + tag + "] each entry in 'replacements' must have 'find' and 'replace'";
                    return false;
                }
                PolicyReplacement rep;
                rep.find    = item.at("find").get<std::string>();
                rep.replace = item.at("replace").get<std::string>();
                pol.replacements.push_back(std::move(rep));
            }
        }

        policies[tag] = std::move(pol);
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // PolicyRegistry loading
    // ═══════════════════════════════════════════════════════════════════

    bool load_policy_json(PolicyRegistry &registry,
                          const nlohmann::json &j,
                          std::vector<Diagnostic> &diagnostics)
    {
        std::string error;
        if (parsePolicy(j, registry.policies_, error))
            return true;

        diagnostics.push_back(makeErrorDiagnostic(error));
        return false;
    }

    const Policy *PolicyRegistry::find(const std::string &tag) const noexcept
    {
        auto it = policies_.find(tag);
        if (it == policies_.end()) return nullptr;
        return &it->second;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Compiler::clear_policies / add_policy
    // ═══════════════════════════════════════════════════════════════════

} // namespace tpp::compiler

namespace tpp
{

    void Compiler::clear_policies() noexcept
    {
        semanticModel_.mutable_policies() = compiler::PolicyRegistry{};
    }

    bool Compiler::add_policy_text(const std::string &policyText,
                                   std::vector<Diagnostic> &diagnostics) noexcept
    {
        if (policyText.empty())
        {
            diagnostics.push_back(makeErrorDiagnostic("policy file not found or empty"));
            return false;
        }

        try
        {
            return compiler::load_policy_json(semanticModel_.mutable_policies(),
                                              nlohmann::json::parse(policyText),
                                              diagnostics);
        }
        catch (const std::exception &e)
        {
            diagnostics.push_back(makeErrorDiagnostic(std::string("invalid JSON: ") + e.what()));
            return false;
        }
        catch (...)
        {
            diagnostics.push_back(makeErrorDiagnostic("unknown error loading policy"));
            return false;
        }
    }

} // namespace tpp
