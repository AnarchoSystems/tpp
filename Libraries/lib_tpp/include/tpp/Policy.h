#pragma once

#include <tpp/Diagnostic.h>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <regex>
#include <nlohmann/json.hpp>

namespace tpp
{

    // Transforms @subexpr_N@ -> $N for use with regex replacement APIs.
    inline std::string precompile_replace(const std::string &replace)
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

}

namespace tpp::compiler
{

    // ═══════════════════════════════════════════════════════════════════
    // Policy  —  data model for interpolation policies
    // ═══════════════════════════════════════════════════════════════════

    struct PolicyLength
    {
        std::optional<int> min;
        std::optional<int> max;
    };

    struct PolicyRejectIf
    {
        std::string regex;
        std::string message;
        // Pre-compiled; not serialized.
        std::regex compiled_rx;
    };

    // One entry in the `require` or `output-filter` array.
    // If `replace` is set, the matched string is replaced (with @subexpr_N@ substitution).
    // If `replace` is empty, the value must match the regex or an error is produced.
    struct PolicyRegexStep
    {
        std::string regex;
        std::string replace; // empty = require-match only (no substitution)
        // Pre-compiled; not serialized.
        std::regex compiled_rx;
        std::string compiled_replace; // replace with @subexpr_N@ → $N transformed
    };

    // One entry in the `replacements` array: literal-string find/replace.
    struct PolicyReplacement
    {
        std::string find;
        std::string replace;
    };

    struct Policy
    {
        std::string tag;
        std::optional<PolicyLength> length;
        std::optional<PolicyRejectIf> rejectIf;
        std::vector<PolicyRegexStep> require;
        std::vector<PolicyReplacement> replacements;
        std::vector<PolicyRegexStep> outputFilter;
    };

    class PolicyRegistry
    {
    public:
        // Returns nullptr if the tag is not registered.
        const Policy *find(const std::string &tag) const noexcept;

        const std::map<std::string, Policy> &all() const noexcept { return policies_; }

        bool operator==(const PolicyRegistry &other) const { return policies_ == other.policies_; }

    private:
        friend bool load_policy_json(PolicyRegistry &registry,
                                     const nlohmann::json &j,
                                     std::vector<Diagnostic> &diagnostics);
        std::map<std::string, Policy> policies_;
    };

    bool load_policy_json(PolicyRegistry &registry,
                          const nlohmann::json &j,
                          std::vector<Diagnostic> &diagnostics);

    // ── JSON serialization (needed to round-trip through IR) ──

    inline bool operator==(const PolicyLength &a, const PolicyLength &b)
    { return a.min == b.min && a.max == b.max; }
    inline bool operator==(const PolicyRejectIf &a, const PolicyRejectIf &b)
    { return a.regex == b.regex && a.message == b.message; }
    inline bool operator==(const PolicyRegexStep &a, const PolicyRegexStep &b)
    { return a.regex == b.regex && a.replace == b.replace; }
    inline bool operator==(const PolicyReplacement &a, const PolicyReplacement &b)
    { return a.find == b.find && a.replace == b.replace; }
    inline bool operator==(const Policy &a, const Policy &b)
    {
        return a.tag == b.tag && a.length == b.length && a.rejectIf == b.rejectIf &&
               a.require == b.require && a.replacements == b.replacements &&
               a.outputFilter == b.outputFilter;
    }

    inline void to_json(nlohmann::json &j, const PolicyLength &v)
    {
        j = nlohmann::json::object();
        if (v.min) j["min"] = *v.min;
        if (v.max) j["max"] = *v.max;
    }
    inline void from_json(const nlohmann::json &j, PolicyLength &v)
    {
        if (j.contains("min")) v.min = j.at("min").get<int>();
        if (j.contains("max")) v.max = j.at("max").get<int>();
    }
    inline void to_json(nlohmann::json &j, const PolicyRejectIf &v)
    { j = {{"regex", v.regex}, {"message", v.message}}; }
    inline void from_json(const nlohmann::json &j, PolicyRejectIf &v)
    {
        j.at("regex").get_to(v.regex);
        if (j.contains("message")) j.at("message").get_to(v.message);
        v.compiled_rx = std::regex(v.regex);
    }
    inline void to_json(nlohmann::json &j, const PolicyRegexStep &v)
    { j = {{"regex", v.regex}, {"replace", v.replace}}; }
    inline void from_json(const nlohmann::json &j, PolicyRegexStep &v)
    {
        j.at("regex").get_to(v.regex);
        if (j.contains("replace")) j.at("replace").get_to(v.replace);
        v.compiled_rx = std::regex(v.regex);
        if (!v.replace.empty()) v.compiled_replace = tpp::precompile_replace(v.replace);
    }
    inline void to_json(nlohmann::json &j, const PolicyReplacement &v)
    { j = {{"find", v.find}, {"replace", v.replace}}; }
    inline void from_json(const nlohmann::json &j, PolicyReplacement &v)
    {
        j.at("find").get_to(v.find);
        j.at("replace").get_to(v.replace);
    }
    inline void to_json(nlohmann::json &j, const Policy &v)
    {
        j = nlohmann::json::object();
        j["tag"] = v.tag;
        if (v.length) j["length"] = *v.length;
        if (v.rejectIf) j["reject-if"] = *v.rejectIf;
        if (!v.require.empty()) j["require"] = v.require;
        if (!v.replacements.empty()) j["replacements"] = v.replacements;
        if (!v.outputFilter.empty()) j["output-filter"] = v.outputFilter;
    }
    inline void from_json(const nlohmann::json &j, Policy &v)
    {
        j.at("tag").get_to(v.tag);
        if (j.contains("length")) v.length = j.at("length").get<PolicyLength>();
        if (j.contains("reject-if")) v.rejectIf = j.at("reject-if").get<PolicyRejectIf>();
        if (j.contains("require")) v.require = j.at("require").get<std::vector<PolicyRegexStep>>();
        if (j.contains("replacements")) v.replacements = j.at("replacements").get<std::vector<PolicyReplacement>>();
        if (j.contains("output-filter")) v.outputFilter = j.at("output-filter").get<std::vector<PolicyRegexStep>>();
    }
    inline void to_json(nlohmann::json &j, const PolicyRegistry &v)
    {
        j = nlohmann::json::array();
        for (auto &[tag, pol] : v.all())
        {
            nlohmann::json pj;
            to_json(pj, pol);
            j.push_back(pj);
        }
    }
    inline void from_json(const nlohmann::json &j, PolicyRegistry &v)
    {
        std::vector<Diagnostic> diagnostics;
        for (const auto &item : j)
            load_policy_json(v, item, diagnostics);
    }

} // namespace tpp
