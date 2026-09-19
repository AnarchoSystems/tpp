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

std::string makeJavaTestProjectPath()
{
    return std::filesystem::absolute("MakeJavaTest").string();
}

TEST(Tpp2CppCliTest, EmitsDocCommentsInTypesAndFunctions)
{
    const auto projectPath = fixtureProjectPath();

    const auto compileOutput = runCommandDirect({TPP_EXE, projectPath});
    ASSERT_TRUE(compileOutput.success)
        << "Output: " << compileOutput.output
        << "\nDiagnostics: " << nlohmann::json(compileOutput.diagnostics).dump(2);

    auto typesOutput = runCommandDirect({TPP2CPP_EXE, "types"}, compileOutput.output);
    ASSERT_TRUE(typesOutput.success)
        << "Output: " << typesOutput.output
        << "\nDiagnostics: " << nlohmann::json(typesOutput.diagnostics).dump(2);
    EXPECT_EQ(typesOutput.output.find("__TPP_DOC__"), std::string::npos);
    EXPECT_NE(typesOutput.output.find("/**\nA person with a name and age.\n */\nstruct Person"), std::string::npos);
    EXPECT_NE(typesOutput.output.find("    /**\n    The person's full name.\n     */\n    std::string name;"), std::string::npos);
    EXPECT_NE(typesOutput.output.find("    /**\n    The person's age in years.\n     */\n    int age;"), std::string::npos);

    auto functionsOutput = runCommandDirect({TPP2CPP_EXE, "functions"}, compileOutput.output);
    ASSERT_TRUE(functionsOutput.success)
        << "Output: " << functionsOutput.output
        << "\nDiagnostics: " << nlohmann::json(functionsOutput.diagnostics).dump(2);
    EXPECT_EQ(functionsOutput.output.find("__TPP_DOC__"), std::string::npos);
    EXPECT_NE(functionsOutput.output.find("/**\nRender a greeting.\nKeeps this test on the function-header path.\n */\nstd::string greet("), std::string::npos);
}

TEST(Tpp2CppCliTest, FunctionsHandlesMakeJavaTestFixture)
{
    const auto projectPath = makeJavaTestProjectPath();

    const auto compileOutput = runCommandDirect({TPP_EXE, projectPath});
    ASSERT_TRUE(compileOutput.success)
        << "Output: " << compileOutput.output
        << "\nDiagnostics: " << nlohmann::json(compileOutput.diagnostics).dump(2);

    auto functionsOutput = runCommandDirect({TPP2CPP_EXE, "functions"}, compileOutput.output);
    ASSERT_TRUE(functionsOutput.success)
        << "Output: " << functionsOutput.output
        << "\nDiagnostics: " << nlohmann::json(functionsOutput.diagnostics).dump(2);
    EXPECT_NE(functionsOutput.output.find("std::string render_test("), std::string::npos);
    EXPECT_NE(functionsOutput.output.find("std::string render_fragment("), std::string::npos);
}

TEST(Tpp2CppCliTest, StandaloneImplKeepsRuntimeGlobalAndGeneratedHelpersAnonymous)
{
    const auto projectPath = makeJavaTestProjectPath();

    const auto compileOutput = runCommandDirect({TPP_EXE, projectPath});
    ASSERT_TRUE(compileOutput.success)
        << "Output: " << compileOutput.output
        << "\nDiagnostics: " << nlohmann::json(compileOutput.diagnostics).dump(2);

    auto implOutput = runCommandDirect({TPP2CPP_EXE, "impl", "--standalone", "-ns", "fixture"}, compileOutput.output);
    ASSERT_TRUE(implOutput.success)
        << "Output: " << implOutput.output
        << "\nDiagnostics: " << nlohmann::json(implOutput.diagnostics).dump(2);

    const auto runtimeNamespace = implOutput.output.find("namespace tpp {");
    const auto generatedNamespace = implOutput.output.find("namespace fixture {");
    const auto generatedHelperNamespace = implOutput.output.find("namespace {", generatedNamespace);
    ASSERT_NE(runtimeNamespace, std::string::npos);
    ASSERT_NE(generatedNamespace, std::string::npos);
    ASSERT_NE(generatedHelperNamespace, std::string::npos);
    EXPECT_LT(runtimeNamespace, generatedNamespace);
    EXPECT_LT(generatedNamespace, generatedHelperNamespace);
    EXPECT_EQ(implOutput.output.find("namespace {\nnamespace tpp {"), std::string::npos);
    EXPECT_NE(implOutput.output.find("struct _tpp_case_has_value_member"), std::string::npos);
}

} // anonymous namespace