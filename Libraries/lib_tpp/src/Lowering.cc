#include <tpp/Lowering.h>
#include <map>
#include <stdexcept>

namespace tpp::compiler
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

    static tpp::TypeKind lowerPublicType(const TypeRef &type)
    {
        return std::visit([](auto &&arg) -> tpp::TypeKind
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, StringType>)
                return tpp::TypeKind{tpp::TypeKind::Value{std::in_place_index<0>}};
            else if constexpr (std::is_same_v<T, IntType>)
                return tpp::TypeKind{tpp::TypeKind::Value{std::in_place_index<1>}};
            else if constexpr (std::is_same_v<T, BoolType>)
                return tpp::TypeKind{tpp::TypeKind::Value{std::in_place_index<2>}};
            else if constexpr (std::is_same_v<T, NamedType>)
                return tpp::TypeKind{tpp::TypeKind::Value{std::in_place_index<3>, arg.name}};
            else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
                return tpp::TypeKind{tpp::TypeKind::Value{std::in_place_index<4>,
                    std::make_unique<tpp::TypeKind>(lowerPublicType(arg->elementType))}};
            else if constexpr (std::is_same_v<T, std::shared_ptr<OptionalType>>)
                return tpp::TypeKind{tpp::TypeKind::Value{std::in_place_index<5>,
                    std::make_unique<tpp::TypeKind>(lowerPublicType(arg->innerType))}};
            else
                return tpp::TypeKind{tpp::TypeKind::Value{std::in_place_index<0>}};
        }, type);
    }

    static tpp::ExprInfo lowerPublicExpr(const Expression &e, const TypeEnv &env)
    {
        tpp::ExprInfo expr;
        expr.path = flattenExpr(e);
        expr.type = std::make_unique<tpp::TypeKind>(lowerPublicType(resolveExprType(e, env)));
        return expr;
    }

    static std::optional<tpp::SourceRange> lowerPublicRange(const Range &range,
                                                            bool includeSourceRanges)
    {
        if (!includeSourceRanges)
            return std::nullopt;

        tpp::SourceRange sourceRange;
        sourceRange.start.line = range.start.line;
        sourceRange.start.character = range.start.character;
        sourceRange.end.line = range.end.line;
        sourceRange.end.character = range.end.character;
        return sourceRange;
    }

    static TypeRef normalizeCallArgumentType(const TypeRef &type)
    {
        if (auto optionalType = std::get_if<std::shared_ptr<OptionalType>>(&type))
            return (*optionalType)->innerType;
        return type;
    }

    static int resolveFunctionOverloadIndex(const std::vector<TemplateFunction> &functions,
                                            const std::string &name,
                                            const std::vector<TypeRef> &arguments)
    {
        for (size_t i = 0; i < functions.size(); ++i)
        {
            const auto &function = functions[i];
            if (function.name != name || function.params.size() != arguments.size())
                continue;

            bool matches = true;
            for (size_t argIndex = 0; argIndex < arguments.size(); ++argIndex)
            {
                if (!(function.params[argIndex].type == normalizeCallArgumentType(arguments[argIndex])))
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
                return static_cast<int>(i);
        }

        return -1;
    }

    // ═══════════════════════════════════════════════════════════════════
    // AST → public IR lowering
    // ═══════════════════════════════════════════════════════════════════

    static std::vector<tpp::Instruction> lowerPublicNodes(const std::vector<ASTNode> &nodes,
                                                          TypeEnv &env,
                                                          const std::vector<TemplateFunction> &functions,
                                                          const std::map<std::string, int> &funcIndex);

    static std::vector<tpp::Instruction> lowerPublicNodes(const std::vector<ASTNode> &nodes,
                                                          TypeEnv &env,
                                                          const std::vector<TemplateFunction> &functions,
                                                          const std::map<std::string, int> &funcIndex)
    {
        std::vector<tpp::Instruction> out;
        for (const auto &node : nodes)
        {
            std::visit([&](auto &&arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, TextNode>)
                {
                    if (!out.empty() && std::holds_alternative<tpp::EmitInstr>(out.back().value))
                        std::get<tpp::EmitInstr>(out.back().value).text += arg.text;
                    else
                        out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<0>, tpp::EmitInstr{arg.text}}});
                }
                else if constexpr (std::is_same_v<T, InterpolationNode>)
                {
                    tpp::EmitExprInstr exprInstr;
                    exprInstr.expr = lowerPublicExpr(arg.expr, env);
                    exprInstr.policy = arg.policy;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<1>, std::move(exprInstr)}});
                }
                else if constexpr (std::is_same_v<T, AlignmentCellNode>)
                {
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<2>}});
                }
                else if constexpr (std::is_same_v<T, CommentNode>)
                {
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<ForNode>>)
                {
                    TypeRef elementType{StringType{}};
                    TypeRef collectionType = resolveExprType(arg->collectionExpr, env);
                    if (auto *listType = std::get_if<std::shared_ptr<ListType>>(&collectionType))
                        elementType = (*listType)->elementType;

                    env.push(arg->varName, elementType);
                    if (!arg->enumeratorName.empty())
                        env.push(arg->enumeratorName, TypeRef{IntType{}});

                    auto body = lowerPublicNodes(arg->body, env, functions, funcIndex);

                    if (!arg->enumeratorName.empty())
                        env.pop();
                    env.pop();

                    auto forInstr = std::make_unique<tpp::ForInstr>();
                    forInstr->varName = arg->varName;
                    forInstr->enumeratorName = arg->enumeratorName;
                    forInstr->collection = lowerPublicExpr(arg->collectionExpr, env);
                    forInstr->body = std::make_unique<std::vector<tpp::Instruction>>(std::move(body));
                    forInstr->sep = arg->sep;
                    forInstr->followedBy = arg->followedBy;
                    forInstr->precededBy = arg->precededBy;
                    forInstr->isBlock = arg->isBlock;
                    forInstr->insertCol = arg->insertCol;
                    forInstr->policy = arg->policy;
                    forInstr->hasAlign = arg->hasAlign;
                    forInstr->alignSpec = arg->alignSpec;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<3>, std::move(forInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
                {
                    auto ifInstr = std::make_unique<tpp::IfInstr>();
                    ifInstr->condExpr = lowerPublicExpr(arg->condExpr, env);
                    ifInstr->negated = arg->negated;
                    ifInstr->thenBody = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(arg->thenBody, env, functions, funcIndex));
                    ifInstr->elseBody = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(arg->elseBody, env, functions, funcIndex));
                    ifInstr->isBlock = arg->isBlock;
                    ifInstr->insertCol = arg->insertCol;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<4>, std::move(ifInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
                {
                    TypeRef exprType = resolveExprType(arg->expr, env);
                    const EnumDef *enumDef = nullptr;
                    if (auto *namedType = std::get_if<NamedType>(&exprType))
                    {
                        auto it = env.types.nameIndex.find(namedType->name);
                        if (it != env.types.nameIndex.end() && it->second.kind == TypeKind::Enum)
                            enumDef = &env.types.enums[it->second.index];
                    }

                    auto switchInstr = std::make_unique<tpp::SwitchInstr>();
                    switchInstr->expr = lowerPublicExpr(arg->expr, env);
                    auto cases = std::make_unique<std::vector<tpp::CaseInstr>>();

                    for (const auto &c : arg->cases)
                    {
                        tpp::CaseInstr caseInstr;
                        caseInstr.tag = c.tag;

                        std::optional<TypeRef> payloadType;
                        if (enumDef)
                        {
                            for (const auto &variant : enumDef->variants)
                            {
                                if (variant.tag == c.tag)
                                {
                                    payloadType = variant.payload;
                                    break;
                                }
                            }
                        }

                        if (payloadType.has_value())
                            caseInstr.payloadType = std::make_unique<tpp::TypeKind>(lowerPublicType(*payloadType));

                        if (c.isSyntheticRenderCase)
                        {
                            tpp::CallInstr callInstr;
                            if (!c.body.empty())
                            {
                                if (auto *callNode = std::get_if<std::shared_ptr<FunctionCallNode>>(&c.body.front()))
                                {
                                    if (*callNode)
                                        callInstr.functionName = (*callNode)->functionName;
                                }
                            }

                            std::vector<TypeRef> argumentTypes;
                            if (payloadType.has_value())
                            {
                                tpp::ExprInfo payloadExpr;
                                payloadExpr.path = switchInstr->expr.path + "." + c.tag;
                                payloadExpr.type = std::make_unique<tpp::TypeKind>(lowerPublicType(*payloadType));
                                callInstr.arguments.push_back(std::move(payloadExpr));
                                argumentTypes.push_back(*payloadType);
                            }
                            callInstr.functionIndex = resolveFunctionOverloadIndex(functions, callInstr.functionName, argumentTypes);

                            auto body = std::make_unique<std::vector<tpp::Instruction>>();
                            body->push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, std::move(callInstr)}});
                            caseInstr.body = std::move(body);
                        }
                        else
                        {
                            caseInstr.bindingName = c.bindingName;

                            if (!c.bindingName.empty() && payloadType.has_value())
                                env.push(c.bindingName, *payloadType);
                            else if (!c.bindingName.empty())
                                env.push(c.bindingName, TypeRef{StringType{}});

                            caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(c.body, env, functions, funcIndex));

                            if (!c.bindingName.empty())
                                env.pop();
                        }

                        cases->push_back(std::move(caseInstr));
                    }

                    if (arg->defaultCase)
                    {
                        const auto &c = *arg->defaultCase;
                        tpp::CaseInstr caseInstr;
                        caseInstr.tag = c.tag;
                        caseInstr.bindingName = c.bindingName;

                        caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(c.body, env, functions, funcIndex));
                        switchInstr->defaultCase = std::make_unique<tpp::CaseInstr>(std::move(caseInstr));
                    }

                    switchInstr->cases = std::move(cases);
                    switchInstr->isBlock = arg->isBlock;
                    switchInstr->insertCol = arg->insertCol;
                    switchInstr->policy = arg->policy;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<5>, std::move(switchInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>)
                {
                    tpp::CallInstr callInstr;
                    callInstr.functionName = arg->functionName;
                    std::vector<TypeRef> argumentTypes;
                    for (const auto &argument : arg->arguments)
                    {
                        argumentTypes.push_back(resolveExprType(argument, env));
                        callInstr.arguments.push_back(lowerPublicExpr(argument, env));
                    }
                    callInstr.functionIndex = resolveFunctionOverloadIndex(functions, arg->functionName, argumentTypes);
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, std::move(callInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
                {
                    tpp::RenderViaInstr renderViaInstr;
                    renderViaInstr.collection = lowerPublicExpr(arg->collectionExpr, env);
                    renderViaInstr.functionName = arg->functionName;
                    renderViaInstr.sep = arg->sep;
                    renderViaInstr.followedBy = arg->followedBy;
                    renderViaInstr.precededBy = arg->precededBy;
                    renderViaInstr.isBlock = arg->isBlock;
                    renderViaInstr.insertCol = arg->insertCol;
                    renderViaInstr.policy = arg->policy;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<7>, std::move(renderViaInstr)}});
                }
            }, node);
        }
        return out;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Public API
    // ═══════════════════════════════════════════════════════════════════

    bool lowerToFunctions(const std::vector<TemplateFunction> &functions,
                          const TypeRegistry &types,
                          std::vector<tpp::FunctionDef> &out,
                          bool includeSourceRanges,
                          std::string &error)
    {
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
            for (const auto &param : fn.params)
                env.push(param.name, param.type);

            tpp::FunctionDef functionDef;
            functionDef.name = fn.name;
            for (const auto &param : fn.params)
            {
                tpp::ParamDef paramDef;
                paramDef.name = param.name;
                paramDef.type = std::make_unique<tpp::TypeKind>(lowerPublicType(param.type));
                functionDef.params.push_back(std::move(paramDef));
            }
            functionDef.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(fn.body, env, functions, funcIndex));
            functionDef.policy = fn.policy;
            functionDef.doc = fn.doc;
            functionDef.sourceRange = lowerPublicRange(fn.sourceRange, includeSourceRanges);

            out.push_back(std::move(functionDef));
        }

        return true;
    }
}
