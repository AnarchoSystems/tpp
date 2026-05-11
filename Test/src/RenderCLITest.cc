#include "TestUtils.h"

// ── CompareRenderByCLI — pipeline: tpp | render-tpp ──────────────────────────

class AcceptanceTestOnlyPositives : public ::testing::TestWithParam<tTestCase>
{
};

TEST_P(AcceptanceTestOnlyPositives, CompareRenderByCLI)
{
    const auto testCase = GetParam().extract();

    if (!testCase.expectSuccess)
        return; // failure cases are covered by AcceptanceTest

    const auto projectPath = std::filesystem::absolute("TestCases/" + testCase.name).string();
    auto compileOutput = runCommandDirect({TPP_EXE, projectPath});

    ASSERT_TRUE(compileOutput.success)
        << "Test case: " << testCase.name
        << "\nDiagnostics: " << nlohmann::json(compileOutput.diagnostics).dump(2);

    std::vector<std::string> renderArgs = {RENDER_TPP_EXE, testCase.previewTemplateName, testCase.input.dump()};
    renderArgs.insert(renderArgs.end(), testCase.previewSignature.begin(), testCase.previewSignature.end());
    auto cliOutput = runCommandDirect(renderArgs, compileOutput.output);

    EXPECT_TRUE(cliOutput.success)
        << "Test case: " << testCase.name
        << "\nOutput: " << cliOutput.output
        << "\nDiagnostics: " << nlohmann::json(cliOutput.diagnostics).dump(2);
    if (!cliOutput.success) return;

    EXPECT_EQ(cliOutput.output, testCase.expectedOutput)
        << "Test case: " << testCase.name
        << "\nExpected: " << testCase.expectedOutput
        << "\nActual: "   << cliOutput.output;
}

INSTANTIATE_TEST_SUITE_P(AcceptanceTestOnlyPositives, AcceptanceTestOnlyPositives,
    ::testing::ValuesIn(GetPositiveTestCases()),
    [](const testing::TestParamInfo<AcceptanceTestOnlyPositives::ParamType> &info)
    { return info.param.name; });
