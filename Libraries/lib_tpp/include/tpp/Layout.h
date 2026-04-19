#pragma once

#include <tpp/IR.h>
#include <tpp/Slot.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // Layout — describes how a type's fields are packed into a flat Slot array
    //
    // Each StructDef and EnumDef maps to one Layout.  Scalar fields occupy one
    // slot.  Optionals occupy N+1 slots (flag + inner).  Variants occupy 1+max
    // slots (tag + largest case).  Lists and recursive fields are boxed
    // (one slot pointing to a heap-allocated vector of Slots).
    //
    // Layouts are computed once from the IR and cached in a LayoutTable.
    // ════════════════════════════════════════════════════════════════════════════

    // ── SlotDescriptor — compile-time metadata for one logical field ────────

    struct SlotDescriptor
    {
        std::string name;    // field/variant-case name (for diagnostics)
        int offset = 0;      // offset into the owning frame (slot index)
        int size = 1;         // number of Slot cells occupied
        SlotKind kind = SlotKind::Empty;
    };

    // ── FieldRange — contiguous span of slots for one struct field ──────────

    struct FieldRange
    {
        std::string name;     // field name
        int offset = 0;       // slot offset in parent frame
        int size = 1;         // number of slots
        bool isOptional = false;   // if true, slot[offset] is OptionalFlag
        bool isList = false;       // if true, slot[offset] is List
        bool isBox = false;        // if true, slot[offset] is BoxRef (recursive)
    };

    // ── CaseRange — describes one variant case in an enum layout ────────────

    struct CaseRange
    {
        std::string tag;          // variant tag name
        int payloadOffset = 0;    // where payload slots begin (after tag slot)
        int payloadSize = 0;      // number of payload slots for this case
        bool hasPayload = false;
    };

    // ── Layout ──────────────────────────────────────────────────────────────

    struct Layout
    {
        std::string typeName;                 // struct or enum name
        int totalSlots = 0;                   // total frame size
        std::vector<FieldRange> fieldRanges;  // struct fields (empty for enums)
        std::vector<CaseRange> caseRanges;    // enum cases (empty for structs)
        bool isEnum = false;

        // Quick lookup: field/case name → index in fieldRanges/caseRanges
        std::unordered_map<std::string, int> nameIndex;

        int field_offset(const std::string &name) const
        {
            auto it = nameIndex.find(name);
            if (it == nameIndex.end())
                return -1;
            return fieldRanges[static_cast<size_t>(it->second)].offset;
        }

        int case_index(const std::string &tag) const
        {
            auto it = nameIndex.find(tag);
            if (it == nameIndex.end())
                return -1;
            return it->second;
        }
    };

    // ── LayoutTable — owns all computed layouts, keyed by type name ─────────

    class LayoutTable
    {
      public:
        /// Build a LayoutTable from an IR.  Computes layouts for every struct
        /// and enum definition.
        static LayoutTable build(const IR &ir);

        /// Look up a layout by type name.  Returns nullptr if not found.
        const Layout *find(const std::string &name) const;

        /// Compute the slot size needed for an arbitrary TypeKind.
        /// Named types use their layout; scalars return 1; lists/boxes return 1.
        int slot_size(const TypeKind &tk) const;

      private:
        std::unordered_map<std::string, Layout> layouts_;

        void compute_struct(const StructDef &sd, const IR &ir);
        void compute_enum(const EnumDef &ed, const IR &ir);
        int compute_type_size(const TypeKind &tk) const;
    };

} // namespace tpp
