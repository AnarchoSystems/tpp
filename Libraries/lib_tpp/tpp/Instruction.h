#pragma once

#include <tpp/Types.h>
#include <tpp/AST.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

namespace tpp::compiler
{
    // ═══════════════════════════════════════════════════════════════════
    // Instruction IR — a lowered, type-resolved representation of
    // template function bodies.  Produced from the AST by the lowering
    // pass and consumed by code-generation backends to emit native
    // rendering code.  Unlike the AST, every expression carries its
    // resolved TypeRef so backends never need type lookup.
    // ═══════════════════════════════════════════════════════════════════

    // ── Expression path with resolved type ──

    struct InstrExpr
    {
        std::string path;          // dotted path: "def", "def.name", "f.fields"
        TypeRef resolvedType;      // static type this expression evaluates to
    };

    // ── Instruction nodes ──

    struct EmitInstr            { std::string text; };
    struct EmitExprInstr        { InstrExpr expr; std::string policy; };
    struct AlignCellInstr       {};

    struct ForInstr;
    struct IfInstr;
    struct SwitchInstr;
    struct CallInstr;
    struct RenderViaInstr;

    using Instruction = std::variant<
        EmitInstr,
        EmitExprInstr,
        AlignCellInstr,
        std::shared_ptr<ForInstr>,
        std::shared_ptr<IfInstr>,
        std::shared_ptr<SwitchInstr>,
        std::shared_ptr<CallInstr>,
        std::shared_ptr<RenderViaInstr>>;

    struct ForInstr
    {
        std::string varName;
        std::string enumeratorName;
        InstrExpr collection;                   // resolvedType is ListType
        std::vector<Instruction> body;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        bool isBlock = false;
        int insertCol = 0;
        std::string policy;
        bool hasAlign = false;
        std::string alignSpec;
    };

    struct IfInstr
    {
        InstrExpr condExpr;
        bool negated = false;
        std::vector<Instruction> thenBody;
        std::vector<Instruction> elseBody;
        bool isBlock = false;
        int insertCol = 0;
    };

    struct CaseInstr
    {
        std::string tag;
        std::string bindingName;
        std::optional<TypeRef> payloadType;     // resolved from the EnumDef
        std::vector<Instruction> body;
    };

    struct SwitchInstr
    {
        InstrExpr expr;                         // resolvedType is NamedType → EnumDef
        bool checkExhaustive = false;
        std::vector<CaseInstr> cases;
        bool isBlock = false;
        int insertCol = 0;
        std::string policy;
    };

    struct CallInstr
    {
        std::string functionName;
        int functionIndex = -1;                 // index into InstructionFunction array
        std::vector<InstrExpr> arguments;
    };

    struct RenderViaInstr
    {
        InstrExpr collection;                   // resolvedType is ListType of enum
        std::string functionName;
        std::string sep;
        std::string followedBy;
        std::string precededBy;
        bool isBlock = false;
        int insertCol = 0;
        std::string policy;
    };

    // ── Instruction-level function ──

    struct InstructionFunction
    {
        std::string name;
        std::vector<ParamDef> params;
        std::vector<Instruction> body;
        std::string policy;
        std::string doc;
        Range sourceRange{};
    };

    // ─────────────────────────────────────────────────────────────────
    // JSON serialization
    // ─────────────────────────────────────────────────────────────────

    inline void to_json(nlohmann::json &j, const InstrExpr &e)
    {
        j = {{"path", e.path}};
        nlohmann::json typeJson;
        to_json(typeJson, e.resolvedType);
        j["type"] = typeJson;
    }
    inline void from_json(const nlohmann::json &j, InstrExpr &e)
    {
        j.at("path").get_to(e.path);
        e.resolvedType = j.at("type").get<TypeRef>();
    }

    // Forward declarations for mutual recursion
    inline void to_json(nlohmann::json &j, const Instruction &i);
    inline void from_json(const nlohmann::json &j, Instruction &i);

