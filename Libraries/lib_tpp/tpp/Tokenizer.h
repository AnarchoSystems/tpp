#pragma once

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
        KwBool,
        Ident,
        Comment,    // // or /* */ — ignored by parser, used for LSP highlighting
        DocComment, // /// or /** */ — ignored by parser, attached as doc to next declaration
        Eof
    };

    struct Token
    {
        TokKind kind;
        std::string text;
        int line; // 0-based
        int col;  // 0-based
    };

    // ── Template tokenizer ──

    struct Segment
    {
        bool isDirective;
        std::string text;
        int startCol = 0;
        int endCol = 0;
    };

    namespace compiler
    {
        std::vector<Segment> tokenize_template(const std::string &src);
    }
}
