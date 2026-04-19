#include <gtest/gtest.h>

#include <tpp/Layout.h>
#include <tpp/DataLoader.h>
#include <tpp/IR.h>
#include <tpp/Slot.h>

#include <nlohmann/json.hpp>

namespace
{

using namespace tpp;

// ════════════════════════════════════════════════════════════════════════════
// Helper: build a simple TypeKind
// ════════════════════════════════════════════════════════════════════════════

TypeKind make_str_type()
{
    TypeKind tk;
    tk.value = TypeKind_Str{};
    return tk;
}

TypeKind make_int_type()
{
    TypeKind tk;
    tk.value = TypeKind_Int{};
    return tk;
}

TypeKind make_bool_type()
{
    TypeKind tk;
    tk.value = TypeKind_Bool{};
    return tk;
}

TypeKind make_named_type(const std::string &name)
{
    TypeKind tk;
    tk.value = name;
    return tk;
}

TypeKind make_list_type(TypeKind inner)
{
    TypeKind tk;
    tk.value.emplace<4>(std::make_unique<TypeKind>(std::move(inner)));
    return tk;
}

TypeKind make_optional_type(TypeKind inner)
{
    TypeKind tk;
    tk.value.emplace<5>(std::make_unique<TypeKind>(std::move(inner)));
    return tk;
}

// ════════════════════════════════════════════════════════════════════════════
// Helper: build FieldDef
// ════════════════════════════════════════════════════════════════════════════

FieldDef make_field(const std::string &name, TypeKind type, bool recursive = false)
{
    FieldDef fd;
    fd.name = name;
    fd.type = std::make_unique<TypeKind>(std::move(type));
    fd.recursive = recursive;
    return fd;
}

// ════════════════════════════════════════════════════════════════════════════
// Layout tests
// ════════════════════════════════════════════════════════════════════════════

TEST(LayoutTest, ScalarStruct)
{
    IR ir;
    StructDef sd;
    sd.name = "User";
    sd.fields.push_back(make_field("name", make_str_type()));
    sd.fields.push_back(make_field("age", make_int_type()));
    sd.fields.push_back(make_field("active", make_bool_type()));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    const Layout *layout = table.find("User");

    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->typeName, "User");
    EXPECT_EQ(layout->totalSlots, 3);
    EXPECT_FALSE(layout->isEnum);

    ASSERT_EQ(layout->fieldRanges.size(), 3u);
    EXPECT_EQ(layout->fieldRanges[0].name, "name");
    EXPECT_EQ(layout->fieldRanges[0].offset, 0);
    EXPECT_EQ(layout->fieldRanges[0].size, 1);

    EXPECT_EQ(layout->fieldRanges[1].name, "age");
    EXPECT_EQ(layout->fieldRanges[1].offset, 1);

    EXPECT_EQ(layout->fieldRanges[2].name, "active");
    EXPECT_EQ(layout->fieldRanges[2].offset, 2);
}

TEST(LayoutTest, OptionalField)
{
    IR ir;
    StructDef sd;
    sd.name = "Config";
    sd.fields.push_back(make_field("label", make_str_type()));
    sd.fields.push_back(make_field("nickname", make_optional_type(make_str_type())));
    sd.fields.push_back(make_field("count", make_int_type()));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    const Layout *layout = table.find("Config");

    ASSERT_NE(layout, nullptr);
    // label=1 + nickname=(1 flag + 1 inner)=2 + count=1 = 4
    EXPECT_EQ(layout->totalSlots, 4);

    ASSERT_EQ(layout->fieldRanges.size(), 3u);
    EXPECT_EQ(layout->fieldRanges[1].name, "nickname");
    EXPECT_EQ(layout->fieldRanges[1].offset, 1);
    EXPECT_EQ(layout->fieldRanges[1].size, 2);
    EXPECT_TRUE(layout->fieldRanges[1].isOptional);

    EXPECT_EQ(layout->fieldRanges[2].name, "count");
    EXPECT_EQ(layout->fieldRanges[2].offset, 3);
}

