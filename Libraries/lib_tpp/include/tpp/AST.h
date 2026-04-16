#pragma once

#include <tpp/Types.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <optional>

namespace tpp::compiler
{
    // ── Expressions ──

    struct Variable    { std::string name; };
    struct FieldAccess;
    using Expression = std::variant<Variable, std::shared_ptr<FieldAccess>>;
    struct FieldAccess { Expression base; std::string field; };

    // ── AST nodes ──

    struct TextNode            { std::string text; };
    struct AlignmentCellNode  { Range sourceRange; };
    struct CommentNode        { Range startRange; Range endRange; }; // @comment@...@end comment@
    struct InterpolationNode { Expression expr; std::string policy; Range sourceRange{}; };
    struct ForNode;
    struct IfNode;
    struct SwitchNode;
    struct FunctionCallNode;
    struct RenderViaNode;

    using ASTNode = std::variant<TextNode,
                                 AlignmentCellNode,
                                 CommentNode,
                                 InterpolationNode,
                                 std::shared_ptr<ForNode>,
                                 std::shared_ptr<IfNode>,
                                 std::shared_ptr<SwitchNode>,
                                 std::shared_ptr<FunctionCallNode>,
                                 std::shared_ptr<RenderViaNode>>;

    struct ForNode
    {
        std::string varName;
        std::string enumeratorName;
        Expression collectionExpr;
        std::vector<ASTNode> body;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        bool isBlock = false;
        int insertCol = 0;
        std::string policy;
        std::string alignSpec; // empty = no alignment; "" with hasAlign=true = left-align all
        bool hasAlign = false;
        Range sourceRange{};
        Range endRange{};
    };

    struct IfNode
    {
        Expression condExpr;
        bool negated = false;
        std::string condText;
        std::vector<ASTNode> thenBody;
        std::vector<ASTNode> elseBody;
        bool isBlock = false;
        int insertCol = 0;
        Range sourceRange{};
        Range elseRange{};
        Range endRange{};
    };

    struct CaseNode
    {
        std::string tag;
        std::string bindingName;
        bool isSyntheticRenderCase = false;
        std::vector<ASTNode> body;
        Range sourceRange{};
        Range endRange{};
    };

    struct SwitchNode
    {
        Expression expr;
        bool checkExhaustive = false;
        std::vector<CaseNode> cases;
        std::optional<CaseNode> defaultCase;
        bool isBlock = false;
        int insertCol = 0;
        std::string policy;
        Range sourceRange{};
        Range endRange{};
    };

    struct FunctionCallNode
    {
        std::string functionName;
        std::vector<Expression> arguments;
        Range sourceRange{};
    };

    struct RenderViaNode
    {
        Expression collectionExpr;
        std::string functionName;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        bool isBlock = false;
        int insertCol = 0;
        std::string policy;
        Range sourceRange{};
    };

    // ── Template function ──

    struct ParamDef
    {
        std::string name;
        TypeRef type;
    };

    struct TemplateFunction
    {
        std::string name;
        std::vector<ParamDef> params;
        std::vector<ASTNode> body;
        std::string policy;
        std::string doc;
        Range sourceRange{};
    };

    // ─────────────────────────────────────────────────────────────────
    // Equality
    // ─────────────────────────────────────────────────────────────────

    // Mutual-recursion forward declarations
    inline bool operator==(const Expression &a, const Expression &b);
    inline bool operator==(const ASTNode &a, const ASTNode &b);

    inline bool operator==(const Variable &a, const Variable &b)
    { return a.name == b.name; }
    inline bool operator==(const FieldAccess &a, const FieldAccess &b)
    { return a.base == b.base && a.field == b.field; }
    inline bool operator==(const Expression &a, const Expression &b)
    {
        if (a.index() != b.index()) return false;
        return std::visit([&](auto &&av) -> bool
        {
            using T = std::decay_t<decltype(av)>;
            const auto &bv = std::get<T>(b);
            if constexpr (std::is_same_v<T, std::shared_ptr<FieldAccess>>)
                return av && bv && *av == *bv;
            else
                return av == bv;
        }, a);
    }

    // AlignmentCellNode is a pure delimiter marker — two markers are always equal.
    inline bool operator==(const AlignmentCellNode &, const AlignmentCellNode &) { return true; }
    // CommentNode carries only LSP range info — always semantically equal.
    inline bool operator==(const CommentNode &, const CommentNode &) { return true; }

    inline bool operator==(const TextNode &a, const TextNode &b)
    { return a.text == b.text; }
    inline bool operator==(const InterpolationNode &a, const InterpolationNode &b)
    { return a.expr == b.expr && a.policy == b.policy; }
    inline bool operator==(const CaseNode &a, const CaseNode &b)
    {
        return a.tag == b.tag && a.bindingName == b.bindingName &&
               a.isSyntheticRenderCase == b.isSyntheticRenderCase && a.body == b.body;
    }
    inline bool operator==(const ForNode &a, const ForNode &b)
    {
        return a.varName == b.varName && a.enumeratorName == b.enumeratorName &&
               a.collectionExpr == b.collectionExpr && a.body == b.body &&
               a.sep == b.sep && a.followedBy == b.followedBy && a.precededBy == b.precededBy &&
               a.isBlock == b.isBlock && a.insertCol == b.insertCol && a.policy == b.policy &&
               a.hasAlign == b.hasAlign && a.alignSpec == b.alignSpec;
    }
    inline bool operator==(const IfNode &a, const IfNode &b)
    {
        return a.condExpr == b.condExpr && a.negated == b.negated && a.condText == b.condText &&
               a.thenBody == b.thenBody && a.elseBody == b.elseBody &&
               a.isBlock == b.isBlock && a.insertCol == b.insertCol;
    }
    inline bool operator==(const SwitchNode &a, const SwitchNode &b)
    {
        return a.expr == b.expr && a.checkExhaustive == b.checkExhaustive &&
               a.cases == b.cases && a.defaultCase == b.defaultCase &&
               a.isBlock == b.isBlock && a.insertCol == b.insertCol &&
               a.policy == b.policy;
    }
    inline bool operator==(const FunctionCallNode &a, const FunctionCallNode &b)
    { return a.functionName == b.functionName && a.arguments == b.arguments; }
    inline bool operator==(const RenderViaNode &a, const RenderViaNode &b)
    {
        return a.collectionExpr == b.collectionExpr && a.functionName == b.functionName &&
               a.sep == b.sep && a.followedBy == b.followedBy && a.precededBy == b.precededBy &&
               a.isBlock == b.isBlock && a.insertCol == b.insertCol && a.policy == b.policy;
    }
    inline bool operator==(const ASTNode &a, const ASTNode &b)
    {
        if (a.index() != b.index()) return false;
        return std::visit([&](auto &&av) -> bool
        {
            using T = std::decay_t<decltype(av)>;
            const auto &bv = std::get<T>(b);
            if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>> ||
                          std::is_same_v<T, std::shared_ptr<IfNode>>  ||
                          std::is_same_v<T, std::shared_ptr<SwitchNode>> ||
                          std::is_same_v<T, std::shared_ptr<FunctionCallNode>> ||
                          std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
                return av && bv && *av == *bv;
            else
                return av == bv;
        }, a);
    }
    inline bool operator==(const ParamDef &a, const ParamDef &b)
    { return a.name == b.name && a.type == b.type; }
    inline bool operator==(const TemplateFunction &a, const TemplateFunction &b)
    { return a.name == b.name && a.params == b.params && a.body == b.body && a.policy == b.policy; }

}

