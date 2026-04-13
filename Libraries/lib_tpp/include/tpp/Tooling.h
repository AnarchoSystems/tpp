#pragma once

#include <tpp/AST.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>

namespace tpp
{
    enum class TypeSourceTokenKind
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
        Comment,
        DocComment,
        Eof
    };

    struct TypeSourceToken
    {
        TypeSourceTokenKind kind;
        std::string text;
        Range range;
    };

    struct ParsedTemplateSource
    {
        compiler::TemplateFunction function;
        size_t bodyStartLine = 0;
        std::string bodyText;
        std::string headerText;
        Range headerRange{};
    };

    std::vector<TypeSourceToken> tokenizeTypeSource(const std::string &src);

    bool parseTemplateSource(const std::string &src,
                             std::vector<ParsedTemplateSource> &templates);

    bool parseTemplateSource(const std::string &src,
                             std::vector<ParsedTemplateSource> &templates,
                             std::vector<Diagnostic> &diagnostics);
}