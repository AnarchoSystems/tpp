#pragma once

#include <tpp/AST.h>
#include <tpp/Tokenizer.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>

namespace tpp
{
    // ── Typedef parser ──

    struct TypedefParser
    {
        const std::vector<Token> &tokens;
        size_t pos = 0;
        TypeRegistry &reg;
        std::vector<Diagnostic> &diags;
        bool ok = true;

        const Token &cur() const;
        const Token &eat(TokKind k);
        bool at(TokKind k) const;

        TypeRef parseTypeRef();
        void parseStruct();
        void parseEnum();
        bool validateTypes();
        void parse();
    };

    // ── Expression parser ──

    Expression parseExpression(const std::string &s);
    TypeRef parseParamType(const std::string &s);

    // ── Template directive classification ──

    enum class DirectiveKind
    {
        Expr,
        For,
        EndFor,
        If,
        Else,
        Endif,
        Switch,
        Case,
        EndCase,
        EndSwitch,
        FunctionCall,
        Render
    };

    struct DirectiveInfo
    {
        DirectiveKind kind;
        std::string forVar;
        Expression forCollection = Variable{""};
        std::string forCollectionText;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        std::string iteratorVar;
        Expression ifCond = Variable{""};
        std::string ifExprText;
        bool ifNegated = false;
        Expression switchExpr = Variable{""};
        bool switchCheckExhaustive = false;
        std::string caseTag;
        std::string caseBinding;
        Expression expr = Variable{""};
        // Function call
        std::string funcCallName;
        std::vector<Expression> funcCallArgs;
        // Render via
        std::string renderExprText;
        Expression renderExpr = Variable{""};
        std::string renderFunc;
        std::string renderSep;
        std::string renderFollowedBy;
        std::string renderPrecededBy;

        bool hasError = false;
        std::string errorMessage;
        int errorStart = 0;
        int errorEnd = 0;
    };

    DirectiveInfo classifyDirective(const std::string &s);

    // ── Template line parser ──

    struct LineSeg
    {
        bool isDirective;
        std::string text;
        DirectiveInfo info;
        int startCol = 0;
        int endCol = 0;
    };

    struct TemplateLine
    {
        std::vector<LineSeg> segments;
        bool isBlockLine = false;
        int indent = 0;
    };

    std::vector<TemplateLine> parseTemplateLines(const std::string &body);

    // ── AST builders ──

    struct InlineParser
    {
        const std::vector<LineSeg> &segs;
        size_t pos = 0;

        std::vector<ASTNode> parse();
    };

    struct ASTBuilder
    {
        const std::vector<TemplateLine> &lines;
        size_t pos = 0;

        std::vector<ASTNode> parseBlock(int insertCol);
    };

    // ── High-level template parsing ──

    bool parseOneTemplate(const std::string &text, size_t &pos,
                          TemplateFunction &func,
                          size_t *outBodyStartLine = nullptr,
                          std::string *outBodyText = nullptr);
}
