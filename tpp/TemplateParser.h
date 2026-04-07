#pragma once

#include <tpp/AST.h>
#include <tpp/Tokenizer.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>
#include <variant>

namespace tpp
{
    // ── Expression parser ──

    Expression parseExpression(const std::string &s);

    // ── Template directive types ──
    // Each directive kind is its own struct, bundled into a variant.
    // This replaces the old "god struct" with 25+ fields most of which
    // were meaningless for any given directive.

    // Error in directive syntax; carries the error message and character range.
    struct ErrorDirective        { std::string message; int start = 0, end = 0; };
    // Inline expression interpolation: @expr@ or @expr | policy="name"@
    struct ExprDirective         { Expression expr; std::string policy; };
    // Direct function call: @fn(arg1, arg2)@
    struct FunctionCallDirective { std::string name; std::vector<Expression> args; };

    // @for item in collection | sep="," ...@
    struct ForDirective {
        std::string var;
        std::string collectionText;
        Expression  collection;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        std::string iteratorVar;
        std::string policy;
    };

    // @if expr@ / @if not expr@
    struct IfDirective     { Expression cond; std::string condText; bool negated = false; };
    // @switch expr | checkExhaustive@
    struct SwitchDirective { Expression expr; bool checkExhaustive = false; std::string policy; };
    // @case Tag@ / @case Tag(binding)@
    struct CaseDirective   { std::string tag; std::string binding; };
    // @render collection via func | sep="," ...@
    struct RenderDirective {
        Expression  expr;
        std::string exprText;
        std::string func;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        std::string policy;
    };

    // Control-flow terminators — carry no data
    struct ElseDirective      {};
    struct EndIfDirective     {};
    struct EndForDirective    {};
    struct EndSwitchDirective {};
    struct EndCaseDirective   {};

    using DirectiveInfo = std::variant<
        ErrorDirective,
        ExprDirective,
        FunctionCallDirective,
        ForDirective,
        IfDirective,
        SwitchDirective,
        CaseDirective,
        RenderDirective,
        ElseDirective,
        EndIfDirective,
        EndForDirective,
        EndSwitchDirective,
        EndCaseDirective
    >;

    // Returns true for flow-control directives (for/if/switch/end*/else).
    // Returns false for expression interpolations and direct function calls.
    inline bool isStructuralDirective(const DirectiveInfo &info)
    {
        return !std::holds_alternative<ExprDirective>(info) &&
               !std::holds_alternative<FunctionCallDirective>(info);
    }

    DirectiveInfo classifyDirective(const std::string &s);

    // ── Template line structs ──

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

    // ── AST builder ──

    struct ASTBuilder
    {
        const std::vector<TemplateLine> &lines;
        size_t pos = 0;

        std::vector<ASTNode> parseBlock(int insertCol);

    private:
        // Parses inline segments (a single non-block line) into AST nodes.
        std::vector<ASTNode> parseInline(const std::vector<LineSeg> &segs, size_t startPos = 0);
    };

    // ── High-level template parsing ──

    bool parseOneTemplate(const std::string &text, size_t &pos,
                          TemplateFunction &func,
                          size_t *outBodyStartLine = nullptr,
                          std::string *outBodyText = nullptr,
                          std::vector<TemplateLine> *outTemplateLines = nullptr,
                          std::vector<Diagnostic> *diags = nullptr);
}
