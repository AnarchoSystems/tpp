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

        int offset = 0;

        for (const auto &field : sd.fields)
        {
            FieldRange fr;
            fr.name = field.name;
            fr.offset = offset;

            if (!field.type)
            {
                fr.size = 1;
                offset += 1;
                layout.nameIndex[field.name] = static_cast<int>(layout.fieldRanges.size());
                layout.fieldRanges.push_back(std::move(fr));
                continue;
            }

            auto typeIdx = field.type->value.index();

            // Recursive fields are always boxed
            if (field.recursive)
            {
                fr.size = 1;
                fr.isBox = true;
                offset += 1;
                layout.nameIndex[field.name] = static_cast<int>(layout.fieldRanges.size());
                layout.fieldRanges.push_back(std::move(fr));
                continue;
            }

            // List → single slot (pointer to external vector)
            if (typeIdx == 4)
            {
                fr.size = 1;
                fr.isList = true;
                offset += 1;
                layout.nameIndex[field.name] = static_cast<int>(layout.fieldRanges.size());
                layout.fieldRanges.push_back(std::move(fr));
                continue;
            }

            // Optional → flag + inner
            if (typeIdx == 5)
            {
                const auto &inner = std::get<5>(field.type->value);
                int innerSize = inner ? slot_size(*inner) : 1;
                fr.size = 1 + innerSize;
                fr.isOptional = true;
                offset += fr.size;
                layout.nameIndex[field.name] = static_cast<int>(layout.fieldRanges.size());
                layout.fieldRanges.push_back(std::move(fr));
                continue;
            }

            // Scalar or named type
            fr.size = slot_size(*field.type);
            offset += fr.size;
            layout.nameIndex[field.name] = static_cast<int>(layout.fieldRanges.size());
            layout.fieldRanges.push_back(std::move(fr));
        }

        layout.totalSlots = offset;
        layouts_[sd.name] = std::move(layout);
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Enum layout computation
    // ════════════════════════════════════════════════════════════════════════════

    void LayoutTable::compute_enum(const EnumDef &ed, const IR & /*ir*/)
    {
        Layout layout;
        layout.typeName = ed.name;
        layout.isEnum = true;

        // Slot 0 is always the variant tag
        int maxPayloadSize = 0;

        for (const auto &variant : ed.variants)
        {
            CaseRange cr;
            cr.tag = variant.tag;
            cr.payloadOffset = 1; // right after the tag slot

            if (variant.payload)
            {
                cr.hasPayload = true;
                if (variant.recursive)
                {
                    cr.payloadSize = 1; // boxed
                }
                else
                {
                    cr.payloadSize = slot_size(*variant.payload);
                }
            }
            else
            {
                cr.hasPayload = false;
                cr.payloadSize = 0;
            }

            maxPayloadSize = std::max(maxPayloadSize, cr.payloadSize);
            layout.nameIndex[variant.tag] = static_cast<int>(layout.caseRanges.size());
            layout.caseRanges.push_back(std::move(cr));
        }

        layout.totalSlots = 1 + maxPayloadSize; // tag + max payload
        layouts_[ed.name] = std::move(layout);
    }

    // ════════════════════════════════════════════════════════════════════════════
    // LayoutTable::build — construct layouts for all types in the IR
    // ════════════════════════════════════════════════════════════════════════════

    LayoutTable LayoutTable::build(const IR &ir)
    {
        LayoutTable table;

        // Two-pass approach: structs first (since enums may reference them),
        // then enums.  In practice the IR is topologically ordered, but we do
        // structs before enums as a simple heuristic.  Truly mutually-recursive
        // types use boxed (recursive) fields, so their inner size is always 1.

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

} // namespace tpp
