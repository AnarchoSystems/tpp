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
    std::string typedefURL;
    std::string typedefs;
    std::string templateURL;
    std::string templateString;
    nlohmann::json input;
    bool expectSuccess = true;
    std::string expectedOutput;
    std::vector<tpp::DiagnosticLSPMessage> expectedDiagnostics;
    std::string getFunctionError;
    std::string bindFunctionError;
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

struct tErrors
{
    std::string getFunctionError;
    std::string bindFunctionError;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(tErrors, getFunctionError, bindFunctionError)
};

std::vector<tTestCase> GetTestCases()
{
    std::vector<tTestCase> testCases;

    bool failed = false;

    for (const auto &entry : std::filesystem::directory_iterator("TestCases"))
    {
        auto &testCase = testCases.emplace_back();
        testCase.name = entry.path().filename().string();
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
            isSuccess = false;
            expectedOutputFile.close();
            expectedOutputFile.open(entry.path() / "expected_diagnostics.json");
            if (expectedOutputFile.is_open())
            {
                auto diagnosticsJson = nlohmann::json::parse(std::string((std::istreambuf_iterator<char>(expectedOutputFile)), std::istreambuf_iterator<char>()));
                testCase.expectedDiagnostics = diagnosticsJson.get<std::vector<tpp::DiagnosticLSPMessage>>();
                expectedOutputFile.close();
            }
            std::ifstream expectedErrorsFile(entry.path() / "expected_errors.json");
            if (expectedErrorsFile.is_open())
            {
                auto expectedErrorsJson = nlohmann::json::parse(std::string((std::istreambuf_iterator<char>(expectedErrorsFile)), std::istreambuf_iterator<char>()));
                tErrors expectedErrors = expectedErrorsJson.get<tErrors>();
                expectedErrorsFile.close();
                testCase.getFunctionError = expectedErrors.getFunctionError;
                testCase.bindFunctionError = expectedErrors.bindFunctionError;
            }
        }

        if (!typedefsFile.is_open() || !templateFile.is_open() || !inputFile.is_open())
        {
            std::cerr << "Failed to open test case files in " << entry.path() << std::endl;
            std::cerr << "Expected files: typedefs.tpp, template.tpp, input.json, and either expected_output.txt or expected_diagnostics.json/expected_errors.json" << std::endl;
            failed = true;
            continue;
        }

        testCase.expectSuccess = isSuccess;
        auto input = std::string((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
        testCase.input = nlohmann::json::parse(input);
        testCase.typedefs = std::string((std::istreambuf_iterator<char>(typedefsFile)), std::istreambuf_iterator<char>());
        testCase.templateString = std::string((std::istreambuf_iterator<char>(templateFile)), std::istreambuf_iterator<char>());
        if (isSuccess)
        {
            testCase.expectedOutput = std::string((std::istreambuf_iterator<char>(expectedOutputFile)), std::istreambuf_iterator<char>());
        }
        testCase.typedefURL = testCase.name + "/typedefs.tpp";
        testCase.templateURL = testCase.name + "/template.tpp";
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

    tpp::DiagnosticLSPMessage TypesDiagnostics(testCase.typedefURL);
    tpp::DiagnosticLSPMessage CompilerDiagnostics(testCase.templateURL);
    std::string getFunctionError;
    std::string bindFunctionError;

    tpp::CompilerOutput output;
    tpp::FunctionSymbol functionSymbol;
    tpp::Program program;

    const bool isSuccess = compiler.add_types(testCase.typedefs, TypesDiagnostics.diagnostics) && compiler.compile(testCase.templateString, output, CompilerDiagnostics.diagnostics) && output.get_function("main", functionSymbol, getFunctionError) && functionSymbol.bind(testCase.input, program, bindFunctionError);

    EXPECT_EQ(isSuccess, testCase.expectSuccess) << "Test case: " << testCase.name;
    if (isSuccess)
    {
        std::string generatedCode = program.run();
        EXPECT_EQ(generatedCode, testCase.expectedOutput) << "Test case: " << testCase.name;
    }
    else
    {
        std::vector<tpp::DiagnosticLSPMessage> actualDiagnostics;
        if (!TypesDiagnostics.diagnostics.empty())
        {
            actualDiagnostics.push_back(TypesDiagnostics);
        }
        if (!CompilerDiagnostics.diagnostics.empty())
        {
            actualDiagnostics.push_back(CompilerDiagnostics);
        }
        EXPECT_EQ(actualDiagnostics, testCase.expectedDiagnostics) << "Test case: " << testCase.name << "\nActual diagnostics: " << nlohmann::json(actualDiagnostics).dump(2);
        EXPECT_EQ(getFunctionError, testCase.getFunctionError) << "Test case: " << testCase.name;
        EXPECT_EQ(bindFunctionError, testCase.bindFunctionError) << "Test case: " << testCase.name;
    }
}

INSTANTIATE_TEST_SUITE_P(AcceptanceTests, AcceptanceTest, ::testing::ValuesIn(GetTestCases()), [](const testing::TestParamInfo<AcceptanceTest::ParamType> &info)
                         { return info.param.name; });