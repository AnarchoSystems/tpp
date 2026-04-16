#include <tpp/IRAssembler.h>
#include "tpp/Version.h"

namespace tpp
{
// ═══════════════════════════════════════════════════════════════════════════
// Raw typedef extraction
// ═══════════════════════════════════════════════════════════════════════════

// Extract a single type definition (struct or enum) from the raw typedefs string.
// Includes any preceding doc comments (lines starting with //).
static std::string extract_type_definition(const std::string &raw,
                                           const std::string &keyword,
                                           const std::string &name)
{
    std::string marker = keyword + " " + name;
    auto pos = raw.find(marker);
    if (pos == std::string::npos) return "";

    // Walk backward to include preceding doc-comment lines and blank lines
    auto start = pos;
    while (start > 0)
    {
        auto prev_end = start - 1;
        if (prev_end == std::string::npos || raw[prev_end] != '\n') break;
        auto prev_start = raw.rfind('\n', prev_end - 1);
        prev_start = (prev_start == std::string::npos) ? 0 : prev_start + 1;
        auto line = raw.substr(prev_start, prev_end - prev_start);
        auto first_non_ws = line.find_first_not_of(" \t");
        if (first_non_ws == std::string::npos)
        {
            start = prev_start;
            continue;
        }
        if (line.substr(first_non_ws, 2) == "//")
        {
            start = prev_start;
            continue;
        }
        break;
    }

    // Strip leading blank lines from extracted text
    while (start < pos && (raw[start] == '\n' || raw[start] == '\r'))
        ++start;

    // Find closing brace
    auto brace = raw.find('{', pos);
    if (brace == std::string::npos) return "";
    int depth = 0;
    auto end = brace;
    for (; end < raw.size(); ++end)
    {
        if (raw[end] == '{') ++depth;
        else if (raw[end] == '}') { --depth; if (depth == 0) { ++end; break; } }
    }

    return raw.substr(start, end - start);
}

// ═══════════════════════════════════════════════════════════════════════════
// Top-level assembler
// ═══════════════════════════════════════════════════════════════════════════

IR assemble_ir(std::vector<StructDef> structs,
               std::vector<EnumDef> enums,
               std::vector<FunctionDef> functions,
               std::vector<PolicyDef> policies,
               const std::string &rawTypedefs,
               bool)
{
    IR out;
    out.versionMajor = TPP_VERSION_MAJOR;
    out.versionMinor = TPP_VERSION_MINOR;
    out.versionPatch = TPP_VERSION_PATCH;

    for (auto &s : structs)
    {
        s.rawTypedefs = extract_type_definition(rawTypedefs, "struct", s.name);
        out.structs.push_back(std::move(s));
    }
    for (auto &e : enums)
    {
        e.rawTypedefs = extract_type_definition(rawTypedefs, "enum", e.name);
        out.enums.push_back(std::move(e));
    }
    out.functions = std::move(functions);
    out.policies = std::move(policies);

    return out;
}

} // namespace tpp