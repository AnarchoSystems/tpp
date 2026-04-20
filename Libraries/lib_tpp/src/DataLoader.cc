#include <tpp/DataLoader.h>

#include <stdexcept>

namespace tpp
{

    DataLoader::DataLoader(const LayoutTable &layouts)
        : layouts_(layouts)
    {
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Heap allocation helpers
    // ════════════════════════════════════════════════════════════════════════════

    std::vector<Slot> *DataLoader::alloc_list()
    {
        owned_vectors_.push_back(std::make_unique<std::vector<Slot>>());
        return owned_vectors_.back().get();
    }

    std::vector<Slot> *DataLoader::alloc_box(int size)
    {
        auto v = std::make_unique<std::vector<Slot>>(static_cast<size_t>(size));
        auto *ptr = v.get();
        owned_vectors_.push_back(std::move(v));
        return ptr;
    }

    const std::string *DataLoader::intern(std::string s)
    {
        owned_strings_.push_back(std::make_unique<std::string>(std::move(s)));
        return owned_strings_.back().get();
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Public entry points
    // ════════════════════════════════════════════════════════════════════════════

    std::vector<Slot> DataLoader::load(const std::string &typeName,
                                       const nlohmann::json &json)
    {
        const Layout *layout = layouts_.find(typeName);
        if (!layout)
            throw std::runtime_error("DataLoader: unknown type '" + typeName + "'");

        std::vector<Slot> frame(static_cast<size_t>(layout->totalSlots));

        if (layout->isEnum)
            load_enum(frame, *layout, json);
        else
            load_struct(frame, *layout, json);

        return frame;
    }

    std::vector<Slot> DataLoader::load_typed(const TypeKind &tk,
                                             const nlohmann::json &json)
    {
        auto idx = tk.value.index();
        switch (idx)
        {
        case 0: // Str
        case 1: // Int
        case 2: // Bool
        {
            std::vector<Slot> frame(1);
            load_scalar(frame, 0, tk, json);
            return frame;
        }
        case 3: // Named
        {
            const auto &name = std::get<3>(tk.value);
            return load(name, json);
        }
        case 4: // List
        {
            const auto &elemType = std::get<4>(tk.value);
            auto *list = alloc_list();
            if (json.is_array())
            {
                for (const auto &elem : json)
                {
                    if (elemType)
                    {
                        auto elemFrame = load_typed(*elemType, elem);
                        for (auto &s : elemFrame)
                            list->push_back(std::move(s));
                    }
                    else
                    {
                        load_json_value_into_list(*list, elem);
                    }
                }
            }
            std::vector<Slot> frame(1);
            frame[0] = Slot::make_list(list);
            return frame;
        }
        case 5: // Optional
        {
            const auto &inner = std::get<5>(tk.value);
            if (!inner)
            {
                std::vector<Slot> frame(1);
                frame[0] = Slot::make_optional_flag(false);
                return frame;
            }
            int innerSize = layouts_.slot_size(*inner);
            std::vector<Slot> frame(static_cast<size_t>(1 + innerSize));
            if (json.is_null())
            {
                frame[0] = Slot::make_optional_flag(false);
            }
            else
            {
                frame[0] = Slot::make_optional_flag(true);
                auto inner_frame = load_typed(*inner, json);
                for (int i = 0; i < innerSize && i < static_cast<int>(inner_frame.size()); ++i)
                    frame[static_cast<size_t>(1 + i)] = inner_frame[static_cast<size_t>(i)];
            }
            return frame;
        }
        default:
        {
            std::vector<Slot> frame(1);
            return frame;
        }
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Struct loading
    // ════════════════════════════════════════════════════════════════════════════

    void DataLoader::load_struct(std::vector<Slot> &frame,
                                 const Layout &layout,
                                 const nlohmann::json &json)
    {
        if (!json.is_object())
            throw std::runtime_error("DataLoader: expected JSON object for struct '" +
                                     layout.typeName + "'");

        for (const auto &fr : layout.fieldRanges)
        {
            auto it = json.find(fr.name);
            bool present = (it != json.end() && !it->is_null());

            // If we have full type info, use type-aware load_field
            if (fr.type)
            {
                if (!present)
                {
                    // For Optional types, set flag to false
                    if (fr.isOptional)
                        frame[static_cast<size_t>(fr.offset)] = Slot::make_optional_flag(false);
                    // Otherwise leave slots as Empty
                    continue;
                }
                load_field(frame, fr.offset, *fr.type, fr.isBox, *it);
                continue;
            }

            // Fallback: type-unaware loading (legacy path)
            if (fr.isOptional)
            {
                frame[static_cast<size_t>(fr.offset)] = Slot::make_optional_flag(present);
                if (present)
                    load_json_value(frame, fr.offset + 1, *it);
                continue;
            }

            if (!present)
                continue; // leave as Empty

            if (fr.isList)
            {
                auto *list = alloc_list();
                frame[static_cast<size_t>(fr.offset)] = Slot::make_list(list);
                if (it->is_array())
                {
                    for (const auto &elem : *it)
                    {
                        load_json_value_into_list(*list, elem);
                    }
                }
                continue;
            }

            if (fr.isBox)
            {
                // Recursive field: box the inner value
                auto *box = alloc_box(fr.size);
                frame[static_cast<size_t>(fr.offset)] = Slot::make_box(box);
                load_json_value(*box, 0, *it);
                continue;
            }

            // Regular scalar or inline struct
            load_json_value(frame, fr.offset, *it);
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Enum loading
    // ════════════════════════════════════════════════════════════════════════════

    void DataLoader::load_enum(std::vector<Slot> &frame,
                               const Layout &layout,
                               const nlohmann::json &json)
    {
        // Enum JSON formats:
        //   1. Just a string: "CaseName"
        //   2. Key-value: { "CaseName": <payload> }  (tpp canonical format)
        //   3. Tagged: { "tag": "CaseName", "value": <payload> }
        std::string tag;
        const nlohmann::json *payloadJson = nullptr;

        if (json.is_string())
        {
            tag = json.get<std::string>();
        }
        else if (json.is_object())
        {
            if (json.contains("tag"))
            {
                // Format 3: { "tag": ..., "value": ... }
                tag = json["tag"].get<std::string>();
                if (json.contains("value") && !json["value"].is_null())
                    payloadJson = &json["value"];
            }
            else
            {
                // Format 2: { "CaseName": <payload> }
                for (auto it = json.begin(); it != json.end(); ++it)
                {
                    tag = it.key();
                    if (!it->is_null() && !(it->is_object() && it->empty()))
                        payloadJson = &it.value();
                    break; // use first key
                }
            }
        }
        else
        {
            throw std::runtime_error("DataLoader: expected string or object for enum '" +
                                     layout.typeName + "'");
        }

        int caseIdx = layout.case_index(tag);
        if (caseIdx < 0)
            throw std::runtime_error("DataLoader: unknown variant '" + tag +
                                     "' for enum '" + layout.typeName + "'");

        frame[0] = Slot::make_variant_tag(static_cast<int32_t>(caseIdx));

        const auto &cr = layout.caseRanges[static_cast<size_t>(caseIdx)];
        if (cr.hasPayload && payloadJson)
        {
            if (cr.payloadType)
                load_field(frame, cr.payloadOffset, *cr.payloadType, cr.isRecursive, *payloadJson);
            else
                load_json_value(frame, cr.payloadOffset, *payloadJson);
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Scalar / JSON-value loading helpers
    // ════════════════════════════════════════════════════════════════════════════

    void DataLoader::load_scalar(std::vector<Slot> &frame,
                                 int offset,
                                 const TypeKind &tk,
                                 const nlohmann::json &json)
    {
        auto idx = tk.value.index();
        auto off = static_cast<size_t>(offset);

        switch (idx)
        {
        case 0: // Str
            if (json.is_string())
                frame[off] = Slot::make_str(&json.get_ref<const std::string &>());
            else
                frame[off] = Slot::make_str(intern(json.dump()));
            break;
        case 1: // Int
            frame[off] = Slot::make_int(json.get<int64_t>());
            break;
        case 2: // Bool
            frame[off] = Slot::make_bool(json.get<bool>());
            break;
        default:
            break;
        }
    }

    // ── Type-unaware JSON-to-slot (infers from JSON value type) ─────────────

    void DataLoader::load_json_value(std::vector<Slot> &frame,
                                     int offset,
                                     const nlohmann::json &json)
    {
        auto off = static_cast<size_t>(offset);

        if (json.is_string())
        {
            frame[off] = Slot::make_str(&json.get_ref<const std::string &>());
        }
        else if (json.is_number_integer())
        {
            frame[off] = Slot::make_int(json.get<int64_t>());
        }
        else if (json.is_boolean())
        {
            frame[off] = Slot::make_bool(json.get<bool>());
        }
        else if (json.is_object())
        {
            // Inline struct — we'd need layout info to do this properly.
            // For now, store the first-level fields starting at offset.
            // This will be enhanced when we integrate with type-aware loading.
            // For a single-field inline, just store the value.
        }
        else if (json.is_null())
        {
            // Leave as Empty
        }
    }

    void DataLoader::load_json_value_into_list(std::vector<Slot> &list,
                                               const nlohmann::json &elem)
    {
        if (elem.is_string())
        {
            list.push_back(Slot::make_str(&elem.get_ref<const std::string &>()));
        }
        else if (elem.is_number_integer())
        {
            list.push_back(Slot::make_int(elem.get<int64_t>()));
        }
        else if (elem.is_boolean())
        {
            list.push_back(Slot::make_bool(elem.get<bool>()));
        }
        else
        {
            list.push_back(Slot());
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Field loading (type-aware, dispatches on FieldDef information)
    // ════════════════════════════════════════════════════════════════════════════

    void DataLoader::load_field(std::vector<Slot> &frame,
                                int offset,
                                const TypeKind &tk,
                                bool isRecursive,
                                const nlohmann::json &json)
    {
        if (isRecursive)
        {
            // Recursive field — box it
            int innerSize = layouts_.slot_size(tk);
            auto *box = alloc_box(innerSize);
            frame[static_cast<size_t>(offset)] = Slot::make_box(box);
            auto inner = load_typed(tk, json);
            for (int i = 0; i < innerSize && i < static_cast<int>(inner.size()); ++i)
                (*box)[static_cast<size_t>(i)] = inner[static_cast<size_t>(i)];
            return;
        }

        auto idx = tk.value.index();

        switch (idx)
        {
        case 0: // Str
        case 1: // Int
        case 2: // Bool
            load_scalar(frame, offset, tk, json);
            break;

        case 3: // Named
        {
            const auto &name = std::get<3>(tk.value);
            const Layout *layout = layouts_.find(name);
            if (!layout)
            {
                load_json_value(frame, offset, json);
                break;
            }
            auto inner = load(name, json);
            for (int i = 0; i < layout->totalSlots && i < static_cast<int>(inner.size()); ++i)
                frame[static_cast<size_t>(offset + i)] = inner[static_cast<size_t>(i)];
            break;
        }

        case 4: // List
        {
            const auto &elemType = std::get<4>(tk.value);
            auto *list = alloc_list();
            frame[static_cast<size_t>(offset)] = Slot::make_list(list);
            if (json.is_array())
            {
                for (const auto &elem : json)
                {
                    if (elemType)
                    {
                        auto elemFrame = load_typed(*elemType, elem);
                        for (auto &s : elemFrame)
                            list->push_back(std::move(s));
                    }
                    else
                    {
                        load_json_value_into_list(*list, elem);
                    }
                }
            }
            break;
        }

        case 5: // Optional
        {
            const auto &inner = std::get<5>(tk.value);
            if (json.is_null())
            {
                frame[static_cast<size_t>(offset)] = Slot::make_optional_flag(false);
            }
            else
            {
                frame[static_cast<size_t>(offset)] = Slot::make_optional_flag(true);
                if (inner)
                {
                    auto innerFrame = load_typed(*inner, json);
                    for (int i = 0; i < static_cast<int>(innerFrame.size()); ++i)
                        frame[static_cast<size_t>(offset + 1 + i)] = innerFrame[static_cast<size_t>(i)];
                }
            }
            break;
        }

        default:
            break;
        }
    }

} // namespace tpp
