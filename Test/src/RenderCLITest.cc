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

    auto sCommand = TPP_EXE " " +
        std::filesystem::absolute("TestCases/" + testCase.name).string() +
        " | " RENDER_TPP_EXE " main '" + testCase.input.dump() + "' 2>&1";
    auto cliOutput = runCommand(sCommand);

    EXPECT_TRUE(cliOutput.success)
        << "Test case: " << testCase.name
        << "\nCommand: " << sCommand
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
