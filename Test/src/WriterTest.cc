#include <gtest/gtest.h>

#include <tpp/Policy.h>
#include <tpp/Writer.h>

namespace
{

using namespace tpp;

static TppPolicy make_replace_policy(const std::string &tag,
                                     const std::string &find,
                                     const std::string &replace)
{
    TppPolicy policy;
    policy.tag = tag;
    policy.replacements.push_back({find, replace});
    return policy;
}

TEST(WriterTest, ConstructionEmpty)
{
    Writer writer;

    EXPECT_FALSE(writer.hasError());
    EXPECT_EQ(writer.takeOutput(), "");
}

TEST(WriterTest, EmitLiteral)
{
    Writer writer;

    EXPECT_TRUE(writer.emit("hello "));
    EXPECT_TRUE(writer.emit("world"));
    EXPECT_EQ(writer.takeOutput(), "hello world");
}

TEST(WriterTest, EmitValueWithPolicy)
{
    Writer writer;
    TppPolicy policy = make_replace_policy("underscores", " ", "_");

    EXPECT_TRUE(writer.emitValue("hello world", policy));
    EXPECT_EQ(writer.takeOutput(), "hello_world");
}

TEST(WriterTest, EmitValueWithActivePolicy)
{
    std::map<std::string, TppPolicy> policies;
    policies.emplace("upper_a", make_replace_policy("upper_a", "a", "A"));
    Writer writer(std::move(policies));

    EXPECT_TRUE(writer.pushPolicy("upper_a"));
    EXPECT_TRUE(writer.emitValue("banana", ""));
    EXPECT_TRUE(writer.popPolicy());
    EXPECT_EQ(writer.takeOutput(), "bAnAnA");
}

TEST(WriterTest, EmitValueWithNoneOverride)
{
    std::map<std::string, TppPolicy> policies;
    policies.emplace("replace_x", make_replace_policy("replace_x", "x", "X"));
    Writer writer(std::move(policies));

    EXPECT_TRUE(writer.pushPolicy("replace_x"));
    EXPECT_TRUE(writer.emitValue("fox", "none"));
    EXPECT_TRUE(writer.popPolicy());
    EXPECT_EQ(writer.takeOutput(), "fox");
}

TEST(WriterTest, PolicyScopeUnderflow)
{
    Writer writer;

    EXPECT_FALSE(writer.popPolicy());
    EXPECT_TRUE(writer.hasError());
}

TEST(WriterTest, CapturedBlockScopeUnderflow)
{
    Writer writer;

    EXPECT_FALSE(writer.emitCapturedBlock());
    EXPECT_TRUE(writer.hasError());
}

TEST(WriterTest, EmitCapturedBlockAtColumn)
{
    Writer writer;

    EXPECT_TRUE(writer.emitCapturedBlockAtColumn(4, [&]() {
        EXPECT_TRUE(writer.emit("  alpha\n  beta\n"));
    }));

    EXPECT_EQ(writer.takeOutput(), "    alpha\n    beta\n");
}

TEST(WriterTest, BeginAndEmitCapturedBlock)
{
    Writer writer;

    EXPECT_TRUE(writer.emit("header\n"));
    EXPECT_TRUE(writer.beginCapturedBlock(4));
    EXPECT_TRUE(writer.emit("  alpha\n  beta\n"));
    EXPECT_TRUE(writer.emitCapturedBlock());

    EXPECT_EQ(writer.takeOutput(), "header\n    alpha\n    beta\n");
}

TEST(WriterTest, BeginCapturedBlockFallsBackToRuntimeColumn)
{
    Writer writer;

    EXPECT_TRUE(writer.emit("prefix: "));
    EXPECT_TRUE(writer.beginCapturedBlock(4));
    EXPECT_TRUE(writer.emit("  alpha\n  beta\n"));
    EXPECT_TRUE(writer.emitCapturedBlock());

    EXPECT_EQ(writer.takeOutput(), "prefix: alpha\n        beta\n");
}

TEST(WriterTest, EmitForEach)
{
    Writer writer;
    std::vector<std::string> items = {"alpha", "beta", "gamma"};
    Writer::NativeLoopOptions options;
    options.sep = std::string_view{", "};
    options.followedBy = std::string_view{"."};

    int expectedIndex = 0;
    EXPECT_TRUE(writer.emitForEach(items, options, [&](const auto &item, int index) {
        EXPECT_EQ(index, expectedIndex++);
        EXPECT_TRUE(writer.emitValue(item));
    }));

    EXPECT_EQ(writer.takeOutput(), "alpha, beta, gamma.");
}

TEST(WriterTest, EmitCapturedBlockForEach)
{
    Writer writer;
    std::vector<std::string> items = {"alpha", "beta"};
    Writer::NativeLoopOptions options;
    options.sep = std::string_view{"\n"};
    options.followedBy = std::string_view{"\n"};

    EXPECT_TRUE(writer.emitCapturedBlockForEach(items, 4, options, [&](const auto &item, int) {
        EXPECT_TRUE(writer.emit("  "));
        EXPECT_TRUE(writer.emitValue(item));
    }));

    EXPECT_EQ(writer.takeOutput(), "    alpha\n    beta\n");
}

TEST(WriterTest, EmitAlignedForEach)
{
    struct Row
    {
        std::string lhs;
        std::string rhs;
    };

    Writer writer;
    std::vector<Row> rows = {{"x", "1"}, {"yy", "22"}};
    Writer::NativeLoopOptions options;
    options.sep = std::string_view{"\n"};

    EXPECT_TRUE(writer.emitAlignedForEach(rows,
                                          2,
                                          std::vector<char>{'l', 'r'},
                                          false,
                                          options,
                                          [&](const auto &row, int) {
                                              EXPECT_TRUE(writer.captureAlignedCell(0, [&]() {
                                                  EXPECT_TRUE(writer.emitValue(row.lhs));
                                              }));
                                              EXPECT_TRUE(writer.captureAlignedCell(1, [&]() {
                                                  EXPECT_TRUE(writer.emitValue(row.rhs));
                                              }));
                                          }));

    EXPECT_EQ(writer.takeOutput(), "x  1\nyy22");
}

TEST(WriterTest, MultiLineIndent)
{
    Writer writer;

    EXPECT_TRUE(writer.emit("    "));
    EXPECT_TRUE(writer.emitValue("alpha\nbeta\ngamma"));

    EXPECT_EQ(writer.takeOutput(), "    alpha\n    beta\n    gamma");
}

TEST(WriterTest, BeginEndCapture)
{
    Writer writer;

    EXPECT_TRUE(writer.emit("before|"));
    writer.beginCapture();
    EXPECT_TRUE(writer.emit("captured"));
    EXPECT_EQ(writer.endCapture(), "captured");
    EXPECT_TRUE(writer.emit("|after"));

    EXPECT_EQ(writer.takeOutput(), "before||after");
}

} // anonymous namespace