TEST(LayoutTest, ListField)
{
    IR ir;
    StructDef sd;
    sd.name = "Group";
    sd.fields.push_back(make_field("name", make_str_type()));
    sd.fields.push_back(make_field("tags", make_list_type(make_str_type())));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    const Layout *layout = table.find("Group");

    ASSERT_NE(layout, nullptr);
    // name=1 + tags(list)=1 = 2
    EXPECT_EQ(layout->totalSlots, 2);

    EXPECT_TRUE(layout->fieldRanges[1].isList);
    EXPECT_EQ(layout->fieldRanges[1].size, 1);
}

TEST(LayoutTest, RecursiveFieldIsBoxed)
{
    IR ir;
    StructDef sd;
    sd.name = "TreeNode";
    sd.fields.push_back(make_field("value", make_str_type()));
    sd.fields.push_back(make_field("child", make_named_type("TreeNode"), /*recursive=*/true));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    const Layout *layout = table.find("TreeNode");

    ASSERT_NE(layout, nullptr);
    // value=1 + child(boxed)=1 = 2
    EXPECT_EQ(layout->totalSlots, 2);

    EXPECT_TRUE(layout->fieldRanges[1].isBox);
    EXPECT_EQ(layout->fieldRanges[1].size, 1);
}

TEST(LayoutTest, InlineNamedType)
{
    IR ir;

    StructDef address;
    address.name = "Address";
    address.fields.push_back(make_field("street", make_str_type()));
    address.fields.push_back(make_field("city", make_str_type()));
    ir.structs.push_back(std::move(address));

    StructDef person;
    person.name = "Person";
    person.fields.push_back(make_field("name", make_str_type()));
    person.fields.push_back(make_field("home", make_named_type("Address")));
    ir.structs.push_back(std::move(person));

    auto table = LayoutTable::build(ir);
    const Layout *personLayout = table.find("Person");

    ASSERT_NE(personLayout, nullptr);
    // name=1 + home(Address: street+city=2) = 3
    EXPECT_EQ(personLayout->totalSlots, 3);

    EXPECT_EQ(personLayout->fieldRanges[1].name, "home");
    EXPECT_EQ(personLayout->fieldRanges[1].offset, 1);
    EXPECT_EQ(personLayout->fieldRanges[1].size, 2);
}

TEST(LayoutTest, SimpleEnum)
{
    IR ir;
    EnumDef ed;
    ed.name = "Color";

    VariantDef v1;
    v1.tag = "Red";
    v1.recursive = false;
    ed.variants.push_back(std::move(v1));

    VariantDef v2;
    v2.tag = "Green";
    v2.recursive = false;
    ed.variants.push_back(std::move(v2));

    VariantDef v3;
    v3.tag = "Blue";
    v3.recursive = false;
    ed.variants.push_back(std::move(v3));

    ir.enums.push_back(std::move(ed));

    auto table = LayoutTable::build(ir);
    const Layout *layout = table.find("Color");

    ASSERT_NE(layout, nullptr);
    EXPECT_TRUE(layout->isEnum);
    // Tag-only enum: 1 slot for the tag, no payload
    EXPECT_EQ(layout->totalSlots, 1);
    EXPECT_EQ(layout->caseRanges.size(), 3u);
    EXPECT_EQ(layout->case_index("Red"), 0);
    EXPECT_EQ(layout->case_index("Green"), 1);
    EXPECT_EQ(layout->case_index("Blue"), 2);
}

TEST(LayoutTest, EnumWithPayload)
{
    IR ir;
    EnumDef ed;
    ed.name = "Shape";

    VariantDef circle;
    circle.tag = "Circle";
    circle.payload = std::make_unique<TypeKind>(make_int_type());
    circle.recursive = false;
    ed.variants.push_back(std::move(circle));

    VariantDef rect;
    rect.tag = "Rectangle";
    // No payload
    rect.recursive = false;
    ed.variants.push_back(std::move(rect));

    ir.enums.push_back(std::move(ed));

    auto table = LayoutTable::build(ir);
    const Layout *layout = table.find("Shape");

    ASSERT_NE(layout, nullptr);
    // tag + max(1, 0) = 2
    EXPECT_EQ(layout->totalSlots, 2);
    EXPECT_EQ(layout->caseRanges[0].tag, "Circle");
    EXPECT_TRUE(layout->caseRanges[0].hasPayload);
    EXPECT_EQ(layout->caseRanges[0].payloadSize, 1);
    EXPECT_EQ(layout->caseRanges[1].tag, "Rectangle");
    EXPECT_FALSE(layout->caseRanges[1].hasPayload);
}

