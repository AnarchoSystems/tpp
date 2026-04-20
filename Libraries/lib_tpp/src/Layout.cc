#include <tpp/Layout.h>

#include <algorithm>
#include <stdexcept>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // TypeKind → slot size
    // ════════════════════════════════════════════════════════════════════════════

    int LayoutTable::compute_type_size(const TypeKind &tk) const
    {
        // variant index:
        //   0 = Str, 1 = Int, 2 = Bool  → 1 scalar slot
        //   3 = Named(string)            → lookup struct/enum layout size
        //   4 = List(TypeKind)           → 1 slot (pointer to list vector)
        //   5 = Optional(TypeKind)       → 1 (flag) + inner size
        return std::visit(
            [this](const auto &v) -> int
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, TypeKind_Str> ||
                              std::is_same_v<T, TypeKind_Int> ||
                              std::is_same_v<T, TypeKind_Bool>)
                {
                    return 1;
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    // Named type — look up its layout
                    const Layout *layout = find(v);
                    if (layout)
                        return layout->totalSlots;
                    // Unknown type — treat as 1 slot (shouldn't happen with valid IR)
                    return 1;
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<TypeKind>>)
                {
                    // Could be List or Optional — distinguished by variant index
                    // But since both List and Optional are unique_ptr<TypeKind>,
                    // we need the parent variant index. This path is reached
                    // generically; for precise dispatch, see slot_size().
                    //
                    // List → 1 slot (boxed)
                    // Optional → 1 + inner
                    // We can't distinguish here, so this shouldn't be called directly.
                    // Return 1 as a safe fallback (list case).
                    return 1;
                }
                else
                {
                    return 1;
                }
            },
            tk.value);
    }

    int LayoutTable::slot_size(const TypeKind &tk) const
    {
        auto idx = tk.value.index();
        switch (idx)
        {
        case 0: // Str
        case 1: // Int
        case 2: // Bool
            return 1;
        case 3: // Named(string)
        {
            const auto &name = std::get<3>(tk.value);
            const Layout *layout = find(name);
            return layout ? layout->totalSlots : 1;
        }
        case 4: // List(TypeKind) — boxed, one pointer slot
            return 1;
        case 5: // Optional(TypeKind) — flag + inner
        {
            const auto &inner = std::get<5>(tk.value);
            if (!inner)
                return 1;
            return 1 + slot_size(*inner);
        }
        default:
            return 1;
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Struct layout computation
    // ════════════════════════════════════════════════════════════════════════════

    void LayoutTable::compute_struct(const StructDef &sd, const IR & /*ir*/)
    {
        Layout layout;
        layout.typeName = sd.name;
        layout.isEnum = false;
        layout.totalSlots = sd.slotCount;

        for (const auto &field : sd.fields)
        {
            FieldRange fr;
            fr.name = field.name;
            fr.offset = field.slotOffset;
            fr.size = field.slotSize;

            if (field.type)
            {
                auto typeIdx = field.type->value.index();
                fr.isBox = field.recursive;
                fr.isList = (!field.recursive && typeIdx == 4);
                fr.isOptional = (!field.recursive && typeIdx == 5);
                fr.type = field.type.get();
            }

            layout.nameIndex[field.name] = static_cast<int>(layout.fieldRanges.size());
            layout.fieldRanges.push_back(std::move(fr));
        }

        layouts_[sd.name] = std::move(layout);
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Enum layout — read precomputed data from IR
    // ════════════════════════════════════════════════════════════════════════════

    void LayoutTable::compute_enum(const EnumDef &ed, const IR & /*ir*/)
    {
        Layout layout;
        layout.typeName = ed.name;
        layout.isEnum = true;
        layout.totalSlots = ed.slotCount;

        for (const auto &variant : ed.variants)
        {
            CaseRange cr;
            cr.tag = variant.tag;
            cr.payloadOffset = 1;
            cr.payloadSize = variant.payloadSlotSize;
            cr.hasPayload = (variant.payload != nullptr);
            cr.isRecursive = variant.recursive;
            cr.payloadType = variant.payload.get();

            layout.nameIndex[variant.tag] = static_cast<int>(layout.caseRanges.size());
            layout.caseRanges.push_back(std::move(cr));
        }

        layouts_[ed.name] = std::move(layout);
    }

    // ════════════════════════════════════════════════════════════════════════════
    // LayoutTable::build — construct layouts from precomputed IR data
    // ════════════════════════════════════════════════════════════════════════════

    LayoutTable LayoutTable::build(const IR &ir)
    {
        LayoutTable table;

        // Read precomputed layout data from the IR fields.
        // compute_ir_layouts() already filled slotOffset/slotSize/slotCount
        // during compilation.

        for (const auto &sd : ir.structs)
        {
            table.compute_struct(sd, ir);
        }
        for (const auto &ed : ir.enums)
        {
            table.compute_enum(ed, ir);
        }

        return table;
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Lookup
    // ════════════════════════════════════════════════════════════════════════════

    const Layout *LayoutTable::find(const std::string &name) const
    {
        auto it = layouts_.find(name);
        if (it == layouts_.end())
            return nullptr;
        return &it->second;
    }

    // ════════════════════════════════════════════════════════════════════════════
    // compute_ir_layouts — fill precomputed slot fields on IR type definitions
    // ════════════════════════════════════════════════════════════════════════════

    static int ir_slot_size(const TypeKind &tk,
                            const std::unordered_map<std::string, int> &sizes)
    {
        auto idx = tk.value.index();
        switch (idx)
        {
        case 0: // Str
        case 1: // Int
        case 2: // Bool
            return 1;
        case 3: // Named(string)
        {
            const auto &name = std::get<3>(tk.value);
            auto it = sizes.find(name);
            return it != sizes.end() ? it->second : 1;
        }
        case 4: // List — boxed, one slot
            return 1;
        case 5: // Optional — flag + inner
        {
            const auto &inner = std::get<5>(tk.value);
            if (!inner)
                return 1;
            return 1 + ir_slot_size(*inner, sizes);
        }
        default:
            return 1;
        }
    }

    void compute_ir_layouts(IR &ir)
    {
        // Map type name → total slot count, built up as we go.
        std::unordered_map<std::string, int> sizes;

        // Pass 1: compute enum slot counts first (structs may reference enums).
        for (auto &ed : ir.enums)
        {
            int maxPayload = 0;
            for (auto &variant : ed.variants)
            {
                if (variant.payload)
                {
                    if (variant.recursive)
                        variant.payloadSlotSize = 1; // boxed
                    else
                        variant.payloadSlotSize = ir_slot_size(*variant.payload, sizes);
                }
                else
                {
                    variant.payloadSlotSize = 0;
                }
                maxPayload = std::max(maxPayload, variant.payloadSlotSize);
            }
            ed.slotCount = 1 + maxPayload; // tag + max payload
            sizes[ed.name] = ed.slotCount;
        }

        // Pass 2: compute struct slot counts (now enum sizes are known).
        for (auto &sd : ir.structs)
        {
            int offset = 0;
            for (auto &field : sd.fields)
            {
                field.slotOffset = offset;

                if (!field.type)
                {
                    field.slotSize = 1;
                }
                else if (field.recursive)
                {
                    field.slotSize = 1; // boxed
                }
                else if (field.type->value.index() == 4) // List
                {
                    field.slotSize = 1; // boxed
                }
                else if (field.type->value.index() == 5) // Optional
                {
                    const auto &inner = std::get<5>(field.type->value);
                    int innerSize = inner ? ir_slot_size(*inner, sizes) : 1;
                    field.slotSize = 1 + innerSize; // flag + inner
                }
                else
                {
                    field.slotSize = ir_slot_size(*field.type, sizes);
                }

                offset += field.slotSize;
            }
            sd.slotCount = offset;
            sizes[sd.name] = offset;
        }

        // Pass 3: recompute enum payloads that reference structs (now struct sizes are known).
        for (auto &ed : ir.enums)
        {
            int maxPayload = 0;
            for (auto &variant : ed.variants)
            {
                if (variant.payload)
                {
                    if (variant.recursive)
                        variant.payloadSlotSize = 1; // boxed
                    else
                        variant.payloadSlotSize = ir_slot_size(*variant.payload, sizes);
                }
                else
                {
                    variant.payloadSlotSize = 0;
                }
                maxPayload = std::max(maxPayload, variant.payloadSlotSize);
            }
            ed.slotCount = 1 + maxPayload; // tag + max payload
            sizes[ed.name] = ed.slotCount;
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    // compute_instruction_slots — resolve path-based expressions to slot offsets
    // ════════════════════════════════════════════════════════════════════════════

    struct SlotScope
    {
        struct Entry
        {
            std::string name;
            int slotOffset;
            int slotCount;
            const TypeKind *type; // non-owning, for field resolution
        };

        std::vector<Entry> entries;

        void push(const std::string &name, int offset, int count, const TypeKind *type)
        {
            entries.push_back({name, offset, count, type});
        }

        void pop() { entries.pop_back(); }

        const Entry *find(const std::string &name) const
        {
            for (auto it = entries.rbegin(); it != entries.rend(); ++it)
                if (it->name == name)
                    return &*it;
            return nullptr;
        }
    };

    // Resolve a dot-separated path to a slot offset
    static int resolve_path_full(const std::string &path, const SlotScope &scope,
                                 const LayoutTable &layouts, const IR &ir)
    {
        auto dot = path.find('.');
        std::string root = (dot == std::string::npos) ? path : path.substr(0, dot);

        const auto *entry = scope.find(root);
        if (!entry)
            return 0;

        int offset = entry->slotOffset;

        if (dot == std::string::npos)
            return offset;

        // Walk remaining segments using TypeKind to navigate
        const TypeKind *currentType = entry->type;
        size_t pos = dot + 1;

        while (pos < path.size() && currentType)
        {
            auto nextDot = path.find('.', pos);
            std::string segment = (nextDot == std::string::npos)
                                      ? path.substr(pos)
                                      : path.substr(pos, nextDot - pos);

            auto typeIdx = currentType->value.index();

            if (typeIdx == 5) // Optional — skip the flag slot
            {
                offset += 1;
                currentType = std::get<5>(currentType->value).get();
                continue; // don't consume segment
            }

            if (typeIdx == 3) // Named(string) — look up struct
            {
                const auto &typeName = std::get<3>(currentType->value);

                // Find struct in IR
                const StructDef *sd = nullptr;
                for (const auto &s : ir.structs)
                    if (s.name == typeName)
                    {
                        sd = &s;
                        break;
                    }
                if (!sd)
                    break;

                // Find field
                const FieldDef *fd = nullptr;
                for (const auto &f : sd->fields)
                    if (f.name == segment)
                    {
                        fd = &f;
                        break;
                    }
                if (!fd)
                    break;

                offset += fd->slotOffset;
                currentType = fd->type.get();

                pos = (nextDot == std::string::npos) ? path.size() : nextDot + 1;
                continue;
            }

            break; // can't resolve further (scalar, list, etc.)
        }

        return offset;
    }

    static void resolve_expr(ExprInfo &expr, const SlotScope &scope,
                             const LayoutTable &layouts, const IR &ir)
    {
        expr.slotOffset = resolve_path_full(expr.path, scope, layouts, ir);
    }

    static void resolve_instructions(std::vector<Instruction> &instrs,
                                     SlotScope &scope,
                                     const LayoutTable &layouts,
                                     const IR &ir,
                                     int &nextSlot,
                                     int &maxSlot);

    static void resolve_instructions(std::vector<Instruction> &instrs,
                                     SlotScope &scope,
                                     const LayoutTable &layouts,
                                     const IR &ir,
                                     int &nextSlot,
                                     int &maxSlot)
    {
        for (auto &instr : instrs)
        {
            switch (instr.value.index())
            {
            case 0: // EmitInstr — no expressions
            case 2: // AlignCell — no expressions
                break;

            case 1: // EmitExprInstr
            {
                auto &emit = std::get<1>(instr.value);
                resolve_expr(emit.expr, scope, layouts, ir);
                break;
            }

            case 3: // ForInstr
            {
                auto &forInstr = *std::get<3>(instr.value);
                resolve_expr(forInstr.collection, scope, layouts, ir);

                // Determine element type and size
                const TypeKind *collType = forInstr.collection.type.get();
                int elemSize = 1;
                const TypeKind *elemType = nullptr;

                // Unwrap Optional<List<T>> to get List<T>
                if (collType && collType->value.index() == 5) // Optional
                    collType = std::get<5>(collType->value).get();

                if (collType && collType->value.index() == 4) // List<T>
                {
                    elemType = std::get<4>(collType->value).get();
                    if (elemType)
                        elemSize = layouts.slot_size(*elemType);
                }

                forInstr.elementSlotCount = elemSize;

                // Allocate slots for loop var
                int savedNext = nextSlot;
                forInstr.varSlotOffset = nextSlot;
                nextSlot += elemSize;

                // Allocate enumerator slot if present
                if (forInstr.enumeratorName.has_value())
                {
                    forInstr.enumeratorSlotOffset = nextSlot;
                    nextSlot += 1; // enumerator is int → 1 slot
                }
                else
                {
                    forInstr.enumeratorSlotOffset = -1;
                }

                if (nextSlot > maxSlot)
                    maxSlot = nextSlot;

                // Push loop variable into scope
                scope.push(forInstr.varName, forInstr.varSlotOffset, elemSize, elemType);
                if (forInstr.enumeratorName.has_value())
                    scope.push(*forInstr.enumeratorName, forInstr.enumeratorSlotOffset, 1, nullptr);

                if (forInstr.body)
                    resolve_instructions(*forInstr.body, scope, layouts, ir, nextSlot, maxSlot);

                // Pop scope entries
                if (forInstr.enumeratorName.has_value())
                    scope.pop();
                scope.pop();

                nextSlot = savedNext;
                break;
            }

            case 4: // IfInstr
            {
                auto &ifInstr = *std::get<4>(instr.value);
                resolve_expr(ifInstr.condExpr, scope, layouts, ir);

                if (ifInstr.thenBody)
                    resolve_instructions(*ifInstr.thenBody, scope, layouts, ir, nextSlot, maxSlot);
                if (ifInstr.elseBody)
                    resolve_instructions(*ifInstr.elseBody, scope, layouts, ir, nextSlot, maxSlot);
                break;
            }

            case 5: // SwitchInstr
            {
                auto &switchInstr = *std::get<5>(instr.value);
                resolve_expr(switchInstr.expr, scope, layouts, ir);

                // Find the enum type for variant index resolution
                const TypeKind *switchType = switchInstr.expr.type.get();
                std::string enumName;
                if (switchType && switchType->value.index() == 3)
                    enumName = std::get<3>(switchType->value);

                const Layout *enumLayout = enumName.empty() ? nullptr : layouts.find(enumName);

                if (switchInstr.cases)
                {
                    for (auto &caseInstr : *switchInstr.cases)
                    {
                        // Set variant index
                        if (enumLayout)
                        {
                            caseInstr.variantIndex = enumLayout->case_index(caseInstr.tag);
                            auto it = enumLayout->nameIndex.find(caseInstr.tag);
                            if (it != enumLayout->nameIndex.end())
                            {
                                const auto &cr = enumLayout->caseRanges[static_cast<size_t>(it->second)];
                                if (cr.isRecursive && cr.payloadType)
                                {
                                    // Recursive payload is a BoxRef (1 slot) in the variant,
                                    // but the binding needs the unboxed size.
                                    caseInstr.payloadSlotCount = layouts.slot_size(*cr.payloadType);
                                }
                                else
                                {
                                    caseInstr.payloadSlotCount = cr.payloadSize;
                                }
                            }
                        }

                        int savedNext = nextSlot;

                        // If there's a binding, allocate slots for it
                        if (caseInstr.bindingName.has_value())
                        {
                            caseInstr.bindingSlotOffset = nextSlot;
                            nextSlot += caseInstr.payloadSlotCount > 0 ? caseInstr.payloadSlotCount : 1;

                            if (nextSlot > maxSlot)
                                maxSlot = nextSlot;

                            // Push binding into scope
                            scope.push(caseInstr.bindingName->name,
                                       caseInstr.bindingSlotOffset,
                                       caseInstr.payloadSlotCount,
                                       caseInstr.payloadType.get());
                        }
                        else
                        {
                            caseInstr.bindingSlotOffset = -1;
                        }

                        if (caseInstr.body)
                            resolve_instructions(*caseInstr.body, scope, layouts, ir, nextSlot, maxSlot);

                        if (caseInstr.bindingName.has_value())
                            scope.pop();

                        nextSlot = savedNext;
                    }
                }
                break;
            }

            case 6: // CallInstr
            {
                auto &call = std::get<6>(instr.value);
                for (auto &arg : call.arguments)
                    resolve_expr(arg, scope, layouts, ir);
                break;
            }

            default:
                break;
            }
        }
    }

    void compute_instruction_slots(IR &ir)
    {
        LayoutTable layouts = LayoutTable::build(ir);

        for (auto &fn : ir.functions)
        {
            SlotScope scope;
            int nextSlot = 0;

            // Assign param slot offsets
            for (auto &param : fn.params)
            {
                param.slotOffset = nextSlot;
                param.slotCount = param.type ? layouts.slot_size(*param.type) : 1;
                scope.push(param.name, param.slotOffset, param.slotCount, param.type.get());
                nextSlot += param.slotCount;
            }

            int maxSlot = nextSlot;

            if (fn.body)
                resolve_instructions(*fn.body, scope, layouts, ir, nextSlot, maxSlot);

            fn.frameSlotCount = maxSlot;
        }
    }

} // namespace tpp
