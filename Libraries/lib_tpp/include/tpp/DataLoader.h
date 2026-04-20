#pragma once

#include <tpp/Layout.h>
#include <tpp/Slot.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // DataLoader — converts JSON values into flat Slot frames using Layouts
    //
    // The loader owns all heap allocations (list vectors, box frames, and
    // interned strings for integer/bool-to-string conversions).  The lifetime
    // of a DataLoader must exceed any Slot references it produces.
    //
    // The JSON input is assumed to outlive the slots (string slots point into
    // the JSON's std::string storage).
    // ════════════════════════════════════════════════════════════════════════════

    class DataLoader
    {
      public:
        explicit DataLoader(const LayoutTable &layouts);

        /// Load a JSON object into a freshly-allocated frame using the named
        /// layout.  Returns a frame (vector of Slots) sized to the layout.
        /// Throws std::runtime_error on structural mismatch.
        std::vector<Slot> load(const std::string &typeName,
                               const nlohmann::json &json);

        /// Load a JSON value into a frame sized for the given TypeKind.
        /// Used for list elements and nested values.
        std::vector<Slot> load_typed(const TypeKind &tk,
                                     const nlohmann::json &json);

        /// Load a JSON value into a frame at the given offset using type info.
        void load_field(std::vector<Slot> &frame,
                        int offset,
                        const TypeKind &tk,
                        bool isRecursive,
                        const nlohmann::json &json);

      private:
        const LayoutTable &layouts_;

        // Owned heap storage — kept alive as long as the DataLoader lives
        std::vector<std::unique_ptr<std::vector<Slot>>> owned_vectors_;
        std::vector<std::unique_ptr<std::string>> owned_strings_;

        void load_struct(std::vector<Slot> &frame,
                         const Layout &layout,
                         const nlohmann::json &json);

        void load_enum(std::vector<Slot> &frame,
                       const Layout &layout,
                       const nlohmann::json &json);

        void load_scalar(std::vector<Slot> &frame,
                         int offset,
                         const TypeKind &tk,
                         const nlohmann::json &json);

        void load_json_value(std::vector<Slot> &frame,
                             int offset,
                             const nlohmann::json &json);

        void load_json_value_into_list(std::vector<Slot> &list,
                                       const nlohmann::json &elem);

        // Allocate a list vector and track ownership
        std::vector<Slot> *alloc_list();

        // Allocate a box frame and track ownership
        std::vector<Slot> *alloc_box(int size);

        // Intern a string (for storing stringified ints/bools)
        const std::string *intern(std::string s);
    };

} // namespace tpp
