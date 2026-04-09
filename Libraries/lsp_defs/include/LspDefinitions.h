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

    // clang-format off

    #define TPP_SEP_COMMA ,

#define TPP_LSP_TOKEN_TYPES(X, SEP)                 \
    X(keyword,      keyword,        0)      SEP     \
    X(type,         type,           1)      SEP     \
    X(variable,     variable,       2)      SEP     \
    X(property,     property,       3)      SEP     \
    X(function,     function,       4)      SEP     \
    X(string_,      string,         5)      SEP     \
    X(parameter,    parameter,      6)      SEP     \
    X(operator_,    operator,       7)      SEP     \
    X(enumMember,   enumMember,     8)

    // clang-format on

    enum class SemanticTokenType : int
    {
#define MAKE_TPP_SEMANTIC_TOKEN_ENUM(case_name, name, idx) case_name = idx
        TPP_LSP_TOKEN_TYPES(MAKE_TPP_SEMANTIC_TOKEN_ENUM, TPP_SEP_COMMA)
    };

    constexpr int tokenTypeIndex(SemanticTokenType t) noexcept
    {
        return static_cast<int>(t);
    }

    constexpr const char *tokenTypeName(SemanticTokenType t) noexcept
    {
        switch (t)
        {
#define MAKE_TPP_SEMANTIC_TOKEN_NAME(case_name, name, idx) \
    case SemanticTokenType::case_name:                     \
        return #name;
            TPP_LSP_TOKEN_TYPES(MAKE_TPP_SEMANTIC_TOKEN_NAME, )
        default:
            return "unknown";
        }
    }

    inline SemanticTokenType tokenTypeFromName(const std::string &name)
    {
#define MAKE_TPP_SEMANTIC_TOKEN_FROM_NAME(case_name, type, idx) \
    if (name == #type)                                          \
        return SemanticTokenType::case_name;
        TPP_LSP_TOKEN_TYPES(MAKE_TPP_SEMANTIC_TOKEN_FROM_NAME, )
        throw std::invalid_argument("Unknown semantic token type: " + name);
    }

    // Array of token type names in legend order — used to populate the LSP
    // initialize response and to decode token type indices in tests.
    inline nlohmann::json tokenTypeLegend()
    {
#define MAKE_TPP_SEMANTIC_TOKEN_LEGEND(case_name, name, idx) tokenTypeName(SemanticTokenType::case_name)
        std::vector<std::string> legend = {TPP_LSP_TOKEN_TYPES(MAKE_TPP_SEMANTIC_TOKEN_LEGEND, TPP_SEP_COMMA)};
        return legend;
    }

    // ── Delta-decode ──────────────────────────────────────────────────────────────

    struct SemanticToken
    {
        int line;
        int character;
        int length;
        SemanticTokenType type;
        int modifiers = 0;
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
            const int dl = data[i].get<int>();
            const int dc = data[i + 1].get<int>();
            const int len = data[i + 2].get<int>();
            const int tt = data[i + 3].get<int>();
            const int mod = data[i + 4].get<int>();
            if (dl != 0)
                character = 0;
            line += dl;
            character += dc;
            out.push_back({line, character, len,
                           static_cast<SemanticTokenType>(tt), mod});
        }
        return out;
    }

} // namespace lsp
