#pragma once
// LspDefinitions.h — LSP semantic-token type enum, index/name mapping, and
// delta-decode helper.
//
// Header-only.  Both the tpp-lsp server and the acceptance-test binary include
// this so that token type names used in lsp-test.json exactly match what the
// server advertises in its initialization legend.

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace lsp
{

// ── Semantic token types ──────────────────────────────────────────────────────
// Indices must stay in sync with the legend advertised by onInitialize().
// "string_" and "operator_" avoid clashing with C++ keywords.

enum class SemanticTokenType : int
{
    keyword   = 0,
    type      = 1,
    variable  = 2,
    property  = 3,
    function  = 4,
    string_   = 5,
    parameter = 6,
    operator_ = 7,
    _count    = 8,
};

constexpr int tokenTypeIndex(SemanticTokenType t) noexcept
{
    return static_cast<int>(t);
}

constexpr const char *tokenTypeName(SemanticTokenType t) noexcept
{
    switch (t)
    {
        case SemanticTokenType::keyword:   return "keyword";
        case SemanticTokenType::type:      return "type";
        case SemanticTokenType::variable:  return "variable";
        case SemanticTokenType::property:  return "property";
        case SemanticTokenType::function:  return "function";
        case SemanticTokenType::string_:   return "string";
        case SemanticTokenType::parameter: return "parameter";
        case SemanticTokenType::operator_: return "operator";
        default:                           return "unknown";
    }
}

inline SemanticTokenType tokenTypeFromName(const std::string &name)
{
    if (name == "keyword")   return SemanticTokenType::keyword;
    if (name == "type")      return SemanticTokenType::type;
    if (name == "variable")  return SemanticTokenType::variable;
    if (name == "property")  return SemanticTokenType::property;
    if (name == "function")  return SemanticTokenType::function;
    if (name == "string")    return SemanticTokenType::string_;
    if (name == "parameter") return SemanticTokenType::parameter;
    if (name == "operator")  return SemanticTokenType::operator_;
    throw std::invalid_argument("Unknown semantic token type: " + name);
}

// Array of token type names in legend order — used to populate the LSP
// initialize response and to decode token type indices in tests.
inline nlohmann::json tokenTypeLegend()
{
    nlohmann::json arr = nlohmann::json::array();
    for (int i = 0; i < tokenTypeIndex(SemanticTokenType::_count); ++i)
        arr.push_back(tokenTypeName(static_cast<SemanticTokenType>(i)));
    return arr;
}

// ── Delta-decode ──────────────────────────────────────────────────────────────

struct SemanticToken
{
    int               line;
    int               character;
    int               length;
    SemanticTokenType type;
    int               modifiers = 0;
};

// Decode the flat delta-encoded array from textDocument/semanticTokens/full
// ("result.data") into a list of absolute-position tokens.
inline std::vector<SemanticToken> decodeTokens(const nlohmann::json &data)
{
    std::vector<SemanticToken> out;
    out.reserve(data.size() / 5);
    int line = 0, character = 0;
    for (std::size_t i = 0; i + 4 < data.size(); i += 5)
    {
        const int dl  = data[i    ].get<int>();
        const int dc  = data[i + 1].get<int>();
        const int len = data[i + 2].get<int>();
        const int tt  = data[i + 3].get<int>();
        const int mod = data[i + 4].get<int>();
        if (dl != 0) character = 0;
        line      += dl;
        character += dc;
        out.push_back({line, character, len,
                       static_cast<SemanticTokenType>(tt), mod});
    }
    return out;
}

} // namespace lsp
