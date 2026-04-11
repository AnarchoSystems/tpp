#include <tpp/Lowering.h>
#include <map>
#include <stdexcept>

namespace tpp
{
    // ═══════════════════════════════════════════════════════════════════
    // Type environment — maps variable names to their TypeRef
    // ═══════════════════════════════════════════════════════════════════

    struct TypeEnv
    {
        const TypeRegistry &types;
        std::vector<std::pair<std::string, TypeRef>> bindings;

        explicit TypeEnv(const TypeRegistry &t) : types(t) {}

        void push(const std::string &name, const TypeRef &type)
        {
            bindings.emplace_back(name, type);
        }

        void pop()
        {
            bindings.pop_back();
        }

        const TypeRef *lookup(const std::string &name) const
        {
            for (auto it = bindings.rbegin(); it != bindings.rend(); ++it)
                if (it->first == name)
                    return &it->second;
            return nullptr;
        }

        // Look up a field on a NamedType (struct).
        // Returns nullptr if the type is not a struct or the field doesn't exist.
        const TypeRef *lookupField(const TypeRef &baseType, const std::string &field) const
        {
            const NamedType *nt = std::get_if<NamedType>(&baseType);
            if (!nt)
                return nullptr;
            auto it = types.nameIndex.find(nt->name);
            if (it == types.nameIndex.end())
                return nullptr;
            if (it->second.kind == TypeKind::Struct)
            {
                const StructDef &sd = types.structs[it->second.index];
                for (const auto &f : sd.fields)
                    if (f.name == field)
                        return &f.type;
            }
            return nullptr;
        }
    };

    // ═══════════════════════════════════════════════════════════════════
    // Expression helpers
    // ═══════════════════════════════════════════════════════════════════

