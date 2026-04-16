#pragma once

#include <tpp/Types.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

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

    // ─────────────────────────────────────────────────────────────────
    // JSON serialization
    // ─────────────────────────────────────────────────────────────────

    // Mutual-recursion forward declarations
    inline void to_json(nlohmann::json &j, const Expression &e);
    inline void from_json(const nlohmann::json &j, Expression &e);
    inline void to_json(nlohmann::json &j, const ASTNode &n);
    inline void from_json(const nlohmann::json &j, ASTNode &n);

    inline void to_json(nlohmann::json &j, const Expression &e)
    {
        std::visit([&](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Variable>)
                j = {{"kind", "var"}, {"name", arg.name}};
            else
            {
                nlohmann::json baseJson;
                to_json(baseJson, arg->base);
                j = {{"kind", "field"}, {"base", baseJson}, {"field", arg->field}};
            }
        }, e);
    }
    inline void from_json(const nlohmann::json &j, Expression &e)
    {
        std::string kind = j.at("kind").get<std::string>();
        if (kind == "var")
            e = Variable{j.at("name").get<std::string>()};
        else
        {
            Expression base = j.at("base").get<Expression>();
            e = std::make_shared<FieldAccess>(FieldAccess{std::move(base), j.at("field").get<std::string>()});
        }
    }

    inline void to_json(nlohmann::json &j, const CaseNode &c)
    {
        nlohmann::json bodyJson = nlohmann::json::array();
        for (const auto &n : c.body) { nlohmann::json nj; to_json(nj, n); bodyJson.push_back(nj); }
        j = {{"tag", c.tag},
             {"bindingName", c.bindingName},
             {"isSyntheticRenderCase", c.isSyntheticRenderCase},
             {"body", bodyJson}};
    }
    inline void from_json(const nlohmann::json &j, CaseNode &c)
    {
        j.at("tag").get_to(c.tag);
        j.at("bindingName").get_to(c.bindingName);
        c.isSyntheticRenderCase = j.value("isSyntheticRenderCase", false);
        c.body = j.at("body").get<std::vector<ASTNode>>();
    }

    inline void to_json(nlohmann::json &j, const ASTNode &n)
    {
        std::visit([&](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, TextNode>)
            {
                j = {{"kind", "text"}, {"text", arg.text}};
            }
            else if constexpr (std::is_same_v<T, InterpolationNode>)
            {
                nlohmann::json exprJson; to_json(exprJson, arg.expr);
                j = {{"kind", "interpolation"}, {"expr", exprJson}, {"policy", arg.policy}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>)
            {
                nlohmann::json collJson, bodyJson = nlohmann::json::array();
                to_json(collJson, arg->collectionExpr);
                for (const auto &bn : arg->body) { nlohmann::json bj; to_json(bj, bn); bodyJson.push_back(bj); }
                j = {{"kind", "for"}, {"varName", arg->varName}, {"enumeratorName", arg->enumeratorName},
                     {"collectionExpr", collJson}, {"body", bodyJson},
                     {"sep", arg->sep}, {"followedBy", arg->followedBy}, {"precededBy", arg->precededBy},
                     {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol}, {"policy", arg->policy},
                     {"hasAlign", arg->hasAlign}, {"alignSpec", arg->alignSpec}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
            {
                nlohmann::json condJson, thenJson = nlohmann::json::array(), elseJson = nlohmann::json::array();
                to_json(condJson, arg->condExpr);
                for (const auto &tn : arg->thenBody) { nlohmann::json tj; to_json(tj, tn); thenJson.push_back(tj); }
                for (const auto &en : arg->elseBody) { nlohmann::json ej; to_json(ej, en); elseJson.push_back(ej); }
                j = {{"kind", "if"}, {"condExpr", condJson}, {"negated", arg->negated},
                     {"condText", arg->condText}, {"thenBody", thenJson}, {"elseBody", elseJson},
                     {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
            {
                nlohmann::json exprJson, casesJson = nlohmann::json::array();
                to_json(exprJson, arg->expr);
                for (const auto &c : arg->cases) { nlohmann::json cj; to_json(cj, c); casesJson.push_back(cj); }
                j = {{"kind", "switch"}, {"expr", exprJson}, {"checkExhaustive", arg->checkExhaustive},
                     {"cases", casesJson}, {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol},
                     {"policy", arg->policy}};
                if (arg->defaultCase)
                {
                    nlohmann::json defaultCaseJson;
                    to_json(defaultCaseJson, *arg->defaultCase);
                    j["defaultCase"] = std::move(defaultCaseJson);
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>)
            {
                nlohmann::json argsJson = nlohmann::json::array();
                for (const auto &a : arg->arguments) { nlohmann::json aj; to_json(aj, a); argsJson.push_back(aj); }
                j = {{"kind", "call"}, {"functionName", arg->functionName}, {"arguments", argsJson}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
            {
                nlohmann::json collJson; to_json(collJson, arg->collectionExpr);
                j = {{"kind", "render"}, {"collectionExpr", collJson}, {"functionName", arg->functionName},
                     {"sep", arg->sep}, {"followedBy", arg->followedBy}, {"precededBy", arg->precededBy},
                     {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol}, {"policy", arg->policy}};
            }
            else if constexpr (std::is_same_v<T, AlignmentCellNode>)
            {
                j = {{"kind", "alignCell"}};
            }
            else if constexpr (std::is_same_v<T, CommentNode>)
            {
                j = {{"kind", "comment"}};
            }
        }, n);
    }
    inline void from_json(const nlohmann::json &j, ASTNode &n)
    {
        std::string kind = j.at("kind").get<std::string>();
        if (kind == "text")
        {
            n = TextNode{j.at("text").get<std::string>()};
        }
        else if (kind == "interpolation")
        {
            InterpolationNode in;
            in.expr = j.at("expr").get<Expression>();
            if (j.contains("policy")) j.at("policy").get_to(in.policy);
            n = std::move(in);
        }
        else if (kind == "for")
        {
            auto fn = std::make_shared<ForNode>();
            fn->varName         = j.at("varName").get<std::string>();
            fn->enumeratorName = j.at("enumeratorName").get<std::string>();
            fn->collectionExpr  = j.at("collectionExpr").get<Expression>();
            fn->body            = j.at("body").get<std::vector<ASTNode>>();
            fn->sep             = j.at("sep").get<std::string>();
            fn->followedBy      = j.at("followedBy").get<std::string>();
            fn->precededBy      = j.at("precededBy").get<std::string>();
            fn->isBlock         = j.at("isBlock").get<bool>();
            fn->insertCol       = j.at("insertCol").get<int>();
            if (j.contains("policy")) j.at("policy").get_to(fn->policy);
            if (j.contains("hasAlign")) fn->hasAlign = j.at("hasAlign").get<bool>();
            if (j.contains("alignSpec")) fn->alignSpec = j.at("alignSpec").get<std::string>();
            n = fn;
        }
        else if (kind == "if")
        {
            auto in_ = std::make_shared<IfNode>();
            in_->condExpr   = j.at("condExpr").get<Expression>();
            in_->negated    = j.at("negated").get<bool>();
            in_->condText   = j.at("condText").get<std::string>();
            in_->thenBody   = j.at("thenBody").get<std::vector<ASTNode>>();
            in_->elseBody   = j.at("elseBody").get<std::vector<ASTNode>>();
            in_->isBlock    = j.at("isBlock").get<bool>();
            in_->insertCol  = j.at("insertCol").get<int>();
            n = in_;
        }
        else if (kind == "switch")
        {
            auto sn = std::make_shared<SwitchNode>();
            sn->expr             = j.at("expr").get<Expression>();
            sn->checkExhaustive  = j.at("checkExhaustive").get<bool>();
            sn->cases            = j.at("cases").get<std::vector<CaseNode>>();
            if (j.contains("defaultCase") && !j.at("defaultCase").is_null())
                sn->defaultCase = j.at("defaultCase").get<CaseNode>();
            sn->isBlock          = j.at("isBlock").get<bool>();
            sn->insertCol        = j.at("insertCol").get<int>();
            if (j.contains("policy")) j.at("policy").get_to(sn->policy);
            n = sn;
        }
        else if (kind == "call")
        {
            auto cn = std::make_shared<FunctionCallNode>();
            cn->functionName = j.at("functionName").get<std::string>();
            cn->arguments    = j.at("arguments").get<std::vector<Expression>>();
            n = cn;
        }
        else if (kind == "render")
        {
            auto rn = std::make_shared<RenderViaNode>();
            rn->collectionExpr = j.at("collectionExpr").get<Expression>();
            rn->functionName   = j.at("functionName").get<std::string>();
            rn->sep            = j.at("sep").get<std::string>();
            rn->followedBy     = j.at("followedBy").get<std::string>();
            rn->precededBy     = j.at("precededBy").get<std::string>();
            rn->isBlock        = j.at("isBlock").get<bool>();
            rn->insertCol      = j.at("insertCol").get<int>();
            if (j.contains("policy")) j.at("policy").get_to(rn->policy);
            n = rn;
        }
        else if (kind == "alignCell")
        {
            n = AlignmentCellNode{};
        }
        else if (kind == "comment")
        {
            n = CommentNode{};
        }
    }

    inline void to_json(nlohmann::json &j, const ParamDef &p)
    {
        nlohmann::json typeJson; to_json(typeJson, p.type);
        j = {{"name", p.name}, {"type", typeJson}};
    }
    inline void from_json(const nlohmann::json &j, ParamDef &p)
    {
        j.at("name").get_to(p.name);
        p.type = j.at("type").get<TypeRef>();
    }

    inline void to_json(nlohmann::json &j, const TemplateFunction &f)
    {
        nlohmann::json paramsJson = nlohmann::json::array();
        for (const auto &p : f.params) { nlohmann::json pj; to_json(pj, p); paramsJson.push_back(pj); }
        nlohmann::json bodyJson = nlohmann::json::array();
        for (const auto &n : f.body) { nlohmann::json nj; to_json(nj, n); bodyJson.push_back(nj); }
        j = {{"name", f.name}, {"params", paramsJson}, {"body", bodyJson}, {"policy", f.policy}};
        if (!f.doc.empty()) j["doc"] = f.doc;
    }
    inline void from_json(const nlohmann::json &j, TemplateFunction &f)
    {
        j.at("name").get_to(f.name);
        f.params = j.at("params").get<std::vector<ParamDef>>();
        f.body   = j.at("body").get<std::vector<ASTNode>>();
        if (j.contains("policy")) j.at("policy").get_to(f.policy);
        if (j.contains("doc")) j.at("doc").get_to(f.doc);
    }

}

