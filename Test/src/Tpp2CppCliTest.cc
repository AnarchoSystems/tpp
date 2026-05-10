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

TEST(Tpp2CppCliTest, EmitsDocCommentsInTypesAndFunctions)
{
    const auto projectPath = fixtureProjectPath();

    auto typesOutput = runCommand(std::string(TPP_EXE) + " '" + projectPath + "' | " + TPP2CPP_EXE + " types 2>&1");
    ASSERT_TRUE(typesOutput.success)
        << "Output: " << typesOutput.output
        << "\nDiagnostics: " << nlohmann::json(typesOutput.diagnostics).dump(2);
    EXPECT_EQ(typesOutput.output.find("__TPP_DOC__"), std::string::npos);
    EXPECT_NE(typesOutput.output.find("/**\nA person with a name and age.\n */\nstruct Person"), std::string::npos);
    EXPECT_NE(typesOutput.output.find("    /**\n    The person's full name.\n     */\n    std::string name;"), std::string::npos);
    EXPECT_NE(typesOutput.output.find("    /**\n    The person's age in years.\n     */\n    int age;"), std::string::npos);

    auto functionsOutput = runCommand(std::string(TPP_EXE) + " '" + projectPath + "' | " + TPP2CPP_EXE + " functions 2>&1");
    ASSERT_TRUE(functionsOutput.success)
        << "Output: " << functionsOutput.output
        << "\nDiagnostics: " << nlohmann::json(functionsOutput.diagnostics).dump(2);
    EXPECT_EQ(functionsOutput.output.find("__TPP_DOC__"), std::string::npos);
    EXPECT_NE(functionsOutput.output.find("/**\nRender a greeting.\nKeeps this test on the function-header path.\n */\nstd::string greet("), std::string::npos);
}

} // anonymous namespace