#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <optional>

namespace tpp
{
    // ── Type system ──

    struct StringType
    {
    };
    struct IntType
    {
    };
    struct ListType;
    struct OptionalType;
    struct NamedType
    {
        std::string name;
    };

    using TypeRef = std::variant<StringType, IntType,
                                 std::shared_ptr<ListType>,
                                 std::shared_ptr<OptionalType>,
                                 NamedType>;

    struct ListType
    {
        TypeRef elementType;
    };
    struct OptionalType
    {
        TypeRef innerType;
    };

    struct FieldDef
    {
        std::string name;
        TypeRef type;
    };
    struct StructDef
    {
        std::string name;
        std::vector<FieldDef> fields;
    };
    struct VariantDef
    {
        std::string tag;
        std::optional<TypeRef> payload;
    };
    struct EnumDef
    {
        std::string name;
        std::vector<VariantDef> variants;
    };

    enum class TypeKind
    {
        Struct,
        Enum
    };
    struct TypeEntry
    {
        TypeKind kind;
        std::size_t index;
    };

    struct TypeRegistry
    {
        std::vector<StructDef> structs;
        std::vector<EnumDef> enums;
        std::map<std::string, TypeEntry> nameIndex;
    };

    // ── AST ──

    struct Variable
    {
        std::string name;
    };
    struct FieldAccess;
    using Expression = std::variant<Variable, std::shared_ptr<FieldAccess>>;
    struct FieldAccess
    {
        Expression base;
        std::string field;
    };

    struct TextNode
    {
        std::string text;
    };
    struct InterpolationNode
    {
        Expression expr;
    };
    struct ForNode;
    struct IfNode;
    struct SwitchNode;

    using ASTNode = std::variant<TextNode,
                                 InterpolationNode,
                                 std::shared_ptr<ForNode>,
                                 std::shared_ptr<IfNode>,
                                 std::shared_ptr<SwitchNode>>;

    struct ForNode
    {
        std::string varName;
        Expression collectionExpr;
        std::vector<ASTNode> body;
        std::string sep;
        std::string followedBy;
        bool isBlock = false;
        int insertCol = 0;
    };

    struct IfNode
    {
        Expression condExpr;
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
        std::vector<CaseNode> cases;
        bool isBlock = false;
        int insertCol = 0;
    };

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
