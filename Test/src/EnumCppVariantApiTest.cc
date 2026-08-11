#include <gtest/gtest.h>

#include "enum_cpp_variant_api_types.h"

TEST(EnumCppVariantApiTest, SupportsNestedTagTypesAssignmentAndEmplace)
{
    using namespace enum_cpp_variant_api;

    Item item = Item::A{42};
    ASSERT_TRUE(std::holds_alternative<Item::A>(item.value));
    EXPECT_EQ(std::get<Item::A>(item.value).value, 42);

    item = Item::B{};
    ASSERT_TRUE(std::holds_alternative<Item::B>(item.value));

    auto& payloadTag = item.emplace<Item::A>(7);
    EXPECT_EQ(payloadTag.value, 7);

    nlohmann::json j = item;
    EXPECT_TRUE(j.contains("A"));
    EXPECT_EQ(j.at("A").get<int>(), 7);
}