TEST(LayoutTest, SlotSizeForTypes)
{
    IR ir;
    StructDef sd;
    sd.name = "Point";
    sd.fields.push_back(make_field("x", make_int_type()));
    sd.fields.push_back(make_field("y", make_int_type()));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);

    EXPECT_EQ(table.slot_size(make_str_type()), 1);
    EXPECT_EQ(table.slot_size(make_int_type()), 1);
    EXPECT_EQ(table.slot_size(make_bool_type()), 1);
    EXPECT_EQ(table.slot_size(make_named_type("Point")), 2);
    EXPECT_EQ(table.slot_size(make_list_type(make_str_type())), 1);
    EXPECT_EQ(table.slot_size(make_optional_type(make_int_type())), 2);
}

// ════════════════════════════════════════════════════════════════════════════
// DataLoader tests
// ════════════════════════════════════════════════════════════════════════════

TEST(DataLoaderTest, LoadScalarStruct)
{
    IR ir;
    StructDef sd;
    sd.name = "User";
    sd.fields.push_back(make_field("name", make_str_type()));
    sd.fields.push_back(make_field("age", make_int_type()));
    sd.fields.push_back(make_field("active", make_bool_type()));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    DataLoader loader(table);

    nlohmann::json input = {{"name", "Alice"}, {"age", 30}, {"active", true}};

    auto frame = loader.load("User", input);

    ASSERT_EQ(frame.size(), 3u);
    EXPECT_EQ(frame[0].kind, SlotKind::Str);
    EXPECT_EQ(frame[0].as_str(), "Alice");
    EXPECT_EQ(frame[1].kind, SlotKind::Int);
    EXPECT_EQ(frame[1].as_int(), 30);
    EXPECT_EQ(frame[2].kind, SlotKind::Bool);
    EXPECT_EQ(frame[2].as_bool(), true);
}

TEST(DataLoaderTest, LoadOptionalPresent)
{
    IR ir;
    StructDef sd;
    sd.name = "Config";
    sd.fields.push_back(make_field("label", make_str_type()));
    sd.fields.push_back(make_field("nickname", make_optional_type(make_str_type())));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    DataLoader loader(table);

    nlohmann::json input = {{"label", "main"}, {"nickname", "nick"}};
    auto frame = loader.load("Config", input);

    ASSERT_EQ(frame.size(), 3u);
    EXPECT_EQ(frame[0].kind, SlotKind::Str);
    EXPECT_EQ(frame[0].as_str(), "main");
    // Optional flag
    EXPECT_EQ(frame[1].kind, SlotKind::OptionalFlag);
    EXPECT_TRUE(frame[1].as_optional_flag());
    // Optional value
    EXPECT_EQ(frame[2].kind, SlotKind::Str);
    EXPECT_EQ(frame[2].as_str(), "nick");
}

TEST(DataLoaderTest, LoadOptionalAbsent)
{
    IR ir;
    StructDef sd;
    sd.name = "Config";
    sd.fields.push_back(make_field("label", make_str_type()));
    sd.fields.push_back(make_field("nickname", make_optional_type(make_str_type())));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    DataLoader loader(table);

    nlohmann::json input = {{"label", "main"}};
    auto frame = loader.load("Config", input);

    ASSERT_EQ(frame.size(), 3u);
    EXPECT_EQ(frame[1].kind, SlotKind::OptionalFlag);
    EXPECT_FALSE(frame[1].as_optional_flag());
}

