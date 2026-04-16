#include "TestUtils.h"
#include <tpp/Rendering.h>
#include <nlohmann/json.hpp>

// ── AcceptanceTest — in-process compile + render ──────────────────────────────

class AcceptanceTest : public ::testing::TestWithParam<tTestCase>
{
protected:
    tLoadedTestCase testCase;
    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> fileDiags;
    void SetUp() override
    {
        testCase = GetParam().extract();
        fileDiags.reserve(testCase.sources.size());
        for (const auto &src : testCase.sources)
            fileDiags.push_back(tpp::DiagnosticLSPMessage(src.url));
        for (size_t i = 0; i < testCase.sources.size(); ++i)
        {
            const auto &src = testCase.sources[i];
            if (src.isTypes)
                compiler.add_types(src.content, fileDiags[i].diagnostics);
            else
                compiler.add_templates(src.content, fileDiags[i].diagnostics);
        }
        for (const auto &pol : testCase.policies)
        {
            std::vector<tpp::Diagnostic> policyDiagnostics;
            compiler.add_policy_text(pol, policyDiagnostics);
            EXPECT_TRUE(policyDiagnostics.empty())
                << "Unexpected policy diagnostics: " << nlohmann::json(policyDiagnostics).dump(2);
        }
    }
};

TEST_P(AcceptanceTest, RunTestCase)
{
    tpp::IR output;
    std::string getFunctionError, renderError, renderedOutput;
    const tpp::FunctionDef *function = nullptr;

    const bool isSuccess = compiler.compile(output) &&
                           tpp::get_function(output, "main", function, getFunctionError) &&
                           tpp::render_function(output, *function, testCase.input, renderedOutput, renderError);

    EXPECT_EQ(isSuccess, testCase.expectSuccess) << "Test case: " << testCase.name;
    if (isSuccess)
    {
        EXPECT_EQ(renderedOutput, testCase.expectedOutput) << "Test case: " << testCase.name;
    }
    else
    {
        std::vector<tpp::DiagnosticLSPMessage> actualDiags;
        for (const auto &d : fileDiags)
            if (!d.diagnostics.empty())
                actualDiags.push_back(d);
        EXPECT_EQ(actualDiags, testCase.expectedDiagnostics)
            << "Test case: " << testCase.name
            << "\nActual diagnostics: " << nlohmann::json(actualDiags).dump(2);
        EXPECT_EQ(getFunctionError, testCase.getFunctionError) << "Test case: " << testCase.name;
        EXPECT_EQ(renderError, testCase.renderError) << "Test case: " << testCase.name;
    }
}

// ── CompareCompileByCLI — compare tpp CLI JSON output with in-process ─────────

TEST_P(AcceptanceTest, CompareCompileByCLI)
{
    // Run tpp directly (no shell) to avoid pipe inheritance issues on macOS.
    auto cliOutput = runCommandDirect({TPP_EXE,
        std::filesystem::absolute("TestCases/" + testCase.name).string()});

    tpp::IR expectedOutput;
    bool expectedSuccess = compiler.compile(expectedOutput);

    EXPECT_EQ(cliOutput.success, expectedSuccess) << "Test case: " << testCase.name;
    if (cliOutput.success != expectedSuccess)
        return;

    if (expectedSuccess)
    {
        tpp::IR actualOutput = nlohmann::json::parse(cliOutput.output);
        EXPECT_EQ(nlohmann::json(actualOutput), nlohmann::json(expectedOutput))
            << "Test case: " << testCase.name
            << "\nExpected: " << nlohmann::json(expectedOutput).dump(2)
            << "\nActual: " << nlohmann::json(actualOutput).dump(2);
    }
    else
    {
        std::vector<std::string> expectedDiagStrings;
        for (const auto &fd : testCase.expectedDiagnostics)
        {
            auto copy = fd;
            copy.uri = std::filesystem::absolute("TestCases/" + copy.uri).string();
            for (const auto &d : copy.toGCCDiagnostics())
                expectedDiagStrings.push_back(d);
        }
        EXPECT_EQ(cliOutput.diagnostics, expectedDiagStrings)
            << "Test case: " << testCase.name
            << "\nActual: " << nlohmann::json(cliOutput.diagnostics).dump(2);
    }
}

INSTANTIATE_TEST_SUITE_P(AcceptanceTests, AcceptanceTest,
                         ::testing::ValuesIn(GetTestCases()),
                         [](const testing::TestParamInfo<AcceptanceTest::ParamType> &info)
                         { return info.param.name; });
