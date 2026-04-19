#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // Slot — the VM's atomic unit of data
    //
    // Every value in a VM frame is stored as a Slot.  Slots are densely packed
    // in contiguous arrays (frames and list vectors).  The kind tag tells the
    // VM how to interpret the payload.
    // ════════════════════════════════════════════════════════════════════════════

    enum class SlotKind : uint8_t
    {
        Str,          // payload.str — pointer to externally-owned string data
        Int,          // payload.i64
        Bool,         // payload.b
        List,         // payload.list — pointer to a vector of element slots
        OptionalFlag, // payload.b — true = present, false = absent
        VariantTag,   // payload.i32 — case index
        BoxRef,       // payload.box — pointer to a heap-allocated sub-frame
        Empty,        // uninitialised / padding
    };

    struct Slot
    {
        SlotKind kind = SlotKind::Empty;

        union Payload
        {
            const std::string *str;         // Str: externally owned (JSON input or string table)
            int64_t i64;                    // Int
            bool b;                         // Bool, OptionalFlag
            std::vector<Slot> *list;        // List: owned by DataLoader
            int32_t i32;                    // VariantTag
            std::vector<Slot> *box;         // BoxRef: heap-allocated sub-frame

            Payload() : i64(0) {}
        } payload;

        Slot() = default;

        static Slot make_str(const std::string *s)
        {
            Slot slot;
            slot.kind = SlotKind::Str;
            slot.payload.str = s;
            return slot;
        }

        static Slot make_int(int64_t v)
        {
            Slot slot;
            slot.kind = SlotKind::Int;
            slot.payload.i64 = v;
            return slot;
        }

        static Slot make_bool(bool v)
        {
            Slot slot;
            slot.kind = SlotKind::Bool;
            slot.payload.b = v;
            return slot;
        }

        static Slot make_list(std::vector<Slot> *v)
        {
            Slot slot;
            slot.kind = SlotKind::List;
            slot.payload.list = v;
            return slot;
        }

        static Slot make_optional_flag(bool present)
        {
            Slot slot;
            slot.kind = SlotKind::OptionalFlag;
            slot.payload.b = present;
            return slot;
        }

        static Slot make_variant_tag(int32_t tag)
        {
            Slot slot;
            slot.kind = SlotKind::VariantTag;
            slot.payload.i32 = tag;
            return slot;
        }

        static Slot make_box(std::vector<Slot> *frame)
        {
            Slot slot;
            slot.kind = SlotKind::BoxRef;
            slot.payload.box = frame;
            return slot;
        }

        // ── Accessors (unchecked — caller must verify kind) ─────────

        const std::string &as_str() const { return *payload.str; }
        int64_t as_int() const { return payload.i64; }
        bool as_bool() const { return payload.b; }
        std::vector<Slot> &as_list() const { return *payload.list; }
        bool as_optional_flag() const { return payload.b; }
        int32_t as_variant_tag() const { return payload.i32; }
        std::vector<Slot> &as_box() const { return *payload.box; }

        // ── String conversion (for emission) ────────────────────────

        std::string to_string() const
        {
            switch (kind)
            {
            case SlotKind::Str:
                return *payload.str;
            case SlotKind::Int:
                return std::to_string(payload.i64);
            case SlotKind::Bool:
                return payload.b ? "true" : "false";
            default:
                return "";
            }
        }
    };

} // namespace tpp
