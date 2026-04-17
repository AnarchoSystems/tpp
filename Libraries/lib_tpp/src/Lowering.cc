#include <tpp/Lowering.h>
#include "tpp/PublicIRConverter.h"
#include <map>
#include <stdexcept>

namespace tpp::compiler
{
    // ═══════════════════════════════════════════════════════════════════
    // Type environment — maps variable names to their TypeRef
    // ═══════════════════════════════════════════════════════════════════

    struct BoundValue
    {
        TypeRef type;
        bool isRecursive = false;
        bool isOptional = false;
    };

    struct TypeEnv
    {
        const SemanticModel &semanticModel;
        std::vector<std::pair<std::string, BoundValue>> bindings;

        explicit TypeEnv(const SemanticModel &model) : semanticModel(model) {}

        void push(const std::string &name,
                  const TypeRef &type,
                  bool isRecursive = false,
                  bool isOptional = false)
        {
            bindings.emplace_back(name, BoundValue{type, isRecursive, isOptional});
        }

        void pop()
        {
            bindings.pop_back();
        }

        const BoundValue *lookup(const std::string &name) const
        {
            for (auto it = bindings.rbegin(); it != bindings.rend(); ++it)
                if (it->first == name)
                    return &it->second;
            return nullptr;
        }

        // Look up a field on a NamedType (struct).
        // Returns nullptr if the type is not a struct or the field doesn't exist.
        const FieldDef *lookupField(const TypeRef &baseType, const std::string &field) const
        {
            return semanticModel.find_field(baseType, field);
        }
    };

    // ═══════════════════════════════════════════════════════════════════
    // Expression helpers
    // ═══════════════════════════════════════════════════════════════════

    static bool isOptionalTypeRef(const TypeRef &type)
    {
        return std::holds_alternative<std::shared_ptr<OptionalType>>(type);
    }

    static TypeRef unwrapOptionalTypeRef(const TypeRef &type)
    {
        if (auto optionalType = std::get_if<std::shared_ptr<OptionalType>>(&type))
            return (*optionalType)->innerType;
        return type;
    }

