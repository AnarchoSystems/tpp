#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <tpp/Compiler.h>

namespace tpp
{
    void PrintTo(const Diagnostic &d, std::ostream *os)
    {
        nlohmann::json j = d;
        *os << j.dump(2);
    }
}

struct tTestCase
{
    std::string name;
    std::string typedefs;
    std::string templateString;
    nlohmann::json input;
    bool expectSuccess = true;
    std::string expectedOutput;
    std::vector<tpp::Diagnostic> expectedDiagnostics;
};

void PrintTo(const tTestCase &testCase, std::ostream *os)
{
    for (char c : testCase.name)
    {
        if (std::isalnum(c))
        {
            *os << c;
        }
        else
        {
            *os << ' ';
        }
    }
}

std::vector<tTestCase> GetTestCases()
{
    std::vector<tTestCase> testCases;

    bool failed = false;

    for (const auto &entry : std::filesystem::directory_iterator("TestCases"))
    {
        if (!entry.is_directory())
        {
            continue;
        }
        std::ifstream typedefsFile(entry.path() / "typedefs.tpp");
        std::ifstream templateFile(entry.path() / "template.tpp");
        std::ifstream inputFile(entry.path() / "input.json");

        bool isSuccess = true;
        // if output file exists, success; otherwise, diagnostic file must exist and test must fail
        std::ifstream expectedOutputFile(entry.path() / "expected_output.txt");
        if (!expectedOutputFile.is_open())
        {
            expectedOutputFile.close();
            expectedOutputFile.open(entry.path() / "expected_diagnostics.json");
            isSuccess = false;
        }

        if (!typedefsFile.is_open() || !templateFile.is_open() || !inputFile.is_open() || !expectedOutputFile.is_open())
        {
            std::cerr << "Failed to open test case files in " << entry.path() << std::endl;
            std::cerr << "Expected files: typedefs.tpp, template.tpp, input.json, and either expected_output.txt or expected_diagnostics.json" << std::endl;
            failed = true;
            continue;
        }

        auto &testCase = testCases.emplace_back();

        testCase.name = entry.path().filename().string();
        testCase.expectSuccess = isSuccess;
        auto input = std::string((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
        testCase.input = nlohmann::json::parse(input);
        testCase.typedefs = std::string((std::istreambuf_iterator<char>(typedefsFile)), std::istreambuf_iterator<char>());
        testCase.templateString = std::string((std::istreambuf_iterator<char>(templateFile)), std::istreambuf_iterator<char>());
        if (isSuccess)
        {
            testCase.expectedOutput = std::string((std::istreambuf_iterator<char>(expectedOutputFile)), std::istreambuf_iterator<char>());
        }
        else
        {
            auto diagnosticsJson = nlohmann::json::parse(std::string((std::istreambuf_iterator<char>(expectedOutputFile)), std::istreambuf_iterator<char>()));
            testCase.expectedDiagnostics = diagnosticsJson.get<std::vector<tpp::Diagnostic>>();
        }
    }

    if (failed)
    {
        throw std::runtime_error("Failed to load test cases");
    }

    return testCases;
}

class AcceptanceTest : public ::testing::TestWithParam<tTestCase>
{
};

TEST_P(AcceptanceTest, RunTestCase)
{
    tTestCase testCase = GetParam();

    tpp::Compiler compiler;

    std::vector<tpp::Diagnostic> diagnostics;

    tpp::CompilerOutput output;
    tpp::FunctionSymbol functionSymbol;
    tpp::Program program;

    const bool isSuccess = compiler.add_types(testCase.typedefs, diagnostics) && compiler.compile(testCase.templateString, output, diagnostics) && output.get_function("main", functionSymbol, diagnostics) && functionSymbol.bind(testCase.input, program, diagnostics);

    EXPECT_EQ(isSuccess, testCase.expectSuccess) << "Test case: " << testCase.name;
    if (isSuccess)
    {
        std::string generatedCode = program.run();
        EXPECT_EQ(generatedCode, testCase.expectedOutput) << "Test case: " << testCase.name;
    }
    else
    {
        EXPECT_EQ(diagnostics, testCase.expectedDiagnostics) << "Test case: " << testCase.name;
    }
}

INSTANTIATE_TEST_SUITE_P(AcceptanceTests, AcceptanceTest, ::testing::ValuesIn(GetTestCases()), [](const testing::TestParamInfo<AcceptanceTest::ParamType> &info)
                         { return info.param.name; });