#pragma once

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

    struct TemplateToken
    {
        bool isDirective = false;
        std::string text;
        Range range;
    };

    enum class TypeSourceSemanticKind
    {
        Keyword,
        Type,
        Property,
        EnumMember,
        Operator,
        Comment,
    };

    struct TypeSourceSemanticSpan
    {
        Range range;
        TypeSourceSemanticKind kind;
    };

    struct TemplateDirectiveRange
    {
        Range range;
        bool structural = false;
    };

    std::vector<TypeSourceToken> tokenizeTypeSource(const std::string &src);

    std::vector<TemplateToken> tokenizeTemplateSource(const std::string &src);

    std::vector<TypeSourceSemanticSpan> classifyTypeSource(const std::string &src);

    std::vector<TypeSourceSemanticSpan> classifyTypeSourceTokens(const std::vector<TypeSourceToken> &tokens);

    std::vector<TemplateDirectiveRange> extractTemplateDirectiveRanges(const std::string &bodyText,
                                                                       size_t bodyStartLine = 0);
}