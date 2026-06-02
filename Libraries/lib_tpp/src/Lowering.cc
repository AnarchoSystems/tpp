#include "tpp/AST.h"
#include "tpp/PublicIRConverter.h"
#include "tpp/SemanticModel.h"
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

    static bool isBoolTypeRef(const TypeRef &type)
    {
        return std::holds_alternative<BoolType>(type);
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

    static std::vector<tpp::ExprPathSegment> lowerPublicExprSegments(const std::string &path)
    {
        std::vector<tpp::ExprPathSegment> segments;

        std::size_t pos = path.find('.');
        while (pos != std::string::npos)
        {
            const std::size_t next = path.find('.', pos + 1);
            const std::size_t fieldStart = pos + 1;
            const std::size_t fieldLen = (next == std::string::npos)
                ? path.size() - fieldStart
                : next - fieldStart;

            if (fieldLen > 0)
            {
                tpp::ExprPathSegment segment;
                segment.field = path.substr(fieldStart, fieldLen);
                segments.push_back(std::move(segment));
            }

            pos = next;
        }

        return segments;
    }

    static tpp::ExprInfo lowerPublicExpr(const std::string &path,
                                         const TypeRef &type,
                                         bool isRecursive,
                                         bool isOptional)
    {
        tpp::ExprInfo expr;
        const std::size_t dot = path.find('.');
        expr.bindingName = (dot == std::string::npos) ? path : path.substr(0, dot);
        expr.segments = lowerPublicExprSegments(path);
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

    static tpp::IfConditionKind lowerPublicConditionKind(const TypeRef &type)
    {
        tpp::IfConditionKind conditionKind;
        if (isBoolTypeRef(type))
            conditionKind.value.emplace<0>();
        else
            conditionKind.value.emplace<1>();
        return conditionKind;
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

    static tpp::Instruction makeBeginCapturedBlockInstruction(std::optional<int> blockIndentInParentBlock)
    {
        tpp::BeginCapturedBlockInstr beginCapturedBlockInstr;
        beginCapturedBlockInstr.blockIndentInParentBlock = std::move(blockIndentInParentBlock);
        return tpp::Instruction{tpp::Instruction::Value{std::in_place_index<7>, std::move(beginCapturedBlockInstr)}};
    }

    static tpp::Instruction makeEmitCapturedBlockInstruction()
    {
        return tpp::Instruction{tpp::Instruction::Value{std::in_place_index<8>}};
    }

    static std::vector<tpp::Instruction> wrapCapturedBlock(std::vector<tpp::Instruction> body,
                                                           std::optional<int> blockIndentInParentBlock)
    {
        std::vector<tpp::Instruction> wrapped;
        wrapped.reserve(body.size() + 2);

        wrapped.push_back(makeBeginCapturedBlockInstruction(std::move(blockIndentInParentBlock)));

        for (auto &instr : body)
            wrapped.push_back(std::move(instr));

        wrapped.push_back(makeEmitCapturedBlockInstruction());
        return wrapped;
    }

    struct ResolvedCallArgument
    {
        std::string path;
        TypeRef type;
        bool isRecursive = false;
        bool isOptional = false;
    };

    static const FunctionCallNode *findSyntheticRenderCall(const std::vector<ASTNode> &body)
    {
        if (body.empty())
            return nullptr;

        if (const auto *callNode = std::get_if<std::shared_ptr<FunctionCallNode>>(&body.front()))
            return (callNode && *callNode) ? callNode->get() : nullptr;

        if (const auto *indentNode = std::get_if<std::shared_ptr<IndentNode>>(&body.front()))
            return (indentNode && *indentNode) ? findSyntheticRenderCall((*indentNode)->body) : nullptr;

        return nullptr;
    }

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
                                                                  int &syntheticCounter,
                                                                  const Range &sourceRange,
                                                                  bool includeSourceRanges)
    {
        auto switchInstr = std::make_unique<tpp::SwitchInstr>();
        switchInstr->expr = std::move(expr);
        switchInstr->policy = policy;
        switchInstr->sourceRange = tpp::to_public_range(sourceRange, includeSourceRanges);

        auto cases = std::make_unique<std::vector<tpp::CaseInstr>>();
        for (size_t variantIndex = 0; variantIndex < enumDef.variants.size(); ++variantIndex)
        {
            const auto &variant = enumDef.variants[variantIndex];
            tpp::CaseInstr caseInstr;
            caseInstr.tag = variant.tag;
            caseInstr.variantIndex = static_cast<int>(variantIndex);

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
                                                            int &syntheticCounter,
                                                            bool includeSourceRanges)
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
            forInstr->elementType = std::make_unique<tpp::TypeKind>(tpp::to_public_type(renderElemType));
            forInstr->sourceRange = tpp::to_public_range(node.sourceRange, includeSourceRanges);

            std::vector<tpp::Instruction> body;
            if (renderEnum && !wholeEnumOverload)
            {
                body.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<5>, lowerRenderViaSwitch(lowerPublicExpr(itemName,
                                                                                                                                      renderElemType,
                                                                                                                                      false,
                                                                                                                                      isOptionalTypeRef(renderElemType)),
                                                                                                                    *renderEnum,
                                                                                                                    node.functionName,
                                                                                                                    functions,
                                                                                                                    std::nullopt,
                                                                                                                    "",
                                                                                                                    syntheticCounter,
                                                                                                                    node.sourceRange,
                                                                                                                    includeSourceRanges)}});
            }
            else
            {
                std::vector<ResolvedCallArgument> callArguments = {{itemName, renderElemType, false, isOptionalTypeRef(renderElemType)}};
                body.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, lowerResolvedCall(node.functionName, callArguments, functions)}});
            }

            forInstr->body = std::make_unique<std::vector<tpp::Instruction>>(std::move(body));
            forInstr->sep = lowerOptionalString(node.sep);
            forInstr->followedBy = lowerOptionalString(node.followedBy);
            forInstr->precededBy = lowerOptionalString(node.precededBy);
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
            out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<5>, lowerRenderViaSwitch(std::move(expr), *renderEnum, node.functionName, functions, wholeEnumArgument, node.policy, syntheticCounter, node.sourceRange, includeSourceRanges)}});
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
                                                          int &syntheticCounter,
                                                          bool includeSourceRanges);

    static std::vector<tpp::Instruction> lowerPublicNodes(const std::vector<ASTNode> &nodes,
                                                          TypeEnv &env,
                                                          const std::vector<TemplateFunction> &functions,
                                                          int &syntheticCounter,
                                                          bool includeSourceRanges)
    {
        std::vector<tpp::Instruction> out;
        for (const auto &node : nodes)
        {
            std::visit([&](auto &&arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, TextNode>)
                {
                    if (!out.empty() &&
                        std::holds_alternative<tpp::EmitInstr>(out.back().value) &&
                        !arg.sourceRange.start.line && !arg.sourceRange.start.character &&
                        !arg.sourceRange.end.line && !arg.sourceRange.end.character &&
                        !std::get<tpp::EmitInstr>(out.back().value).sourceRange.has_value())
                    {
                        std::get<tpp::EmitInstr>(out.back().value).text += arg.text;
                    }
                    else
                    {
                        tpp::EmitInstr emitInstr;
                        emitInstr.text = arg.text;
                        if (arg.sourceRange.start.line != 0 || arg.sourceRange.start.character != 0 ||
                            arg.sourceRange.end.line != 0 || arg.sourceRange.end.character != 0)
                        {
                            emitInstr.sourceRange = tpp::to_public_range(arg.sourceRange, includeSourceRanges);
                        }
                        out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<0>, std::move(emitInstr)}});
                    }
                }
                else if constexpr (std::is_same_v<T, InterpolationNode>)
                {
                    tpp::EmitExprInstr exprInstr;
                    exprInstr.expr = lowerPublicExpr(arg.expr, env);
                    exprInstr.policy = arg.policy;
                    exprInstr.sourceRange = tpp::to_public_range(arg.sourceRange, includeSourceRanges);
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<1>, std::move(exprInstr)}});
                }
                else if constexpr (std::is_same_v<T, AlignmentCellNode>)
                {
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<2>}});
                }
                else if constexpr (std::is_same_v<T, CommentNode>)
                {
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<IndentNode>>)
                {
                    auto body = lowerPublicNodes(arg->body, env, functions, syntheticCounter, includeSourceRanges);

                    if (arg->wrapSingleForBody &&
                        body.size() == 1 &&
                        std::holds_alternative<std::unique_ptr<tpp::ForInstr>>(body.front().value))
                    {
                        auto &forInstr = std::get<std::unique_ptr<tpp::ForInstr>>(body.front().value);
                        std::vector<tpp::Instruction> forBody;
                        if (forInstr->body)
                            forBody = std::move(*forInstr->body);
                        const std::optional<int> blockIndentInParentBlock = arg->amount > 0 ? std::make_optional(arg->amount) : std::nullopt;
                        forInstr->body = std::make_unique<std::vector<tpp::Instruction>>(wrapCapturedBlock(std::move(forBody), blockIndentInParentBlock));
                        out.push_back(std::move(body.front()));
                    }
                    else
                    {
                        const std::optional<int> blockIndentInParentBlock = arg->amount > 0 ? std::make_optional(arg->amount) : std::nullopt;
                        auto wrapped = wrapCapturedBlock(std::move(body), blockIndentInParentBlock);
                        out.insert(out.end(),
                                   std::make_move_iterator(wrapped.begin()),
                                   std::make_move_iterator(wrapped.end()));
                    }
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

                    auto body = lowerPublicNodes(arg->body, env, functions, syntheticCounter, includeSourceRanges);

                    if (!arg->enumeratorName.empty())
                        env.pop();
                    env.pop();

                    auto forInstr = std::make_unique<tpp::ForInstr>();
                    forInstr->varName = arg->varName;
                    forInstr->enumeratorName = lowerOptionalString(arg->enumeratorName);
                    forInstr->collection = lowerPublicExpr(arg->collectionExpr, env);
                    forInstr->elementType = std::make_unique<tpp::TypeKind>(tpp::to_public_type(elementType));
                    forInstr->body = std::make_unique<std::vector<tpp::Instruction>>(std::move(body));
                    forInstr->sep = lowerOptionalString(arg->sep);
                    forInstr->followedBy = lowerOptionalString(arg->followedBy);
                    forInstr->precededBy = lowerOptionalString(arg->precededBy);
                    forInstr->policy = arg->policy;
                    forInstr->alignSpec = arg->alignSpec;
                    forInstr->sourceRange = tpp::to_public_range(arg->sourceRange, includeSourceRanges);
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<3>, std::move(forInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<IfNode>>)
                {
                    auto ifInstr = std::make_unique<tpp::IfInstr>();
                    ifInstr->condExpr = lowerPublicExpr(arg->condExpr, env);
                    ifInstr->conditionKind = lowerPublicConditionKind(resolveExprType(arg->condExpr, env));
                    ifInstr->negated = arg->negated;
                    auto thenBody = lowerPublicNodes(arg->thenBody, env, functions, syntheticCounter, includeSourceRanges);
                    auto elseBody = lowerPublicNodes(arg->elseBody, env, functions, syntheticCounter, includeSourceRanges);
                    ifInstr->thenBody = std::make_unique<std::vector<tpp::Instruction>>(std::move(thenBody));
                    ifInstr->elseBody = std::make_unique<std::vector<tpp::Instruction>>(std::move(elseBody));
                    ifInstr->sourceRange = tpp::to_public_range(arg->sourceRange, includeSourceRanges);
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<4>, std::move(ifInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<SwitchNode>>)
                {
                    TypeRef exprType = resolveExprType(arg->expr, env);
                    const EnumDef *enumDef = env.semanticModel.resolve_enum(exprType);

                    auto switchInstr = std::make_unique<tpp::SwitchInstr>();
                    switchInstr->expr = lowerPublicExpr(arg->expr, env);
                    switchInstr->sourceRange = tpp::to_public_range(arg->sourceRange, includeSourceRanges);
                    auto cases = std::make_unique<std::vector<tpp::CaseInstr>>();

                    std::map<std::string, const CaseNode *> explicitCases;
                    for (const auto &c : arg->cases)
                        explicitCases.emplace(c.tag, &c);

                    if (enumDef)
                    {
                        for (size_t variantIndex = 0; variantIndex < enumDef->variants.size(); ++variantIndex)
                        {
                            const auto &variant = enumDef->variants[variantIndex];
                            tpp::CaseInstr caseInstr;
                            caseInstr.tag = variant.tag;
                            caseInstr.payloadIsRecursive = variant.recursive && variant.payload.has_value();
                            caseInstr.variantIndex = static_cast<int>(variantIndex);
                            if (variant.payload.has_value())
                                caseInstr.payloadType = std::make_unique<tpp::TypeKind>(tpp::to_public_type(*variant.payload));

                            auto explicitCaseIt = explicitCases.find(variant.tag);
                            const CaseNode *sourceCase = explicitCaseIt == explicitCases.end()
                                ? nullptr
                                : explicitCaseIt->second;
                            const CaseNode *mappedCase = sourceCase
                                ? sourceCase
                                : (arg->defaultCase ? &*arg->defaultCase : nullptr);

                            caseInstr.sourceRange = mappedCase
                                ? tpp::to_public_range(mappedCase->sourceRange, includeSourceRanges)
                                : std::nullopt;

                            std::vector<tpp::Instruction> loweredCaseBody;

                            if (sourceCase && sourceCase->isSyntheticRenderCase)
                            {
                                std::string functionName;
                                if (const auto *callNode = findSyntheticRenderCall(sourceCase->body))
                                    functionName = callNode->functionName;

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

                                                        loweredCaseBody.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, lowerResolvedCall(functionName, callArguments, functions)}});
                            }
                            else if (sourceCase)
                            {
                                if (sourceCase->bindingName.empty())
                                {
                                                            loweredCaseBody = lowerPublicNodes(sourceCase->body, env, functions, syntheticCounter, includeSourceRanges);
                                }
                                else if (variant.payload.has_value())
                                {
                                    caseInstr.bindingName = tpp::CaseBinding{sourceCase->bindingName, variant.recursive};
                                    env.push(sourceCase->bindingName,
                                             *variant.payload,
                                             false,
                                             isOptionalTypeRef(*variant.payload));
                                                            loweredCaseBody = lowerPublicNodes(sourceCase->body, env, functions, syntheticCounter, includeSourceRanges);
                                    env.pop();
                                }
                                else
                                {
                                                            loweredCaseBody = lowerPublicNodes(sourceCase->body, env, functions, syntheticCounter, includeSourceRanges);
                                }
                            }
                            else if (arg->defaultCase)
                            {
                                                        loweredCaseBody = lowerPublicNodes(arg->defaultCase->body, env, functions, syntheticCounter, includeSourceRanges);
                            }

                                                    caseInstr.body = std::make_unique<std::vector<tpp::Instruction>>(std::move(loweredCaseBody));

                            cases->push_back(std::move(caseInstr));
                        }
                    }

                    switchInstr->cases = std::move(cases);
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

                    tpp::CallInstr callInstr = lowerResolvedCall(arg->functionName, arguments, functions);
                    callInstr.sourceRange = tpp::to_public_range(arg->sourceRange, includeSourceRanges);
                    out.push_back(tpp::Instruction{tpp::Instruction::Value{std::in_place_index<6>, std::move(callInstr)}});
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<RenderViaNode>>)
                {
                    auto lowered = lowerRenderViaNode(*arg, env, functions, syntheticCounter, includeSourceRanges);
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
            functionDef.body = std::make_unique<std::vector<tpp::Instruction>>(lowerPublicNodes(fn.body, env, functions, syntheticCounter, includeSourceRanges));
            functionDef.policy = fn.policy;
            functionDef.doc = fn.doc;
            functionDef.sourceRange = tpp::to_public_range(fn.sourceRange, includeSourceRanges);
            if (includeSourceRanges && !fn.sourceUri.empty())
                functionDef.sourceUri = fn.sourceUri;

            out.push_back(std::move(functionDef));
        }

        return true;
    }
}
