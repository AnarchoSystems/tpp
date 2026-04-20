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

        // Structs first (enums may reference them via payload types).
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

        // Enums second.
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

} // namespace tpp