    // Flatten an AST Expression to a dotted path string.
    static std::string flattenExpr(const Expression &e)
    {
        if (auto *v = std::get_if<Variable>(&e))
            return v->name;
        auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&e);
        if (fa && *fa)
            return flattenExpr((*fa)->base) + "." + (*fa)->field;
        return "";
    }

    // Resolve the type of an expression given the current type environment.
    // Returns a default TypeRef (StringType) if resolution fails — backends
    // will treat unresolved expressions as strings.
    static TypeRef resolveExprType(const Expression &e, const TypeEnv &env)
    {
        if (auto *v = std::get_if<Variable>(&e))
        {
            const TypeRef *t = env.lookup(v->name);
            return t ? *t : TypeRef{StringType{}};
        }
        auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&e);
        if (fa && *fa)
        {
            TypeRef baseType = resolveExprType((*fa)->base, env);
            // Unwrap optional for field access (tpp allows x.field inside @if x@)
            if (auto *opt = std::get_if<std::shared_ptr<OptionalType>>(&baseType))
                baseType = (*opt)->innerType;
            const TypeRef *fieldType = env.lookupField(baseType, (*fa)->field);
            return fieldType ? *fieldType : TypeRef{StringType{}};
        }
        return TypeRef{StringType{}};
    }

    static InstrExpr lowerExpr(const Expression &e, const TypeEnv &env)
    {
        return InstrExpr{flattenExpr(e), resolveExprType(e, env)};
    }

    // ═══════════════════════════════════════════════════════════════════
    // AST → Instruction lowering
    // ═══════════════════════════════════════════════════════════════════

    static std::vector<Instruction> lowerNodes(const std::vector<ASTNode> &nodes, TypeEnv &env,
                                               const std::map<std::string, int> &funcIndex);

    static std::vector<Instruction> lowerNodes(const std::vector<ASTNode> &nodes, TypeEnv &env,
                                               const std::map<std::string, int> &funcIndex)
    {
        std::vector<Instruction> out;
        for (const auto &node : nodes)
        {
            std::visit([&](auto &&arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, TextNode>)
                {
                    // Merge adjacent text emissions
                    if (!out.empty() && std::holds_alternative<EmitInstr>(out.back()))
                        std::get<EmitInstr>(out.back()).text += arg.text;
                    else
                        out.push_back(EmitInstr{arg.text});
                }
                else if constexpr (std::is_same_v<T, InterpolationNode>)
                {
                    out.push_back(EmitExprInstr{lowerExpr(arg.expr, env), arg.policy});
                }
                else if constexpr (std::is_same_v<T, AlignmentCellNode>)
                {
                    out.push_back(AlignCellInstr{});
                }
                else if constexpr (std::is_same_v<T, CommentNode>)
                {
                    // Comments produce no output — skip
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>)
                {
                    InstrExpr collExpr = lowerExpr(arg->collectionExpr, env);

                    // Determine element type from the collection's resolved type
                    TypeRef elemType{StringType{}};
                    if (auto *lt = std::get_if<std::shared_ptr<ListType>>(&collExpr.resolvedType))
                        elemType = (*lt)->elementType;

                    // Push loop variable binding and lower body
                    env.push(arg->varName, elemType);
                    if (!arg->enumeratorName.empty())
                        env.push(arg->enumeratorName, TypeRef{IntType{}});

                    auto body = lowerNodes(arg->body, env, funcIndex);

                    if (!arg->enumeratorName.empty())
                        env.pop();
                    env.pop();

                    auto f = std::make_shared<ForInstr>();
                    f->varName = arg->varName;
                    f->enumeratorName = arg->enumeratorName;
                    f->collection = std::move(collExpr);
                    f->body = std::move(body);
                    f->sep = arg->sep;
                    f->followedBy = arg->followedBy;
                    f->precededBy = arg->precededBy;
                    f->isBlock = arg->isBlock;
                    f->insertCol = arg->insertCol;
                    f->policy = arg->policy;
                    f->hasAlign = arg->hasAlign;
                    f->alignSpec = arg->alignSpec;
                    out.push_back(std::move(f));
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
                {
                    InstrExpr condExpr = lowerExpr(arg->condExpr, env);
                    auto thenBody = lowerNodes(arg->thenBody, env, funcIndex);
                    auto elseBody = lowerNodes(arg->elseBody, env, funcIndex);

                    auto n = std::make_shared<IfInstr>();
                    n->condExpr = std::move(condExpr);
                    n->negated = arg->negated;
                    n->thenBody = std::move(thenBody);
                    n->elseBody = std::move(elseBody);
                    n->isBlock = arg->isBlock;
                    n->insertCol = arg->insertCol;
                    out.push_back(std::move(n));
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
                {
                    InstrExpr switchExpr = lowerExpr(arg->expr, env);

                    // Look up enum type for case payload resolution
                    const EnumDef *enumDef = nullptr;
                    if (auto *nt = std::get_if<NamedType>(&switchExpr.resolvedType))
                    {
                        auto it = env.types.nameIndex.find(nt->name);
                        if (it != env.types.nameIndex.end() && it->second.kind == TypeKind::Enum)
                            enumDef = &env.types.enums[it->second.index];
                    }

                    std::vector<CaseInstr> cases;
                    for (const auto &c : arg->cases)
                    {
                        CaseInstr ci;
                        ci.tag = c.tag;
                        ci.bindingName = c.bindingName;

                        // Resolve payload type from enum definition
                        if (enumDef)
                        {
                            for (const auto &v : enumDef->variants)
                            {
                                if (v.tag == c.tag)
                                {
                                    ci.payloadType = v.payload;
                                    break;
                                }
                            }
                        }

                        // Push case binding and lower body
                        if (!c.bindingName.empty() && ci.payloadType.has_value())
                            env.push(c.bindingName, *ci.payloadType);
                        else if (!c.bindingName.empty())
                            env.push(c.bindingName, TypeRef{StringType{}});

                        ci.body = lowerNodes(c.body, env, funcIndex);

                        if (!c.bindingName.empty())
                            env.pop();

                        cases.push_back(std::move(ci));
                    }

                    auto s = std::make_shared<SwitchInstr>();
                    s->expr = std::move(switchExpr);
                    s->checkExhaustive = arg->checkExhaustive;
                    s->cases = std::move(cases);
                    s->isBlock = arg->isBlock;
                    s->insertCol = arg->insertCol;
                    s->policy = arg->policy;
                    out.push_back(std::move(s));
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>)
                {
                    auto ci = std::make_shared<CallInstr>();
                    ci->functionName = arg->functionName;
                    auto it = funcIndex.find(arg->functionName);
                    ci->functionIndex = (it != funcIndex.end()) ? it->second : -1;
                    for (const auto &a : arg->arguments)
                        ci->arguments.push_back(lowerExpr(a, env));
                    out.push_back(std::move(ci));
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
                {
                    auto ri = std::make_shared<RenderViaInstr>();
                    ri->collection = lowerExpr(arg->collectionExpr, env);
                    ri->functionName = arg->functionName;
                    ri->sep = arg->sep;
                    ri->followedBy = arg->followedBy;
                    ri->precededBy = arg->precededBy;
                    ri->isBlock = arg->isBlock;
                    ri->insertCol = arg->insertCol;
                    ri->policy = arg->policy;
                    out.push_back(std::move(ri));
                }
            }, node);
        }
        return out;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Public API
    // ═══════════════════════════════════════════════════════════════════

    bool lowerToInstructions(const std::vector<TemplateFunction> &functions,
                             const TypeRegistry &types,
                             std::vector<InstructionFunction> &out,
                             std::string &error)
    {
        // Build function name → first index map (mirrors runtime functionIndex)
        std::map<std::string, int> funcIndex;
        for (size_t i = 0; i < functions.size(); ++i)
        {
            if (funcIndex.find(functions[i].name) == funcIndex.end())
                funcIndex[functions[i].name] = (int)i;
        }

        out.clear();
        out.reserve(functions.size());

        for (const auto &fn : functions)
        {
            TypeEnv env(types);

            // Push function parameter bindings
            for (const auto &p : fn.params)
                env.push(p.name, p.type);

            InstructionFunction ifn;
            ifn.name = fn.name;
            ifn.params = fn.params;
            ifn.policy = fn.policy;
            ifn.doc = fn.doc;
            ifn.body = lowerNodes(fn.body, env, funcIndex);

            out.push_back(std::move(ifn));
        }

        return true;
    }
}
