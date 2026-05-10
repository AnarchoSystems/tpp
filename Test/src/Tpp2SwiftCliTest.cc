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

TEST(Tpp2SwiftCliTest, SourceEmitsDocsAndOmitsRuntime)
{
    const auto projectPath = fixtureProjectPath();

    auto sourceOutput = runCommand(std::string(TPP_EXE) + " '" + projectPath + "' | " + TPP2SWIFT_EXE + " source 2>&1");
    ASSERT_TRUE(sourceOutput.success)
        << "Output: " << sourceOutput.output
        << "\nDiagnostics: " << nlohmann::json(sourceOutput.diagnostics).dump(2);

    EXPECT_NE(sourceOutput.output.find("/// A person with a name and age."), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("/// The person's full name."), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("var name: String"), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("/// The person's age in years."), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("var age: Int"), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("/// Render a greeting."), std::string::npos);
    EXPECT_NE(sourceOutput.output.find("func render_greet("), std::string::npos);

    EXPECT_EQ(sourceOutput.output.find("final class TppWriter"), std::string::npos);
    EXPECT_EQ(sourceOutput.output.find("struct TppPolicy"), std::string::npos);
    EXPECT_EQ(sourceOutput.output.find("final class Box"), std::string::npos);
}

TEST(Tpp2SwiftCliTest, SharedRuntimeContainsOnlyRuntimeSupport)
{
    auto runtimeOutput = runCommand(std::string(TPP2SWIFT_EXE) + " runtime-shared 2>&1");
    ASSERT_TRUE(runtimeOutput.success)
        << "Output: " << runtimeOutput.output
        << "\nDiagnostics: " << nlohmann::json(runtimeOutput.diagnostics).dump(2);

    EXPECT_NE(runtimeOutput.output.find("final class TppWriter"), std::string::npos);
    EXPECT_NE(runtimeOutput.output.find("struct TppPolicy"), std::string::npos);
    EXPECT_NE(runtimeOutput.output.find("final class Box"), std::string::npos);

    EXPECT_EQ(runtimeOutput.output.find("struct Person"), std::string::npos);
    EXPECT_EQ(runtimeOutput.output.find("func render_greet("), std::string::npos);
}

} // anonymous namespace