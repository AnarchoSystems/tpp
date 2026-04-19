#include "tpp/TemplateParser.h"
#include "tpp/TypedefParser.h"
#include <tpp/IR.h>
#include <tpp/Diagnostic.h>
#include <algorithm>
#include <cassert>
#include <map>
#include <set>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // Analysis context: type resolution, slot allocation, optional guard tracking
    // ════════════════════════════════════════════════════════════════════════════

    namespace
    {
        TypeKind make_type_str()
        {
            TypeKind t;
            t.value.emplace<0>();
            return t;
        }

        TypeKind make_type_int()
        {
            TypeKind t;
            t.value.emplace<1>();
            return t;
        }

        TypeKind make_type_bool()
        {
            TypeKind t;
            t.value.emplace<2>();
            return t;
        }

        TypeKind make_type_named(std::string name)
        {
            TypeKind t;
            t.value.emplace<3>(TypeKind_Named{std::move(name)});
            return t;
        }

        TypeKind make_type_list(TypeKind inner)
        {
            TypeKind t;
            t.value.emplace<4>(TypeKind_List{std::make_shared<TypeKind>(std::move(inner))});
            return t;
        }

        TypeKind make_type_optional(TypeKind inner)
        {
            TypeKind t;
            t.value.emplace<5>(TypeKind_Optional{std::make_shared<TypeKind>(std::move(inner))});
            return t;
        }

        bool is_type_named(const TypeKind &t) { return std::holds_alternative<TypeKind_Named>(t.value); }
        bool is_type_list(const TypeKind &t) { return std::holds_alternative<TypeKind_List>(t.value); }
        bool is_type_optional(const TypeKind &t) { return std::holds_alternative<TypeKind_Optional>(t.value); }
        bool is_type_bool(const TypeKind &t) { return std::holds_alternative<TypeKind_Bool>(t.value); }

        const std::string &named_type_name(const TypeKind &t) { return std::get<TypeKind_Named>(t.value).value; }

        const TypeKind *list_inner_type(const TypeKind &t)
        {
            if (!is_type_list(t)) return nullptr;
            auto &p = std::get<TypeKind_List>(t.value).value;
            return p ? p.get() : nullptr;
        }

        const TypeKind *optional_inner_type(const TypeKind &t)
        {
            if (!is_type_optional(t)) return nullptr;
            auto &p = std::get<TypeKind_Optional>(t.value).value;
            return p ? p.get() : nullptr;
        }

        bool type_equal(const TypeKind &a, const TypeKind &b)
        {
            if (a.value.index() != b.value.index()) return false;
            if (is_type_named(a)) return named_type_name(a) == named_type_name(b);
            if (const TypeKind *ai = list_inner_type(a))
            {
                const TypeKind *bi = list_inner_type(b);
                return bi && type_equal(*ai, *bi);
            }
            if (const TypeKind *ai = optional_inner_type(a))
            {
                const TypeKind *bi = optional_inner_type(b);
                return bi && type_equal(*ai, *bi);
            }
            return true;
        }

        bool type_is_scalar(const TypeKind &t)
        {
            return std::holds_alternative<TypeKind_Str>(t.value) ||
                   std::holds_alternative<TypeKind_Int>(t.value) ||
                   std::holds_alternative<TypeKind_Bool>(t.value);
        }

        std::string type_display_name(const TypeKind &t)
        {
            if (std::holds_alternative<TypeKind_Str>(t.value)) return "string";
            if (std::holds_alternative<TypeKind_Int>(t.value)) return "int";
            if (std::holds_alternative<TypeKind_Bool>(t.value)) return "bool";
            if (is_type_named(t)) return named_type_name(t);
            if (const TypeKind *inner = list_inner_type(t))
                return "list<" + type_display_name(*inner) + ">";
            if (const TypeKind *inner = optional_inner_type(t))
                return "optional<" + type_display_name(*inner) + ">";
            return "?";
        }

        struct TypeEnv
        {
            std::map<std::string, StructDef> structs;
            std::map<std::string, EnumDef> enums;

            TypeKind resolveTypeName(const std::string &name) const
            {
                if (name == "string") return make_type_str();
                if (name == "int") return make_type_int();
                if (name == "bool") return make_type_bool();

                // Handle generic types: list<T>, optional<T>
                if (name.size() > 5 && name.substr(0, 5) == "list<" && name.back() == '>')
                {
                    std::string inner = name.substr(5, name.size() - 6);
                    return make_type_list(resolveTypeName(inner));
                }
                if (name.size() > 9 && name.substr(0, 9) == "optional<" && name.back() == '>')
                {
                    std::string inner = name.substr(9, name.size() - 10);
                    return make_type_optional(resolveTypeName(inner));
                }

                if (structs.count(name) || enums.count(name))
                    return make_type_named(name);
                return make_type_str(); // default fallback
            }

            bool isEnumType(const std::string &name) const { return enums.count(name); }
            bool isStructType(const std::string &name) const { return structs.count(name); }

            bool isValidType(const std::string &name) const
            {
                if (name == "string" || name == "int" || name == "bool") return true;
                if (name.size() > 5 && name.substr(0, 5) == "list<" && name.back() == '>')
                    return isValidType(name.substr(5, name.size() - 6));
                if (name.size() > 9 && name.substr(0, 9) == "optional<" && name.back() == '>')
                    return isValidType(name.substr(9, name.size() - 10));
                return structs.count(name) || enums.count(name);
            }

            const StructDef *getStruct(const std::string &name) const
            {
                auto it = structs.find(name);
                return it != structs.end() ? &it->second : nullptr;
            }

            const EnumDef *getEnum(const std::string &name) const
            {
                auto it = enums.find(name);
                return it != enums.end() ? &it->second : nullptr;
            }

            // Given a TypeKind, resolve what a field access returns
            bool resolveFieldType(const TypeKind &parentType, const std::string &field,
                                  TypeKind &result, bool &isOptional, bool &isRecursive) const
            {
                if (!is_type_named(parentType)) return false;
                auto *s = getStruct(named_type_name(parentType));
                if (!s) return false;
                for (auto &f : s->fields)
                {
                    if (f.name == field)
                    {
                        result = f.type;
                        isRecursive = f.recursive;
                        if (is_type_optional(f.type))
                        {
                            isOptional = true;
                            const TypeKind *inner = optional_inner_type(f.type);
                            if (inner) result = *inner;
                        }
                        return true;
                    }
                }
                return false;
            }
        };

        struct SlotInfo
        {
            std::string name;
            int slot;
            TypeKind type;
            bool isOptional = false;
            bool isRecursive = false;
        };

        struct AnalysisContext
        {
            const TypeEnv &typeEnv;
            std::vector<DiagnosticLSPMessage> &diagnostics;
            std::string uri;

            // Slot allocation
            std::map<std::string, SlotInfo> variables; // variable name -> slot info
            int nextSlot = 0;

            // Known functions (for call resolution)
            std::map<std::string, int> functionIndices;
            const std::vector<ASTFunction> *allFunctions = nullptr;

            // Guarded optionals
            std::set<std::string> guardedOptionals;
            std::set<std::string> absentOptionals;

            const std::vector<PolicyDef> *policies = nullptr;

            int allocSlot(const std::string &name, TypeKind type, bool isOptional = false, bool isRecursive = false)
            {
                int s = nextSlot++;
                variables[name] = {name, s, std::move(type), isOptional, isRecursive};
                return s;
            }

            void error(const std::string &msg, int line = 0, int col_ = 0, int endCol = -1)
            {
                DiagnosticLSPMessage dm;
                dm.uri = uri;
                Diagnostic d;
                d.message = msg;
                d.range.start = {line, col_};
                d.range.end = {line, endCol >= 0 ? endCol : col_};
                d.severity = DiagnosticSeverity::Error;
                // Merge into existing DiagnosticLSPMessage for this URI preserving insertion order.
                    for (auto &existing : diagnostics)
                    {
                        if (existing.uri == uri)
                        {
                        existing.diagnostics.push_back(std::move(d));
                            return;
                        }
                    }
                    dm.diagnostics.push_back(std::move(d));
                    diagnostics.push_back(std::move(dm));
            }

            // Resolve overloaded function by name and first-argument type.
            // Returns the index into allFunctions, or -1 if not found.
            int findOverload(const std::string &funcName, const TypeKind &argType) const
            {
                int fallback = -1;
                if (!allFunctions) return fallback;
                for (size_t i = 0; i < allFunctions->size(); i++)
                {
                    auto &f = (*allFunctions)[i];
                    if (f.name != funcName) continue;
                    if (fallback < 0) fallback = static_cast<int>(i);
                    if (f.params.size() >= 1)
                    {
                        TypeKind ptype = typeEnv.resolveTypeName(f.params[0].typeName);
                        if (type_equal(ptype, argType))
                            return static_cast<int>(i);
                    }
                }
                // If no exact match, return first with that name (for no-payload cases)
                return fallback;
            }

            bool hasPolicy(const std::string &policyName) const
            {
                if (policyName.empty() || !policies) return true;
                for (const auto &p : *policies)
                    if (p.tag == policyName) return true;
                return false;
            }

            bool hasExactOverload(const std::string &funcName, const TypeKind &argType) const
            {
                if (!allFunctions) return false;
                for (const auto &f : *allFunctions)
                {
                    if (f.name != funcName) continue;
                    if (f.params.size() != 1) continue;
                    TypeKind ptype = typeEnv.resolveTypeName(f.params[0].typeName);
                    if (type_equal(ptype, argType)) return true;
                }
                return false;
            }

            bool hasZeroArgOverload(const std::string &funcName) const
            {
                if (!allFunctions) return false;
                for (const auto &f : *allFunctions)
                    if (f.name == funcName && f.params.empty())
                        return true;
                return false;
            }

            ExprRef resolveExpr(const ASTExpr &expr, const std::string &exprPolicy = "")
            {
                ExprRef ref;

                auto it = variables.find(expr.root);
                if (it == variables.end())
                {
                        error("unknown variable '" + expr.root + "'",
                              expr.line, expr.startCol, expr.endCol > 0 ? expr.endCol : expr.startCol);
                    return ref;
                }

                ref.rootSlot = it->second.slot;
                ref.type = it->second.type;
                ref.isOptional = it->second.isOptional;
                ref.isRecursive = it->second.isRecursive;

                // Resolve field chain — each step carries its own metadata
                TypeKind currentType = it->second.type;
                for (size_t i = 0; i < expr.fields.size(); i++)
                {
                    PathStep step;
                    step.field = expr.fields[i];

                    // Compute field index from the current type
                    TypeKind &parentType = (i == 0) ? it->second.type : currentType;
                    if (is_type_named(parentType))
                    {
                        auto *s = typeEnv.getStruct(named_type_name(parentType));
                        if (s)
                        {
                            for (size_t fi = 0; fi < s->fields.size(); fi++)
                            {
                                if (s->fields[fi].name == expr.fields[i])
                                {
                                    step.fieldIndex = static_cast<int>(fi);
                                    break;
                                }
                            }
                        }
                    }

                    TypeKind fieldType;
                    bool fieldOpt = false, fieldRec = false;
                    if (typeEnv.resolveFieldType(currentType, expr.fields[i], fieldType, fieldOpt, fieldRec))
                    {
                        currentType = fieldType;
                        step.isRecursive = fieldRec;
                        step.isOptional = fieldOpt;
                        step.resolvedType = fieldType;
                        if (fieldOpt) ref.isOptional = true;
                        if (fieldRec) ref.isRecursive = true;
                        ref.type = fieldType;
                    }
                    else if (is_type_named(currentType))
                    {
                        error("Reference to undeclared member '" + expr.fields[i] +
                              "' of struct '" + named_type_name(currentType) + "'",
                              expr.line, expr.startCol, expr.endCol);
                    }

                    ref.steps.push_back(std::move(step));
                }

                return ref;
            }
        };

        // ═══════════════════════════════════════════════════════════════════
        // Lower AST nodes to IR instructions
        // ═══════════════════════════════════════════════════════════════════

        void lowerBody(const std::vector<ASTNode> &body, std::vector<Instruction> &out,
                       AnalysisContext &ctx);

        Instruction lowerNode(const ASTNode &node, AnalysisContext &ctx)
        {
            return std::visit([&](auto &n) -> Instruction {
                using T = std::decay_t<decltype(n)>;

                if constexpr (std::is_same_v<T, ASTEmit>)
                {
                    Instruction instr;
                    instr.value.emplace<0>(Instruction_Emit{Emit{n.text}});
                    return instr;
                }
                else if constexpr (std::is_same_v<T, ASTEmitExpr>)
                {
                    size_t diagsBefore = ctx.diagnostics.size();
                    auto ref = ctx.resolveExpr(n.expr);
                    bool hadError = ctx.diagnostics.size() > diagsBefore;
                    if (!hadError && !type_is_scalar(ref.type))
                    {
                        ctx.error("interpolation requires string, int, or bool; got '" +
                                  type_display_name(ref.type) + "'",
                                  n.expr.line, n.expr.startCol, n.expr.endCol);
                    }
                        if (!hadError && ref.isOptional)
                        {
                            std::string path = std::to_string(ref.rootSlot);
                            for (auto &s : ref.steps) path += "." + s.field;
                            std::string nicePath = n.expr.root;
                            for (auto &f : n.expr.fields) nicePath += "." + f;

                            if (ctx.absentOptionals.count(path))
                            {
                                ctx.error("optional value '" + nicePath + "' does not exist in this branch",
                                          n.expr.line, n.expr.startCol, n.expr.endCol);
                            }
                            else if (!ctx.guardedOptionals.count(path))
                            {
                                ctx.error("optional value '" + nicePath + "' must be guarded by '@if " + nicePath + "@'",
                                          n.expr.line, n.expr.startCol, n.expr.endCol);
                            }
                        }
                    Instruction instr;
                    std::string policy = n.expr.policy;
                    instr.value.emplace<1>(Instruction_EmitExpr{EmitExpr{std::move(ref), std::move(policy)}});
                    return instr;
                }
                else if constexpr (std::is_same_v<T, ASTAlignCell>)
                {
                    Instruction instr;
                    instr.value.emplace<2>(Instruction_AlignCell{AlignCell{}});
                    return instr;
                }
                else if constexpr (std::is_same_v<T, ASTFor>)
                {
                    ForInstr fi;
                    fi.collection = ctx.resolveExpr(n.collection);

                    // Determine element type from collection type
                    TypeKind elemType = make_type_str();
                    if (const TypeKind *inner = list_inner_type(fi.collection.type))
                        elemType = *inner;

                    fi.varName = n.varName;
                    fi.varSlot = ctx.allocSlot(n.varName, elemType);
                    fi.insertCol = n.insertCol;
                    fi.isBlock = n.isBlock;
                    fi.policy = n.options.policy;

                    if (!n.options.sep.empty()) fi.sep = n.options.sep;
                    if (!n.options.precededBy.empty()) fi.precededBy = n.options.precededBy;
                    if (!n.options.followedBy.empty()) fi.followedBy = n.options.followedBy;
                    if (!n.options.enumerator.empty())
                    {
                        fi.enumeratorName = n.options.enumerator;
                        fi.enumeratorSlot = ctx.allocSlot(n.options.enumerator, make_type_int());
                    }
                    if (n.options.alignSpec.has_value())
                        fi.alignSpec = *n.options.alignSpec;

                    fi.body = std::make_shared<std::vector<Instruction>>();
                    lowerBody(n.body, *fi.body, ctx);

                        if (n.options.alignSpec.has_value())
                        {
                            int specCols = static_cast<int>(n.options.alignSpec->size());
                            int bodyCols = n.alignCellCount + 1;
                            if (specCols <= 1) {
                                // Empty align or single-char align applies to all columns.
                                specCols = bodyCols;
                            }
                            int allowedAmpersands = std::max(0, specCols - 1);
                            if (n.alignCellCount > allowedAmpersands)
                            {
                                int seenAmp = 0;
                                for (const auto &bodyNode : n.body)
                                {
                                    auto *align = std::get_if<ASTAlignCell>(&bodyNode.data);
                                    if (!align) continue;
                                    if (seenAmp == allowedAmpersands)
                                    {
                                        ctx.error("@&@ exceeds align spec — spec only covers " +
                                                  std::to_string(specCols) + " column(s)",
                                                  align->line - 1, align->col - 1, align->endCol);
                                        break;
                                    }
                                    ++seenAmp;
                                }
                                ctx.error("align spec has " + std::to_string(specCols) +
                                          " column(s) but the loop body has " + std::to_string(bodyCols),
                                          n.openLine - 1, n.openCol - 1, n.openEndCol);
                            }
                        }

                    // Remove loop variable from scope
                    ctx.variables.erase(n.varName);
                    if (!n.options.enumerator.empty())
                        ctx.variables.erase(n.options.enumerator);

                    Instruction instr;
                    instr.value.emplace<3>(Instruction_For{std::make_shared<ForInstr>(std::move(fi))});
                    return instr;
                }
                else if constexpr (std::is_same_v<T, ASTIf>)
                {
                    IfInstr ii;
                    ii.condExpr = ctx.resolveExpr(n.condition);
                    ii.negated = n.negated;
                    ii.insertCol = n.insertCol;
                    ii.isBlock = n.isBlock;

                        if (n.negated)
                        {
                            bool isBoolExpr = is_type_bool(ii.condExpr.type);
                            if (!isBoolExpr && !ii.condExpr.isOptional)
                            {
                                ctx.error("'not' is only allowed for bool or optional expressions",
                                          n.openLine - 1, n.openCol - 1, n.openEndCol);
                            }
                        }

                    // If guarding an optional, add to guarded set
                    bool thenGuarded = false;
                    bool thenAbsent = false;
                    bool elseGuarded = false;
                    bool elseAbsent = false;
                    std::string guardPath;
                    if (ii.condExpr.isOptional)
                    {
                        guardPath = std::to_string(ii.condExpr.rootSlot);
                        for (auto &s : ii.condExpr.steps) guardPath += "." + s.field;
                            std::string nicePath = n.condition.root;
                            for (auto &f : n.condition.fields) nicePath += "." + f;

                        if (!n.negated)
                            {
                            if (ctx.absentOptionals.count(guardPath))
                            {
                                ctx.error("cannot guard optional value '" + nicePath +
                                          "' because it does not exist in this branch",
                                          n.openLine - 1, n.openCol - 1, n.openEndCol);
                            }
                            else
                            {
                                ctx.guardedOptionals.insert(guardPath);
                                thenGuarded = true;
                            }
                            }
                            else
                            {
                            ctx.absentOptionals.insert(guardPath);
                            thenAbsent = true;
                            }
                    }

                    ii.thenBody = std::make_shared<std::vector<Instruction>>();
                    lowerBody(n.thenBody, *ii.thenBody, ctx);

                    if (thenGuarded)
                        ctx.guardedOptionals.erase(guardPath);
                    if (thenAbsent)
                        ctx.absentOptionals.erase(guardPath);

                    if (ii.condExpr.isOptional)
                    {
                        if (!n.negated)
                        {
                            ctx.absentOptionals.insert(guardPath);
                            elseAbsent = true;
                        }
                        else
                        {
                            if (ctx.absentOptionals.count(guardPath))
                            {
                                std::string nicePath = n.condition.root;
                                for (auto &f : n.condition.fields) nicePath += "." + f;
                                ctx.error("cannot guard optional value '" + nicePath +
                                          "' because it does not exist in this branch",
                                          n.openLine - 1, n.openCol - 1, n.openEndCol);
                            }
                            else
                            {
                                ctx.guardedOptionals.insert(guardPath);
                                elseGuarded = true;
                            }
                        }
                    }

                    ii.elseBody = std::make_shared<std::vector<Instruction>>();
                    lowerBody(n.elseBody, *ii.elseBody, ctx);

                    if (elseAbsent)
                        ctx.absentOptionals.erase(guardPath);
                    if (elseGuarded)
                        ctx.guardedOptionals.erase(guardPath);

                    Instruction instr;
                    instr.value.emplace<4>(Instruction_If{std::make_shared<IfInstr>(std::move(ii))});
                    return instr;
                }
                else if constexpr (std::is_same_v<T, ASTSwitch>)
                {
                    SwitchInstr si;
                    si.expr = ctx.resolveExpr(n.expr);
                    si.insertCol = n.insertCol;
                    si.isBlock = n.isBlock;
                    si.policy = n.policy;

                        if (!n.policy.empty() && !ctx.hasPolicy(n.policy))
                        {
                            ctx.error("unknown policy '" + n.policy + "'",
                                      n.policyValueLine - 1,
                                      n.policyValueCol,
                                      n.policyValueEndCol);
                        }

                    // Look up the enum to get variant info
                    const EnumDef *enumDef = nullptr;
                    if (is_type_named(si.expr.type))
                        enumDef = ctx.typeEnv.getEnum(named_type_name(si.expr.type));

                    si.cases = std::make_shared<std::vector<CaseDef>>();

                    for (auto &astCase : n.cases)
                    {
                        CaseDef cd;
                        cd.tag = astCase.tag;

                        // Handle @default@ — expand to individual cases
                        if (astCase.tag == "default" && enumDef)
                        {
                            // Collect already-handled tags
                            std::set<std::string> handledTags;
                            for (auto &c : n.cases)
                                if (c.tag != "default") handledTags.insert(c.tag);
                            // Generate a case for each unhandled variant
                            for (auto &variant : enumDef->variants)
                            {
                                if (handledTags.count(variant.tag)) continue;
                                CaseDef defaultCase;
                                defaultCase.tag = variant.tag;
                                if (variant.payload.has_value())
                                {
                                    defaultCase.payloadType = *variant.payload;
                                    defaultCase.payloadIsRecursive = variant.recursive;
                                }
                                defaultCase.body = std::make_shared<std::vector<Instruction>>();
                                lowerBody(astCase.body, *defaultCase.body, ctx);
                                si.cases->push_back(std::move(defaultCase));
                            }
                            continue;
                        }

                        // Find the variant definition
                        if (enumDef)
                        {
                            for (auto &variant : enumDef->variants)
                            {
                                if (variant.tag == astCase.tag)
                                {
                                    if (variant.payload.has_value())
                                    {
                                        cd.payloadType = *variant.payload;
                                        cd.payloadIsRecursive = variant.recursive;
                                    }
                                    break;
                                }
                            }
                        }

                        // Handle binding
                        if (!astCase.binding.empty())
                        {
                            TypeKind bindType = cd.payloadType.value_or(make_type_str());
                            int bindSlot = ctx.allocSlot(astCase.binding, bindType,
                                                         false, cd.payloadIsRecursive);
                            BindingDef bd;
                            bd.name = astCase.binding;
                            bd.slot = bindSlot;
                            bd.isRecursive = cd.payloadIsRecursive;
                            cd.bindingName = bd;
                        }

                        cd.body = std::make_shared<std::vector<Instruction>>();
                        lowerBody(astCase.body, *cd.body, ctx);

                        // Remove binding from scope
                        if (!astCase.binding.empty())
                            ctx.variables.erase(astCase.binding);

                        si.cases->push_back(std::move(cd));
                    }

                    // Fill missing enum variants with empty bodies
                    if (enumDef)
                    {
                        std::set<std::string> handledTags;
                        for (auto &c : *si.cases) handledTags.insert(c.tag);
                        for (auto &variant : enumDef->variants)
                        {
                            if (handledTags.count(variant.tag)) continue;
                            CaseDef cd;
                            cd.tag = variant.tag;
                            if (variant.payload.has_value())
                            {
                                cd.payloadType = *variant.payload;
                                cd.payloadIsRecursive = variant.recursive;
                            }
                            cd.body = std::make_shared<std::vector<Instruction>>();
                            si.cases->push_back(std::move(cd));
                        }

                        if (n.checkExhaustive)
                        {
                            std::vector<std::string> missing;
                            for (auto &variant : enumDef->variants)
                                if (!handledTags.count(variant.tag))
                                    missing.push_back(variant.tag);
                            if (!missing.empty())
                            {
                                std::string list;
                                for (size_t i = 0; i < missing.size(); i++)
                                {
                                    if (i > 0) list += ", ";
                                    list += missing[i];
                                }
                                ctx.error("switch is missing cases for enum '" + enumDef->name + "': " + list,
                                          n.endLine - 1, n.endCol - 1, n.endEndCol);
                            }
                        }
                    }

                    Instruction instr;
                    instr.value.emplace<5>(Instruction_Switch{std::make_shared<SwitchInstr>(std::move(si))});
                    return instr;
                }
                else if constexpr (std::is_same_v<T, ASTCall>)
                {
                    CallInstr ci;
                    ci.functionName = n.functionName;

                    for (auto &arg : n.arguments)
                        ci.arguments.push_back(ctx.resolveExpr(arg));

                    // Try overload resolution based on first argument type
                    ci.functionIndex = -1;
                    if (!ci.arguments.empty())
                        ci.functionIndex = ctx.findOverload(n.functionName, ci.arguments[0].type);
                    if (ci.functionIndex < 0)
                    {
                        auto it = ctx.functionIndices.find(n.functionName);
                        ci.functionIndex = it != ctx.functionIndices.end() ? it->second : -1;
                    }

                    Instruction instr;
                    instr.value.emplace<6>(Instruction_Call{std::move(ci)});
                    return instr;
                }
                else if constexpr (std::is_same_v<T, ASTRenderVia>)
                {
                    // Desugar render_via into For + Switch + Call
                    // For a single variant: just a Call
                    // For a list: For loop calling the function for each element

                    // Check if the collection is a list or a single value
                    auto rootIt = ctx.variables.find(n.collection.root);
                    if (rootIt != ctx.variables.end() &&
                        is_type_list(rootIt->second.type) &&
                        !n.collection.fields.empty())
                    {
                        ctx.error("cannot access member '" + n.collection.fields.front() +
                                  "' on non-struct value '" + n.collection.root + "'",
                                  n.openLine - 1, n.openCol - 1, n.openEndCol);
                        Instruction instr;
                        instr.value.emplace<0>(Instruction_Emit{Emit{""}});
                        return instr;
                    }

                    auto collRef = ctx.resolveExpr(n.collection);

                    bool isList = is_type_list(collRef.type);

                    if (isList)
                    {
                        // Desugar: @for _item in collection | options@ @functionName(_item)@ @end for@
                        ForInstr fi;
                        fi.collection = collRef;

                        TypeKind elemType = make_type_str();
                        if (const TypeKind *inner = list_inner_type(collRef.type))
                            elemType = *inner;

                        std::string syntheticVar = "_render_item";
                        fi.varName = syntheticVar;
                        fi.varSlot = ctx.allocSlot(syntheticVar, elemType);
                        fi.insertCol = n.insertCol;
                        fi.isBlock = n.isBlock;
                        fi.policy = n.options.policy;

                        if (!n.options.sep.empty()) fi.sep = n.options.sep;
                        if (!n.options.precededBy.empty()) fi.precededBy = n.options.precededBy;
                        if (!n.options.followedBy.empty()) fi.followedBy = n.options.followedBy;
                        if (!n.options.enumerator.empty())
                        {
                            fi.enumeratorName = n.options.enumerator;
                            fi.enumeratorSlot = ctx.allocSlot(n.options.enumerator, make_type_int());
                        }

                        // Body: if element type is a Named enum, generate Switch dispatch;
                        // otherwise, a single Call.
                        const EnumDef *elemEnum = nullptr;
                        if (is_type_named(elemType))
                            elemEnum = ctx.typeEnv.getEnum(named_type_name(elemType));

                        if (elemEnum)
                        {
                            std::vector<std::string> missingTags;
                            for (auto &variant : elemEnum->variants)
                            {
                                bool ok = false;
                                if (variant.payload.has_value())
                                    ok = ctx.hasExactOverload(n.functionName, *variant.payload);
                                else
                                    ok = ctx.hasZeroArgOverload(n.functionName);
                                if (!ok) missingTags.push_back(variant.tag);
                            }
                            if (!missingTags.empty())
                            {
                                std::string list;
                                for (size_t i = 0; i < missingTags.size(); i++)
                                {
                                    if (i > 0) list += ", ";
                                    list += missingTags[i];
                                }
                                ctx.error("render-via function '" + n.functionName +
                                          "' is missing overloads for enum '" + elemEnum->name +
                                          "': " + list,
                                          n.openLine - 1, n.openCol - 1, n.openEndCol);
                            }

                            // Generate Switch over the enum, dispatching to overloaded functions
                            SwitchInstr si;
                            ExprRef itemSwitchRef;
                            itemSwitchRef.rootSlot = fi.varSlot;
                            itemSwitchRef.type = elemType;
                            si.expr = itemSwitchRef;
                            si.insertCol = 0;
                            si.isBlock = false;
                            si.cases = std::make_shared<std::vector<CaseDef>>();

                            for (auto &variant : elemEnum->variants)
                            {
                                CaseDef cd;
                                cd.tag = variant.tag;
                                if (variant.payload.has_value())
                                {
                                    cd.payloadType = *variant.payload;
                                    cd.payloadIsRecursive = variant.recursive;
                                }

                                CallInstr ci;
                                ci.functionName = n.functionName;

                                if (variant.payload.has_value())
                                {
                                    ci.functionIndex = ctx.findOverload(n.functionName, *variant.payload);
                                    std::string bindName = "_render_payload";
                                    int bindSlot = ctx.allocSlot(bindName, *variant.payload,
                                                                 false, variant.recursive);
                                    BindingDef bd;
                                    bd.name = bindName;
                                    bd.slot = bindSlot;
                                    bd.isRecursive = variant.recursive;
                                    cd.bindingName = bd;

                                    ExprRef payloadRef;
                                    payloadRef.rootSlot = bindSlot;
                                    payloadRef.type = *variant.payload;
                                    payloadRef.isRecursive = variant.recursive;
                                    ci.arguments.push_back(std::move(payloadRef));

                                    ctx.variables.erase(bindName);
                                }
                                else
                                {
                                    auto fit = ctx.functionIndices.find(n.functionName);
                                    ci.functionIndex = fit != ctx.functionIndices.end() ? fit->second : -1;
                                }

                                Instruction callInstr;
                                callInstr.value.emplace<6>(Instruction_Call{std::move(ci)});
                                cd.body = std::make_shared<std::vector<Instruction>>();
                                cd.body->push_back(std::move(callInstr));

                                si.cases->push_back(std::move(cd));
                            }

                            Instruction switchInstr;
                            switchInstr.value.emplace<5>(Instruction_Switch{std::make_shared<SwitchInstr>(std::move(si))});
                            fi.body = std::make_shared<std::vector<Instruction>>();
                            fi.body->push_back(std::move(switchInstr));
                        }
                        else
                        {
                            if (!ctx.hasExactOverload(n.functionName, elemType))
                            {
                                ctx.error("render-via function '" + n.functionName +
                                          "' must have an overload taking '" + type_display_name(elemType) + "'",
                                          n.openLine - 1, n.openCol - 1, n.openEndCol);
                            }

                            CallInstr ci;
                            ci.functionName = n.functionName;
                            auto fit = ctx.functionIndices.find(n.functionName);
                            ci.functionIndex = fit != ctx.functionIndices.end() ? fit->second : -1;

                            ExprRef itemRef;
                            itemRef.rootSlot = fi.varSlot;
                            itemRef.type = elemType;
                            ci.arguments.push_back(std::move(itemRef));

                            Instruction callInstr;
                            callInstr.value.emplace<6>(Instruction_Call{std::move(ci)});
                            fi.body = std::make_shared<std::vector<Instruction>>();
                            fi.body->push_back(std::move(callInstr));
                        }

                        ctx.variables.erase(syntheticVar);
                        if (!n.options.enumerator.empty())
                            ctx.variables.erase(n.options.enumerator);

                        Instruction instr;
                        instr.value.emplace<3>(Instruction_For{std::make_shared<ForInstr>(std::move(fi))});
                        return instr;
                    }
                    else if (is_type_named(collRef.type))
                    {
                        // Check if it's an enum — if so, desugar into switch
                        const EnumDef *ed = ctx.typeEnv.getEnum(named_type_name(collRef.type));
                        if (ed)
                        {
                            // Desugar: @switch collection@ @case Tag(v)@ @func(v)@ @end case@ ... @end switch@
                            SwitchInstr si;
                            si.expr = collRef;
                            si.insertCol = n.insertCol;
                            si.isBlock = n.isBlock;

                            si.cases = std::make_shared<std::vector<CaseDef>>();
                            for (auto &variant : ed->variants)
                            {
                                CaseDef cd;
                                cd.tag = variant.tag;
                                if (variant.payload.has_value())
                                {
                                    cd.payloadType = *variant.payload;
                                    cd.payloadIsRecursive = variant.recursive;
                                }

                                // Build call body
                                CallInstr ci;
                                ci.functionName = n.functionName;
                                if (variant.payload.has_value())
                                    ci.functionIndex = ctx.findOverload(n.functionName, *variant.payload);
                                else
                                {
                                    auto fit = ctx.functionIndices.find(n.functionName);
                                    ci.functionIndex = fit != ctx.functionIndices.end() ? fit->second : -1;
                                }

                                if (variant.payload.has_value())
                                {
                                    std::string bindName = "_render_payload";
                                    int bindSlot = ctx.allocSlot(bindName, *variant.payload,
                                                                 false, variant.recursive);
                                    BindingDef bd;
                                    bd.name = bindName;
                                    bd.slot = bindSlot;
                                    bd.isRecursive = variant.recursive;
                                    cd.bindingName = bd;

                                    ExprRef payloadRef;
                                    payloadRef.rootSlot = bindSlot;
                                    payloadRef.type = *variant.payload;
                                    payloadRef.isRecursive = variant.recursive;
                                    ci.arguments.push_back(std::move(payloadRef));

                                    ctx.variables.erase(bindName);
                                }
                                // else: no payload → call with no extra args

                                Instruction callInstr;
                                callInstr.value.emplace<6>(Instruction_Call{std::move(ci)});
                                cd.body = std::make_shared<std::vector<Instruction>>();
                                cd.body->push_back(std::move(callInstr));

                                si.cases->push_back(std::move(cd));
                            }

                            Instruction instr;
                            instr.value.emplace<5>(Instruction_Switch{std::make_shared<SwitchInstr>(std::move(si))});
                            return instr;
                        }
                        else
                        {
                            // Named struct: just call the function directly
                            CallInstr ci;
                            ci.functionName = n.functionName;
                            auto fit = ctx.functionIndices.find(n.functionName);
                            ci.functionIndex = fit != ctx.functionIndices.end() ? fit->second : -1;
                            ci.arguments.push_back(collRef);

                            Instruction instr;
                            instr.value.emplace<6>(Instruction_Call{std::move(ci)});
                            return instr;
                        }
                    }
                    else
                    {
                        // Single value render: just call the function
                        CallInstr ci;
                        ci.functionName = n.functionName;
                        auto fit = ctx.functionIndices.find(n.functionName);
                        ci.functionIndex = fit != ctx.functionIndices.end() ? fit->second : -1;
                        ci.arguments.push_back(collRef);

                        Instruction instr;
                        instr.value.emplace<6>(Instruction_Call{std::move(ci)});
                        return instr;
                    }
                }
                else if constexpr (std::is_same_v<T, ASTComment>)
                {
                    // Comments produce no output — return empty emit
                    Instruction instr;
                    instr.value.emplace<0>(Instruction_Emit{Emit{""}});
                    return instr;
                }
                else
                {
                    Instruction instr;
                    instr.value.emplace<0>(Instruction_Emit{Emit{""}});
                    return instr;
                }
            }, node.data);
        }

        void lowerBody(const std::vector<ASTNode> &body, std::vector<Instruction> &out,
                       AnalysisContext &ctx)
        {
            for (auto &node : body)
            {
                auto instr = lowerNode(node, ctx);
                // Skip empty emits
                if (auto *emit = std::get_if<Instruction_Emit>(&instr.value))
                {
                    if (emit->value.text.empty()) continue;
                }
                out.push_back(std::move(instr));
            }
        }
    } // anonymous namespace

    // ════════════════════════════════════════════════════════════════════════════
    // Public analysis + lowering API
    // ════════════════════════════════════════════════════════════════════════════

    struct AnalyzedData
    {
        TypeEnv typeEnv;
        std::vector<ASTFunction> functions;
        std::vector<PolicyDef> policies;
    };

    bool analyze_and_lower(const std::vector<ParsedTypes> &typesSources,
                           const std::vector<std::vector<ASTFunction>> &templateSources,
                           const std::vector<PolicyDef> &policies,
                           IR &output,
                           std::vector<DiagnosticLSPMessage> &diagnostics)
    {
        size_t diagsBefore = diagnostics.size();
        TypeEnv typeEnv;

        // Collect all types
        for (auto &ts : typesSources)
        {
            for (auto &s : ts.structs)
            {
                if (typeEnv.structs.count(s.name) || typeEnv.enums.count(s.name))
                {
                    DiagnosticLSPMessage dm;
                    dm.uri = s.uri;
                    Diagnostic d;
                    d.message = "type '" + s.name + "' is already defined";
                    d.range.start = {s.nameLine, s.nameCol};
                    d.range.end = {s.nameLine, s.nameEndCol};
                    d.severity = DiagnosticSeverity::Error;
                    dm.diagnostics.push_back(d);
                    diagnostics.push_back(dm);
                    continue;
                }
                typeEnv.structs[s.name] = s;
                output.structs.push_back(s);
            }
            for (auto &e : ts.enums)
            {
                if (typeEnv.enums.count(e.name) || typeEnv.structs.count(e.name))
                {
                    DiagnosticLSPMessage dm;
                    dm.uri = e.uri;
                    Diagnostic d;
                    d.message = "type '" + e.name + "' is already defined";
                    d.range.start = {e.nameLine, e.nameCol};
                    d.range.end = {e.nameLine, e.nameEndCol};
                    d.severity = DiagnosticSeverity::Error;
                    dm.diagnostics.push_back(d);
                    diagnostics.push_back(dm);
                    continue;
                }
                typeEnv.enums[e.name] = e;
                output.enums.push_back(e);
            }
        }

        // Validate type references from typedefs
        for (auto &ts : typesSources)
        {
            for (auto &ref : ts.typeReferences)
            {
                if (!typeEnv.structs.count(ref.typeName) && !typeEnv.enums.count(ref.typeName))
                {
                    DiagnosticLSPMessage dm;
                    dm.uri = ref.uri;
                    Diagnostic d;
                    d.message = "undefined type '" + ref.typeName + "'";
                    d.range.start = {ref.line, ref.col};
                    d.range.end = {ref.line, ref.endCol};
                    d.severity = DiagnosticSeverity::Error;
                    dm.diagnostics.push_back(d);
                    diagnostics.push_back(dm);
                }
            }
        }

        if (diagnostics.size() > diagsBefore)
            return false;

        // Detect recursive types (handles mutual recursion)
        // A field/variant is recursive if its type eventually refers back to
        // the enclosing struct/enum through any chain of struct fields or
        // enum variant payloads.
        auto reachesType = [&](const TypeKind &fieldType,
                               const std::string &targetName) -> bool {
            std::set<std::string> visited;
            std::function<bool(const TypeKind &)> visit = [&](const TypeKind &tk) -> bool {
                if (is_type_named(tk))
                {
                    const std::string &name = named_type_name(tk);
                    if (name == targetName) return true;
                    if (visited.count(name)) return false;
                    visited.insert(name);
                    // Follow struct fields
                    auto sit = typeEnv.structs.find(name);
                    if (sit != typeEnv.structs.end())
                    {
                        for (auto &f : sit->second.fields)
                            if (visit(f.type)) return true;
                    }
                    // Follow enum variant payloads
                    auto eit = typeEnv.enums.find(name);
                    if (eit != typeEnv.enums.end())
                    {
                        for (auto &v : eit->second.variants)
                            if (v.payload.has_value() && visit(*v.payload)) return true;
                    }
                    return false;
                }
                if (const TypeKind *inner = list_inner_type(tk))
                    return visit(*inner);
                if (const TypeKind *inner = optional_inner_type(tk))
                    return visit(*inner);
                return false;
            };
            return visit(fieldType);
        };

        for (auto &[name, s] : typeEnv.structs)
        {
            const std::string structName = name;
            for (auto &f : s.fields)
                f.recursive = reachesType(f.type, structName);
            for (auto &outS : output.structs)
            {
                if (outS.name == name)
                {
                    outS.fields = s.fields;
                    break;
                }
            }
        }

        // Do the same for enums
        for (auto &[name, e] : typeEnv.enums)
        {
            const std::string enumName = name;
            for (auto &v : e.variants)
            {
                if (!v.payload.has_value()) continue;
                v.recursive = reachesType(*v.payload, enumName);
            }
            for (auto &outE : output.enums)
            {
                if (outE.name == name)
                {
                    outE.variants = e.variants;
                    break;
                }
            }
        }

        // Detect types with no finite minimal JSON value.
        std::map<std::string, bool> structFinite;
        std::map<std::string, bool> enumFinite;
        for (const auto &[name, _] : typeEnv.structs) structFinite[name] = false;
        for (const auto &[name, _] : typeEnv.enums) enumFinite[name] = false;

        auto typeHasFiniteValue = [&](const TypeKind &tk) -> bool {
            if (std::holds_alternative<TypeKind_Str>(tk.value) ||
                std::holds_alternative<TypeKind_Int>(tk.value) ||
                std::holds_alternative<TypeKind_Bool>(tk.value))
            {
                return true;
            }
            if (is_type_optional(tk) || is_type_list(tk))
                return true; // null / [] are finite
            if (is_type_named(tk))
            {
                const std::string &name = named_type_name(tk);
                return structFinite[name] || enumFinite[name];
            }
            return false;
        };

        bool changed = true;
        while (changed)
        {
            changed = false;

            for (const auto &[name, s] : typeEnv.structs)
            {
                if (structFinite[name]) continue;
                bool ok = true;
                for (const auto &f : s.fields)
                {
                    if (!typeHasFiniteValue(f.type))
                    {
                        ok = false;
                        break;
                    }
                }
                if (ok)
                {
                    structFinite[name] = true;
                    changed = true;
                }
            }

            for (const auto &[name, e] : typeEnv.enums)
            {
                if (enumFinite[name]) continue;
                bool ok = false;
                for (const auto &v : e.variants)
                {
                    if (!v.payload.has_value() || typeHasFiniteValue(*v.payload))
                    {
                        ok = true;
                        break;
                    }
                }
                if (ok)
                {
                    enumFinite[name] = true;
                    changed = true;
                }
            }
        }

        for (const auto &[name, s] : typeEnv.structs)
        {
            if (structFinite[name]) continue;
            DiagnosticLSPMessage dm;
            dm.uri = s.uri;
            Diagnostic d;
            d.message = "type '" + name + "' has no finite minimal JSON value";
            d.range.start = {s.nameLine, s.nameCol};
            d.range.end = {s.nameLine, s.nameEndCol};
            d.severity = DiagnosticSeverity::Error;
            dm.diagnostics.push_back(d);
            diagnostics.push_back(dm);
        }

        for (const auto &[name, e] : typeEnv.enums)
        {
            if (enumFinite[name]) continue;
            DiagnosticLSPMessage dm;
            dm.uri = e.uri;
            Diagnostic d;
            d.message = "type '" + name + "' has no finite minimal JSON value";
            d.range.start = {e.nameLine, e.nameCol};
            d.range.end = {e.nameLine, e.nameEndCol};
            d.severity = DiagnosticSeverity::Error;
            dm.diagnostics.push_back(d);
            diagnostics.push_back(dm);
        }

        if (diagnostics.size() > diagsBefore)
            return false;

        // Collect all functions and build index
        std::vector<ASTFunction> allFunctions;
        for (auto &tfs : templateSources)
            for (auto &f : tfs)
                allFunctions.push_back(f);

        {
            std::set<std::string> seenFunctionSignatures;
            for (const auto &astFn : allFunctions)
            {
                std::string sig = astFn.name + "(";
                for (size_t pi = 0; pi < astFn.params.size(); pi++)
                {
                    if (pi > 0) sig += ",";
                    sig += astFn.params[pi].typeName;
                }
                sig += ")";

                if (!seenFunctionSignatures.insert(sig).second)
                {
                    DiagnosticLSPMessage dm;
                    dm.uri = astFn.uri;
                    Diagnostic d;
                    d.message = "function '" + astFn.name + "' is already defined";
                    d.range.start = {astFn.nameLine, astFn.nameCol};
                    d.range.end = {astFn.nameLine, astFn.nameEndCol};
                    d.severity = DiagnosticSeverity::Error;
                    dm.diagnostics.push_back(d);
                    diagnostics.push_back(dm);
                }
            }
        }

        if (diagnostics.size() > diagsBefore)
            return false;

        std::map<std::string, int> functionIndices;
        for (size_t i = 0; i < allFunctions.size(); i++)
            functionIndices[allFunctions[i].name] = static_cast<int>(i);

        // Policies
        output.policies = policies;

        // Lower each function
        for (auto &astFn : allFunctions)
        {
            AnalysisContext ctx{typeEnv, diagnostics, astFn.uri};
            ctx.functionIndices = functionIndices;
            ctx.allFunctions = &allFunctions;
            ctx.policies = &policies;

            FunctionDef fd;
            fd.name = astFn.name;
            fd.doc = astFn.doc;

            // Allocate parameter slots
            for (auto &p : astFn.params)
            {
                TypeKind ptype = typeEnv.resolveTypeName(p.typeName);
                // Check for undefined types in params
                if (!typeEnv.isValidType(p.typeName))
                {
                    ctx.error("undefined type '" + p.typeName + "'",
                              p.typeLine, p.typeCol, p.typeEndCol);
                }
                int slot = ctx.allocSlot(p.name, ptype);
                ParamDef pd;
                pd.name = p.name;
                pd.slot = slot;
                pd.type = ptype;
                fd.params.push_back(std::move(pd));
            }

            // Check for function-level policy from params
            for (auto &p : astFn.params)
            {
                if (!p.policy.empty())
                    fd.policy = p.policy;
            }

            // Lower body
            lowerBody(astFn.body, fd.body, ctx);

            fd.slotCount = ctx.nextSlot;
            output.functions.push_back(std::move(fd));
        }

        return diagnostics.size() == diagsBefore;
    }

} // namespace tpp