    inline void to_json(nlohmann::json &j, const CaseInstr &c)
    {
        nlohmann::json bodyJson = nlohmann::json::array();
        for (const auto &i : c.body) { nlohmann::json ij; to_json(ij, i); bodyJson.push_back(ij); }
        j = {{"tag", c.tag}, {"bindingName", c.bindingName}, {"body", bodyJson}};
        if (c.payloadType.has_value())
        {
            nlohmann::json typeJson;
            to_json(typeJson, *c.payloadType);
            j["payloadType"] = typeJson;
        }
    }
    inline void from_json(const nlohmann::json &j, CaseInstr &c)
    {
        j.at("tag").get_to(c.tag);
        j.at("bindingName").get_to(c.bindingName);
        c.body = j.at("body").get<std::vector<Instruction>>();
        if (j.contains("payloadType"))
            c.payloadType = j.at("payloadType").get<TypeRef>();
    }

    inline void to_json(nlohmann::json &j, const Instruction &i)
    {
        std::visit([&](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, EmitInstr>)
            {
                j = {{"kind", "emit"}, {"text", arg.text}};
            }
            else if constexpr (std::is_same_v<T, EmitExprInstr>)
            {
                nlohmann::json exprJson;
                to_json(exprJson, arg.expr);
                j = {{"kind", "emitExpr"}, {"expr", exprJson}, {"policy", arg.policy}};
            }
            else if constexpr (std::is_same_v<T, AlignCellInstr>)
            {
                j = {{"kind", "alignCell"}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ForInstr>>)
            {
                nlohmann::json collJson;
                to_json(collJson, arg->collection);
                nlohmann::json bodyJson = nlohmann::json::array();
                for (const auto &bi : arg->body) { nlohmann::json bj; to_json(bj, bi); bodyJson.push_back(bj); }
                j = {{"kind", "for"}, {"varName", arg->varName},
                     {"enumeratorName", arg->enumeratorName},
                     {"collection", collJson}, {"body", bodyJson},
                     {"sep", arg->sep}, {"followedBy", arg->followedBy},
                     {"precededBy", arg->precededBy},
                     {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol},
                     {"policy", arg->policy},
                     {"hasAlign", arg->hasAlign}, {"alignSpec", arg->alignSpec}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<IfInstr>>)
            {
                nlohmann::json condJson;
                to_json(condJson, arg->condExpr);
                nlohmann::json thenJson = nlohmann::json::array(), elseJson = nlohmann::json::array();
                for (const auto &ti : arg->thenBody) { nlohmann::json tj; to_json(tj, ti); thenJson.push_back(tj); }
                for (const auto &ei : arg->elseBody) { nlohmann::json ej; to_json(ej, ei); elseJson.push_back(ej); }
                j = {{"kind", "if"}, {"condExpr", condJson}, {"negated", arg->negated},
                     {"thenBody", thenJson}, {"elseBody", elseJson},
                     {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchInstr>>)
            {
                nlohmann::json exprJson;
                to_json(exprJson, arg->expr);
                nlohmann::json casesJson = nlohmann::json::array();
                for (const auto &c : arg->cases) { nlohmann::json cj; to_json(cj, c); casesJson.push_back(cj); }
                j = {{"kind", "switch"}, {"expr", exprJson},
                     {"checkExhaustive", arg->checkExhaustive},
                     {"cases", casesJson},
                     {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol},
                     {"policy", arg->policy}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<CallInstr>>)
            {
                nlohmann::json argsJson = nlohmann::json::array();
                for (const auto &a : arg->arguments) { nlohmann::json aj; to_json(aj, a); argsJson.push_back(aj); }
                j = {{"kind", "call"}, {"functionName", arg->functionName},
                     {"functionIndex", arg->functionIndex}, {"arguments", argsJson}};
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaInstr>>)
            {
                nlohmann::json collJson;
                to_json(collJson, arg->collection);
                j = {{"kind", "renderVia"}, {"collection", collJson},
                     {"functionName", arg->functionName},
                     {"sep", arg->sep}, {"followedBy", arg->followedBy},
                     {"precededBy", arg->precededBy},
                     {"isBlock", arg->isBlock}, {"insertCol", arg->insertCol},
                     {"policy", arg->policy}};
            }
        }, i);
    }

    inline void from_json(const nlohmann::json &j, Instruction &i)
    {
        std::string kind = j.at("kind").get<std::string>();
        if (kind == "emit")
        {
            i = EmitInstr{j.at("text").get<std::string>()};
        }
        else if (kind == "emitExpr")
        {
            EmitExprInstr e;
            e.expr = j.at("expr").get<InstrExpr>();
            e.policy = j.value("policy", "");
            i = e;
        }
        else if (kind == "alignCell")
        {
            i = AlignCellInstr{};
        }
        else if (kind == "for")
        {
            auto f = std::make_shared<ForInstr>();
            j.at("varName").get_to(f->varName);
            f->enumeratorName = j.value("enumeratorName", "");
            f->collection = j.at("collection").get<InstrExpr>();
            f->body = j.at("body").get<std::vector<Instruction>>();
            f->sep = j.value("sep", "");
            f->followedBy = j.value("followedBy", "");
            f->precededBy = j.value("precededBy", "");
            f->isBlock = j.value("isBlock", false);
            f->insertCol = j.value("insertCol", 0);
            f->policy = j.value("policy", "");
            f->hasAlign = j.value("hasAlign", false);
            f->alignSpec = j.value("alignSpec", "");
            i = f;
        }
        else if (kind == "if")
        {
            auto n = std::make_shared<IfInstr>();
            n->condExpr = j.at("condExpr").get<InstrExpr>();
            n->negated = j.value("negated", false);
            n->thenBody = j.at("thenBody").get<std::vector<Instruction>>();
            n->elseBody = j.value("elseBody", std::vector<Instruction>{});
            n->isBlock = j.value("isBlock", false);
            n->insertCol = j.value("insertCol", 0);
            i = n;
        }
        else if (kind == "switch")
        {
            auto s = std::make_shared<SwitchInstr>();
            s->expr = j.at("expr").get<InstrExpr>();
            s->checkExhaustive = j.value("checkExhaustive", false);
            s->cases = j.at("cases").get<std::vector<CaseInstr>>();
            s->isBlock = j.value("isBlock", false);
            s->insertCol = j.value("insertCol", 0);
            s->policy = j.value("policy", "");
            i = s;
        }
        else if (kind == "call")
        {
            auto c = std::make_shared<CallInstr>();
            j.at("functionName").get_to(c->functionName);
            c->functionIndex = j.value("functionIndex", -1);
            c->arguments = j.at("arguments").get<std::vector<InstrExpr>>();
            i = c;
        }
        else if (kind == "renderVia")
        {
            auto r = std::make_shared<RenderViaInstr>();
            r->collection = j.at("collection").get<InstrExpr>();
            j.at("functionName").get_to(r->functionName);
            r->sep = j.value("sep", "");
            r->followedBy = j.value("followedBy", "");
            r->precededBy = j.value("precededBy", "");
            r->isBlock = j.value("isBlock", false);
            r->insertCol = j.value("insertCol", 0);
            r->policy = j.value("policy", "");
            i = r;
        }
    }

    inline void to_json(nlohmann::json &j, const InstructionFunction &f)
    {
        nlohmann::json bodyJson = nlohmann::json::array();
        for (const auto &i : f.body) { nlohmann::json ij; to_json(ij, i); bodyJson.push_back(ij); }
        j = {{"name", f.name}, {"body", bodyJson}, {"policy", f.policy}, {"doc", f.doc}};
        nlohmann::json paramsJson = nlohmann::json::array();
        for (const auto &p : f.params) { paramsJson.push_back(nlohmann::json{{"name", p.name}, {"type", p.type}}); }
        j["params"] = paramsJson;
    }
    inline void from_json(const nlohmann::json &j, InstructionFunction &f)
    {
        j.at("name").get_to(f.name);
        f.body = j.at("body").get<std::vector<Instruction>>();
        f.policy = j.value("policy", "");
        f.doc = j.value("doc", "");
        f.params = j.at("params").get<std::vector<ParamDef>>();
    }
}
