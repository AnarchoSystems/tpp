#include "TestUtils.h"
#include <tpp/Runtime.h>
#include <nlohmann/json.hpp>

// ── AcceptanceTest — in-process compile + render ──────────────────────────────

class AcceptanceTest : public ::testing::TestWithParam<tTestCase>
{
protected:
    tLoadedTestCase testCase;
    tpp::TppProject project;
    tpp::IR output;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    bool compileSuccess = false;

    void SetUp() override
    {
        testCase = GetParam().extract();
        for (const auto &src : testCase.sources)
        {
            if (src.isTypes)
                project.add_type_source(src.content, src.url);
            else
                project.add_template_source(src.content, src.url);
        }
        for (size_t index = 0; index < testCase.policies.size(); ++index)
            project.add_policy_source(testCase.policies[index], testCase.name + "/policy_" + std::to_string(index) + ".json");

        tpp::LexedProject lexed;
        tpp::ParsedProject parsed;
        diagnostics.clear();
        output = {};
        compileSuccess = tpp::lex(project, lexed, diagnostics) &&
                 tpp::parse(lexed, parsed, diagnostics) &&
                 tpp::compile(parsed, output, diagnostics);
    }
};

TEST_P(AcceptanceTest, RunTestCase)
{
    std::string getFunctionError, renderError, renderedOutput;
    const tpp::FunctionDef *function = nullptr;

    bool lookupSuccess = false;
    if (compileSuccess)
    {
        if (testCase.previewSignature.empty())
        {
            lookupSuccess = tpp::get_function(output, testCase.previewTemplateName,
                                              function, getFunctionError);
        }
        else
        {
            lookupSuccess = tpp::get_function(output, testCase.previewTemplateName,
                                              testCase.previewSignature,
                                              function, getFunctionError);
        }
    }

    bool renderSuccess = false;
    if (compileSuccess && lookupSuccess)
    {
        renderSuccess = tpp::render_function(output, *function, testCase.input,
                                             renderedOutput, renderError);
    }

    const bool isSuccess = compileSuccess && lookupSuccess && renderSuccess;

    EXPECT_EQ(isSuccess, testCase.expectSuccess) << "Test case: " << testCase.name;
    if (isSuccess)
    {
        EXPECT_EQ(renderedOutput, testCase.expectedOutput) << "Test case: " << testCase.name;
    }
    else
    {
        std::vector<tpp::DiagnosticLSPMessage> actualDiags;
        for (const auto &d : diagnostics)
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

    bool expectedSuccess = compileSuccess;

    EXPECT_EQ(cliOutput.success, expectedSuccess) << "Test case: " << testCase.name;
    if (cliOutput.success != expectedSuccess)
        return;

    if (expectedSuccess)
    {
        tpp::IR actualOutput = nlohmann::json::parse(cliOutput.output);
        EXPECT_EQ(nlohmann::json(actualOutput), nlohmann::json(output))
            << "Test case: " << testCase.name
            << "\nExpected: " << nlohmann::json(output).dump(2)
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
