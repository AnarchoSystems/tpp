#pragma once

#include <tpp/Types.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>

namespace tpp
{
    // ── Expressions ──

    struct Variable    { std::string name; };
    struct FieldAccess;
    using Expression = std::variant<Variable, std::shared_ptr<FieldAccess>>;
    struct FieldAccess { Expression base; std::string field; };

    // ── AST nodes ──

    struct TextNode          { std::string text; };
    struct InterpolationNode { Expression expr; };
    struct ForNode;
    struct IfNode;
    struct SwitchNode;
    struct FunctionCallNode;
    struct RenderViaNode;

    using ASTNode = std::variant<TextNode,
                                 InterpolationNode,
                                 std::shared_ptr<ForNode>,
                                 std::shared_ptr<IfNode>,
                                 std::shared_ptr<SwitchNode>,
                                 std::shared_ptr<FunctionCallNode>,
                                 std::shared_ptr<RenderViaNode>>;

    struct ForNode
    {
        std::string varName;
        std::string iteratorVarName;
        Expression collectionExpr;
        std::vector<ASTNode> body;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        bool isBlock = false;
        int insertCol = 0;
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
    };

    struct CaseNode
    {
        std::string tag;
        std::string bindingName;
        std::vector<ASTNode> body;
    };

    struct SwitchNode
    {
        Expression expr;
        bool checkExhaustive = false;
        std::vector<CaseNode> cases;
        bool isBlock = false;
        int insertCol = 0;
    };

    struct FunctionCallNode
    {
        std::string functionName;
        std::vector<Expression> arguments;
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
    };
}