    static std::string typeRefToString(const TypeRef &type)
    {
        return std::visit([&](const auto &arg) -> std::string
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, StringType>)
                return "string";
            else if constexpr (std::is_same_v<T, IntType>)
                return "int";
            else if constexpr (std::is_same_v<T, BoolType>)
                return "bool";
            else if constexpr (std::is_same_v<T, NamedType>)
                return arg.name;
            else if constexpr (std::is_same_v<T, std::shared_ptr<ListType>>)
                return "list<" + typeRefToString(arg->elementType) + ">";
            else
                return "optional<" + typeRefToString(arg->innerType) + ">";
        }, type);
    }

    struct ResolvedExpr
    {
        std::string path;
        TypeRef type;
        bool isRecursive = false;
        bool isOptional = false;
    };

    static ResolvedExpr resolvePublicExpr(const Expression &e, const TypeEnv &env)
    {
        if (auto *v = std::get_if<Variable>(&e))
        {
            if (const BoundValue *binding = env.lookup(v->name))
                return {v->name, binding->type, binding->isRecursive, binding->isOptional};
            return {v->name, TypeRef{StringType{}}, false, false};
        }

        auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&e);
        if (fa && *fa)
        {
            ResolvedExpr base = resolvePublicExpr((*fa)->base, env);
            TypeRef baseType = base.type;
            if (auto *opt = std::get_if<std::shared_ptr<OptionalType>>(&baseType))
                baseType = (*opt)->innerType;

            if (const auto *fieldDef = env.lookupField(baseType, (*fa)->field))
            {
                return {
                    base.path + "." + (*fa)->field,
                    fieldDef->type,
                    fieldDef->recursive,
                    isOptionalTypeRef(fieldDef->type)
                };
            }

            return {base.path + "." + (*fa)->field, TypeRef{StringType{}}, false, false};
        }

        return {"", TypeRef{StringType{}}, false, false};
    }

    // Resolve the type of an expression given the current type environment.
    // Returns a default TypeRef (StringType) if resolution fails — backends
    // will treat unresolved expressions as strings.
    static TypeRef resolveExprType(const Expression &e, const TypeEnv &env)
    {
        if (auto *v = std::get_if<Variable>(&e))
        {
            const BoundValue *binding = env.lookup(v->name);
            return binding ? binding->type : TypeRef{StringType{}};
        }
        auto *fa = std::get_if<std::shared_ptr<FieldAccess>>(&e);
        if (fa && *fa)
        {
            TypeRef baseType = resolveExprType((*fa)->base, env);
            // Unwrap optional for field access (tpp allows x.field inside @if x@)
            if (auto *opt = std::get_if<std::shared_ptr<OptionalType>>(&baseType))
                baseType = (*opt)->innerType;
            const FieldDef *fieldDef = env.lookupField(baseType, (*fa)->field);
            return fieldDef ? fieldDef->type : TypeRef{StringType{}};
        }
        return TypeRef{StringType{}};
    }

    static tpp::ExprInfo lowerPublicExpr(const std::string &path,
                                         const TypeRef &type,
                                         bool isRecursive,
                                         bool isOptional)
    {
        tpp::ExprInfo expr;
        expr.path = path;
        expr.type = std::make_unique<tpp::TypeKind>(tpp::to_public_type(type));
        expr.isRecursive = isRecursive;
        expr.isOptional = isOptional;
        return expr;
    }

    static tpp::ExprInfo lowerPublicExpr(const ResolvedExpr &expr)
    {
        return lowerPublicExpr(expr.path, expr.type, expr.isRecursive, expr.isOptional);
    }

    static tpp::ExprInfo lowerPublicExpr(const Expression &e, const TypeEnv &env)
    {
        return lowerPublicExpr(resolvePublicExpr(e, env));
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

    static std::vector<const TemplateFunction *> findTemplateOverloads(const std::vector<TemplateFunction> &functions,
                                                                       const std::string &name)
    {
        std::vector<const TemplateFunction *> overloads;
        for (const auto &function : functions)
        {
            if (function.name == name)
                overloads.push_back(&function);
        }
        return overloads;
    }

    static bool hasWholeEnumRenderViaOverload(const std::vector<const TemplateFunction *> &overloads,
                                              const TypeRef &enumType)
    {
        return overloads.size() == 1 &&
               overloads[0]->params.size() == 1 &&
               overloads[0]->params[0].type == enumType;
    }

    static std::optional<std::string> lowerOptionalString(const std::string &value)
    {
        if (value.empty())
            return std::nullopt;
        return value;
    }

    struct ResolvedCallArgument
    {
        std::string path;
        TypeRef type;
        bool isRecursive = false;
        bool isOptional = false;
    };

    static tpp::CallInstr lowerResolvedCall(const std::string &functionName,
                                            const std::vector<ResolvedCallArgument> &arguments,
                                            const std::vector<TemplateFunction> &functions)
    {
        tpp::CallInstr callInstr;
        callInstr.functionName = functionName;

        std::vector<TypeRef> argumentTypes;
        argumentTypes.reserve(arguments.size());
        for (const auto &argument : arguments)
        {
            callInstr.arguments.push_back(lowerPublicExpr(argument.path,
                                                         argument.type,
                                                         argument.isRecursive,
                                                         argument.isOptional));
            argumentTypes.push_back(argument.type);
        }

        callInstr.functionIndex = resolveFunctionOverloadIndex(functions, functionName, argumentTypes);
        if (callInstr.functionIndex < 0)
        {
            std::string argsText;
            for (size_t i = 0; i < argumentTypes.size(); ++i)
            {
                if (i > 0)
                    argsText += ", ";
                argsText += typeRefToString(normalizeCallArgumentType(argumentTypes[i]));
            }
            throw std::runtime_error("failed to resolve overload for function '" + functionName +
                                     "' with arguments (" + argsText + ")");
        }
        return callInstr;
    }

    static std::unique_ptr<tpp::SwitchInstr> lowerRenderViaSwitch(tpp::ExprInfo expr,
                                                                  const EnumDef &enumDef,
                                                                  const std::string &functionName,
                                                                  const std::vector<TemplateFunction> &functions,
                                                                  const std::optional<ResolvedCallArgument> &wholeEnumArgument,
                                                                  const std::string &policy,
                                                                  int &syntheticCounter)
    {
        auto switchInstr = std::make_unique<tpp::SwitchInstr>();
        switchInstr->expr = std::move(expr);
        switchInstr->isBlock = false;
        switchInstr->insertCol = 0;
        switchInstr->policy = policy;

        auto cases = std::make_unique<std::vector<tpp::CaseInstr>>();
        for (const auto &variant : enumDef.variants)
        {
            tpp::CaseInstr caseInstr;
            caseInstr.tag = variant.tag;

            std::vector<ResolvedCallArgument> callArguments;
            if (wholeEnumArgument.has_value())
            {
                callArguments.push_back(*wholeEnumArgument);
            }
            else if (variant.payload.has_value())
            {
                std::string bindingName = "_render_via_payload" + std::to_string(syntheticCounter++);
                caseInstr.payloadType = std::make_unique<tpp::TypeKind>(tpp::to_public_type(*variant.payload));
                caseInstr.bindingName = tpp::CaseBinding{bindingName, variant.recursive};
                caseInstr.payloadIsRecursive = variant.recursive;
                callArguments.push_back({bindingName, *variant.payload, false, isOptionalTypeRef(*variant.payload)});
            }
            else
            {
                caseInstr.payloadIsRecursive = false;
            }

            auto body = std::make_unique<std::vector<tpp::Instruction>>();
            body->push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, lowerResolvedCall(functionName, callArguments, functions)}});
            caseInstr.body = std::move(body);
            cases->push_back(std::move(caseInstr));
        }

        switchInstr->cases = std::move(cases);
        return switchInstr;
    }

    static std::vector<tpp::Instruction> lowerRenderViaNode(const RenderViaNode &node,
                                                            const TypeEnv &env,
                                                            const std::vector<TemplateFunction> &functions,
                                                            int &syntheticCounter)
    {
        std::vector<tpp::Instruction> out;
        ResolvedExpr collectionExpr = resolvePublicExpr(node.collectionExpr, env);
        TypeRef collectionType = unwrapOptionalTypeRef(collectionExpr.type);
        const auto *listType = std::get_if<std::shared_ptr<ListType>>(&collectionType);
        TypeRef renderElemType = listType ? (*listType)->elementType : collectionType;
        const EnumDef *renderEnum = env.semanticModel.resolve_enum(renderElemType);
        auto overloads = findTemplateOverloads(functions, node.functionName);
        bool wholeEnumOverload = renderEnum && hasWholeEnumRenderViaOverload(overloads, normalizeCallArgumentType(renderElemType));

        if (listType)
        {
            auto forInstr = std::make_unique<tpp::ForInstr>();
            std::string itemName = "_render_via_item" + std::to_string(syntheticCounter++);
            forInstr->varName = itemName;
            forInstr->collection = lowerPublicExpr(collectionExpr);

            auto body = std::make_unique<std::vector<tpp::Instruction>>();
            if (renderEnum && !wholeEnumOverload)
            {
                body->push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<5>, lowerRenderViaSwitch(lowerPublicExpr(itemName,
                                                                                                                                           renderElemType,
                                                                                                                                           false,
                                                                                                                                           isOptionalTypeRef(renderElemType)),
                                                                                                                         *renderEnum,
                                                                                                                         node.functionName,
                                                                                                                         functions,
                                                                                                                         std::nullopt,
                                                                                                                         "",
                                                                                                                         syntheticCounter)}});
            }
            else
            {
                std::vector<ResolvedCallArgument> callArguments = {{itemName, renderElemType, false, isOptionalTypeRef(renderElemType)}};
                body->push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, lowerResolvedCall(node.functionName, callArguments, functions)}});
            }

            forInstr->body = std::move(body);
            forInstr->sep = lowerOptionalString(node.sep);
            forInstr->followedBy = lowerOptionalString(node.followedBy);
            forInstr->precededBy = lowerOptionalString(node.precededBy);
            forInstr->isBlock = false;
            forInstr->insertCol = 0;
            forInstr->policy = node.policy;
            out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<3>, std::move(forInstr)}});
            return out;
        }

        if (!node.precededBy.empty())
            out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<0>, tpp::EmitInstr{node.precededBy}}});

        ResolvedExpr resolvedExpr = resolvePublicExpr(node.collectionExpr, env);
        auto expr = lowerPublicExpr(resolvedExpr);
        if (renderEnum)
        {
            std::optional<ResolvedCallArgument> wholeEnumArgument;
            if (wholeEnumOverload)
                wholeEnumArgument = ResolvedCallArgument{resolvedExpr.path,
                                                        renderElemType,
                                                        resolvedExpr.isRecursive,
                                                        resolvedExpr.isOptional};
            out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<5>, lowerRenderViaSwitch(std::move(expr), *renderEnum, node.functionName, functions, wholeEnumArgument, node.policy, syntheticCounter)}});
        }
        else
        {
            std::vector<ResolvedCallArgument> callArguments = {{resolvedExpr.path,
                                                                renderElemType,
                                                                resolvedExpr.isRecursive,
                                                                resolvedExpr.isOptional}};
            out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, lowerResolvedCall(node.functionName, callArguments, functions)}});
        }

        if (!node.followedBy.empty())
            out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<0>, tpp::EmitInstr{node.followedBy}}});

        return out;
    }

    // ═══════════════════════════════════════════════════════════════════
    // AST → public IR lowering
    // ═══════════════════════════════════════════════════════════════════

    static std::vector<tpp::Instruction> lowerPublicNodes(const std::vector<ASTNode> &nodes,
                                                          TypeEnv &env,
                                                          const std::vector<TemplateFunction> &functions,
                                                          int &syntheticCounter);

    static std::vector<tpp::Instruction> lowerPublicNodes(const std::vector<ASTNode> &nodes,
                                                          TypeEnv &env,
                                                          const std::vector<TemplateFunction> &functions,
                                                          int &syntheticCounter)
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
                    TypeRef collectionType = unwrapOptionalTypeRef(resolveExprType(arg->collectionExpr, env));
                    if (auto *listType = std::get_if<std::shared_ptr<ListType>>(&collectionType))
                        elementType = (*listType)->elementType;

                    env.push(arg->varName, elementType, false, isOptionalTypeRef(elementType));
                    if (!arg->enumeratorName.empty())
                        env.push(arg->enumeratorName, TypeRef{IntType{}});

                    auto body = lowerPublicNodes(arg->body, env, functions, syntheticCounter);

                    if (!arg->enumeratorName.empty())
                        env.pop();
                    env.pop();

                    auto forInstr = std::make_unique<tpp::ForInstr>();
                    forInstr->varName = arg->varName;
                    forInstr->enumeratorName = lowerOptionalString(arg->enumeratorName);
                    forInstr->collection = lowerPublicExpr(arg->collectionExpr, env);
                    forInstr->body = std::make_unique<std::vector<tpp::Instruction>>(std::move(body));
                    forInstr->sep = lowerOptionalString(arg->sep);
                    forInstr->followedBy = lowerOptionalString(arg->followedBy);
                    forInstr->precededBy = lowerOptionalString(arg->precededBy);
                    forInstr->isBlock = arg->isBlock;
                    forInstr->insertCol = arg->insertCol;
                    forInstr->policy = arg->policy;
                    forInstr->alignSpec = arg->alignSpec;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<3>, std::move(forInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
                {
                    auto ifInstr = std::make_unique<tpp::IfInstr>();
                    ifInstr->condExpr = lowerPublicExpr(arg->condExpr, env);
                    ifInstr->negated = arg->negated;
                    ifInstr->thenBody = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(arg->thenBody, env, functions, syntheticCounter));
                    ifInstr->elseBody = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(arg->elseBody, env, functions, syntheticCounter));
                    ifInstr->isBlock = arg->isBlock;
                    ifInstr->insertCol = arg->insertCol;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<4>, std::move(ifInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
                {
                    TypeRef exprType = resolveExprType(arg->expr, env);
                    const EnumDef *enumDef = env.semanticModel.resolve_enum(exprType);

                    auto switchInstr = std::make_unique<tpp::SwitchInstr>();
                    switchInstr->expr = lowerPublicExpr(arg->expr, env);
                    auto cases = std::make_unique<std::vector<tpp::CaseInstr>>();

                    std::map<std::string, const CaseNode *> explicitCases;
                    for (const auto &c : arg->cases)
                        explicitCases.emplace(c.tag, &c);

                    if (enumDef)
                    {
                        for (const auto &variant : enumDef->variants)
                        {
                            tpp::CaseInstr caseInstr;
                            caseInstr.tag = variant.tag;
                            caseInstr.payloadIsRecursive = variant.recursive && variant.payload.has_value();
                            if (variant.payload.has_value())
                                caseInstr.payloadType = std::make_unique<tpp::TypeKind>(tpp::to_public_type(*variant.payload));

                            auto explicitCaseIt = explicitCases.find(variant.tag);
                            const CaseNode *sourceCase = explicitCaseIt == explicitCases.end()
                                ? nullptr
                                : explicitCaseIt->second;

                            if (sourceCase && sourceCase->isSyntheticRenderCase)
                            {
                                const auto *callNode = (!sourceCase->body.empty())
                                    ? std::get_if<std::shared_ptr<FunctionCallNode>>(&sourceCase->body.front())
                                    : nullptr;
                                std::string functionName;
                                if (callNode && *callNode)
                                    functionName = (*callNode)->functionName;

                                std::vector<ResolvedCallArgument> callArguments;
                                if (variant.payload.has_value())
                                {
                                    std::string bindingName = "_switch_payload" + std::to_string(syntheticCounter++);
                                    caseInstr.bindingName = tpp::CaseBinding{bindingName, variant.recursive};
                                    callArguments.push_back({bindingName,
                                                             *variant.payload,
                                                             false,
                                                             isOptionalTypeRef(*variant.payload)});
                                }

                                auto body = std::make_unique<std::vector<tpp::Instruction>>();
                                body->push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, lowerResolvedCall(functionName, callArguments, functions)}});
                                caseInstr.body = std::move(body);
                            }
                            else if (sourceCase)
                            {
                                if (sourceCase->bindingName.empty())
                                {
                                    caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(sourceCase->body, env, functions, syntheticCounter));
                                }
                                else if (variant.payload.has_value())
                                {
                                    caseInstr.bindingName = tpp::CaseBinding{sourceCase->bindingName, variant.recursive};
                                    env.push(sourceCase->bindingName,
                                             *variant.payload,
                                             false,
                                             isOptionalTypeRef(*variant.payload));
                                    caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(sourceCase->body, env, functions, syntheticCounter));
                                    env.pop();
                                }
                                else
                                {
                                    caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(sourceCase->body, env, functions, syntheticCounter));
                                }
                            }
                            else if (arg->defaultCase)
                            {
                                caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(arg->defaultCase->body, env, functions, syntheticCounter));
                            }
                            else
                            {
                                caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>();
                            }

                            cases->push_back(std::move(caseInstr));
                        }
                    }

                    switchInstr->cases = std::move(cases);
                    switchInstr->isBlock = arg->isBlock;
                    switchInstr->insertCol = arg->insertCol;
                    switchInstr->policy = arg->policy;
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<5>, std::move(switchInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<FunctionCallNode>>)
                {
                    std::vector<ResolvedCallArgument> arguments;
                    for (const auto &argument : arg->arguments)
                    {
                        ResolvedExpr resolvedArgument = resolvePublicExpr(argument, env);
                        arguments.push_back({resolvedArgument.path,
                                             resolvedArgument.type,
                                             resolvedArgument.isRecursive,
                                             resolvedArgument.isOptional});
                    }
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, lowerResolvedCall(arg->functionName, arguments, functions)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
                {
                    auto lowered = lowerRenderViaNode(*arg, env, functions, syntheticCounter);
                    out.insert(out.end(), std::make_move_iterator(lowered.begin()), std::make_move_iterator(lowered.end()));
                }
            }, node);
        }
        return out;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Public API
    // ═══════════════════════════════════════════════════════════════════

    bool lowerToFunctions(const std::vector<TemplateFunction> &functions,
                          const SemanticModel &semanticModel,
                          std::vector<tpp::FunctionDef> &out,
                          bool includeSourceRanges)
    {
        out.clear();
        out.reserve(functions.size());

        for (const auto &fn : functions)
        {
            TypeEnv env(semanticModel);
            int syntheticCounter = 0;
            for (const auto &param : fn.params)
                env.push(param.name, param.type, false, isOptionalTypeRef(param.type));

            tpp::FunctionDef functionDef;
            functionDef.name = fn.name;
            for (const auto &param : fn.params)
            {
                tpp::ParamDef paramDef;
                paramDef.name = param.name;
                paramDef.type = std::make_unique<tpp::TypeKind>(tpp::to_public_type(param.type));
                functionDef.params.push_back(std::move(paramDef));
            }
            functionDef.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(fn.body, env, functions, syntheticCounter));
            functionDef.policy = fn.policy;
            functionDef.doc = fn.doc;
            functionDef.sourceRange = tpp::to_public_range(fn.sourceRange, includeSourceRanges);

            out.push_back(std::move(functionDef));
        }

        return true;
    }
}
