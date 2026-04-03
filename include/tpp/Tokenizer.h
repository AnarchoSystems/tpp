#pragma once

#include <tpp/AST.h>
#include <string>
#include <vector>

namespace tpp
{
    // ── Typedef tokenizer ──

    enum class TokKind
    {
        Struct,
        Enum,
        LBrace,
        RBrace,
        Semi,
        Colon,
        LParen,
        RParen,
        Comma,
        LAngle,
        RAngle,
        KwOptional,
        KwList,
        KwString,
        KwInt,
        Ident,
        Eof
    };

    struct Token
    {
        TokKind kind;
        std::string text;
        int line; // 0-based
        int col;  // 0-based
    };

    std::vector<Token> tokenize_typedefs(const std::string &src);

    // ── Template tokenizer ──

    struct Segment
    {
        bool isDirective;
        std::string text;
    };

    std::vector<Segment> tokenize_template(const std::string &src);
}
