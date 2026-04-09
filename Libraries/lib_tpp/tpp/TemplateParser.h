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
        std::string enumerator;
        std::string policy;
        std::string alignSpec; // empty = no alignment; "" with hasAlign=true = default left
        bool        hasAlign = false;
        // Option-parse errors stored here for recovery: body still processes
        // so that variables bound by the loop remain in scope.
        std::vector<std::tuple<std::string,int,int>> parseErrors; // {message, start, end}
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

    // Alignment cell marker: @&@
    struct AlignmentCellDirective {};

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
        AlignmentCellDirective,
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
        size_t bodyStartLine = 0; // 0-based line offset of the body within the file

        // outEndRange, if non-null, is filled with the range of the closing directive
        // (e.g. @end for@, @end if@) when the block is terminated by one.
        std::vector<ASTNode> parseBlock(int insertCol, Range *outEndRange = nullptr);

    private:
        // Parses inline segments (a single non-block line) into AST nodes.
        // lineIndex is the 0-based index of this line within the body (added to bodyStartLine).
        std::vector<ASTNode> parseInline(const std::vector<LineSeg> &segs,
                                         size_t startPos = 0,
                                         int lineIndex = 0);

        Range makeRange(int lineIndex, const LineSeg &seg) const
        {
            return {{(int)(bodyStartLine + lineIndex), seg.startCol - 1},
                    {(int)(bodyStartLine + lineIndex), seg.endCol + 1}};
        }
    };

    // ── High-level template parsing ──

    bool parseOneTemplate(const std::string &text, size_t &pos,
                          TemplateFunction &func,
                          size_t *outBodyStartLine = nullptr,
                          std::string *outBodyText = nullptr,
                          std::vector<TemplateLine> *outTemplateLines = nullptr,
                          std::vector<Diagnostic> *diags = nullptr,
                          Range *outHeaderRange = nullptr,
                          std::string *outHeaderText = nullptr);
}
