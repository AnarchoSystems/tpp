#include "TestUtils.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace
{

std::string fixtureProjectPath()
{
    return std::filesystem::absolute("Fixtures/tpp2cpp_doc_comments").string();
}

TEST(Tpp2JavaCliTest, SourceEmitsDocsAndOmitsRuntime)
{
    const auto projectPath = fixtureProjectPath();

    auto sourceOutput = runCommand(std::string(TPP_EXE) + " '" + projectPath + "' | " + TPP2JAVA_EXE + " source 2>&1");
    ASSERT_TRUE(sourceOutput.success)
        << "Output: " << sourceOutput.output
        << "\nDiagnostics: " << nlohmann::json(sourceOutput.diagnostics).dump(2);

    EXPECT_NE(sourceOutput.output.find("/**\n * A person with a name and age.\n */\nclass Person"), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("    /**\n     * The person's full name.\n     */\n    public String name;"), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("    /**\n     * The person's age in years.\n     */\n    public int age;"), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("/**\n * Render a greeting.\n * Keeps this test on the function-header path.\n */"), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("static String render_greet("), std::string::npos);

    EXPECT_EQ(sourceOutput.output.find("class TppRuntime"), std::string::npos);
    EXPECT_EQ(sourceOutput.output.find("static class TppWriter"), std::string::npos);
    EXPECT_EQ(sourceOutput.output.find("class TppPolicy"), std::string::npos);
}

TEST(Tpp2JavaCliTest, SharedRuntimeContainsOnlyRuntimeSupport)
{
    auto runtimeOutput = runCommand(std::string(TPP2JAVA_EXE) + " runtime-shared 2>&1");
    ASSERT_TRUE(runtimeOutput.success)
        << "Output: " << runtimeOutput.output
        << "\nDiagnostics: " << nlohmann::json(runtimeOutput.diagnostics).dump(2);

    EXPECT_NE(runtimeOutput.output.find("class TppRuntime"), std::string::npos);
    EXPECT_NE(runtimeOutput.output.find("static class TppWriter"), std::string::npos);
    EXPECT_NE(runtimeOutput.output.find("class TppPolicy"), std::string::npos);

    EXPECT_EQ(runtimeOutput.output.find("class Person"), std::string::npos);
    EXPECT_EQ(runtimeOutput.output.find("class Functions"), std::string::npos);
}

} // anonymous namespace