TEST(DataLoaderTest, LoadList)
{
    IR ir;
    StructDef sd;
    sd.name = "Group";
    sd.fields.push_back(make_field("tags", make_list_type(make_str_type())));
    ir.structs.push_back(std::move(sd));

    auto table = LayoutTable::build(ir);
    DataLoader loader(table);

    nlohmann::json input = {{"tags", {"a", "b", "c"}}};
    auto frame = loader.load("Group", input);

    ASSERT_EQ(frame.size(), 1u);
    EXPECT_EQ(frame[0].kind, SlotKind::List);
    auto &list = frame[0].as_list();
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0].as_str(), "a");
    EXPECT_EQ(list[1].as_str(), "b");
    EXPECT_EQ(list[2].as_str(), "c");
}

TEST(DataLoaderTest, LoadEnum)
{
    IR ir;
    EnumDef ed;
    ed.name = "Color";

    VariantDef v1;
    v1.tag = "Red";
    v1.recursive = false;
    ed.variants.push_back(std::move(v1));

    VariantDef v2;
    v2.tag = "Green";
    v2.recursive = false;
    ed.variants.push_back(std::move(v2));

    ir.enums.push_back(std::move(ed));

    auto table = LayoutTable::build(ir);
    DataLoader loader(table);

    nlohmann::json input = "Green";
    auto frame = loader.load("Color", input);

    ASSERT_EQ(frame.size(), 1u);
    EXPECT_EQ(frame[0].kind, SlotKind::VariantTag);
    EXPECT_EQ(frame[0].as_variant_tag(), 1); // Green is index 1
}

TEST(DataLoaderTest, LoadEnumWithPayload)
{
    IR ir;
    EnumDef ed;
    ed.name = "Shape";

    VariantDef circle;
    circle.tag = "Circle";
    circle.payload = std::make_unique<TypeKind>(make_int_type());
    circle.recursive = false;
    ed.variants.push_back(std::move(circle));

    VariantDef rect;
    rect.tag = "Rectangle";
    rect.recursive = false;
    ed.variants.push_back(std::move(rect));

    ir.enums.push_back(std::move(ed));

    auto table = LayoutTable::build(ir);
    DataLoader loader(table);

    nlohmann::json input = {{"tag", "Circle"}, {"value", 42}};
    auto frame = loader.load("Shape", input);

    ASSERT_EQ(frame.size(), 2u);
    EXPECT_EQ(frame[0].kind, SlotKind::VariantTag);
    EXPECT_EQ(frame[0].as_variant_tag(), 0); // Circle is index 0
    EXPECT_EQ(frame[1].kind, SlotKind::Int);
    EXPECT_EQ(frame[1].as_int(), 42);
}

TEST(SlotTest, MakeAndAccessors)
{
    std::string s = "hello";
    auto str_slot = Slot::make_str(&s);
    EXPECT_EQ(str_slot.kind, SlotKind::Str);
    EXPECT_EQ(str_slot.as_str(), "hello");
    EXPECT_EQ(str_slot.to_string(), "hello");

    auto int_slot = Slot::make_int(42);
    EXPECT_EQ(int_slot.kind, SlotKind::Int);
    EXPECT_EQ(int_slot.as_int(), 42);
    EXPECT_EQ(int_slot.to_string(), "42");

    auto bool_slot = Slot::make_bool(true);
    EXPECT_EQ(bool_slot.kind, SlotKind::Bool);
    EXPECT_EQ(bool_slot.as_bool(), true);
    EXPECT_EQ(bool_slot.to_string(), "true");

    auto flag = Slot::make_optional_flag(false);
    EXPECT_EQ(flag.kind, SlotKind::OptionalFlag);
    EXPECT_FALSE(flag.as_optional_flag());

    auto tag = Slot::make_variant_tag(3);
    EXPECT_EQ(tag.kind, SlotKind::VariantTag);
    EXPECT_EQ(tag.as_variant_tag(), 3);
}

TEST(SlotTest, DefaultIsEmpty)
{
    Slot s;
    EXPECT_EQ(s.kind, SlotKind::Empty);
}

} // namespace
