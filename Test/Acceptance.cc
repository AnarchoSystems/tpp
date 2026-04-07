#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
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

struct SourceFile
{
    std::string url;
    std::string content;
    bool isTypes = false;
};

struct tTestCase
{
    std::string name;
    std::vector<SourceFile> sources;
    nlohmann::json input;
    bool expectSuccess = true;
    std::string expectedOutput;
    std::vector<tpp::DiagnosticLSPMessage> expectedDiagnostics;
    std::string getFunctionError;
    std::string renderError;
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
    std::string renderError;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(tErrors, getFunctionError, renderError)
};

static std::string readFile(const std::filesystem::path &path)
{
    std::ifstream f(path);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Returns true if text matches the glob pattern (supports '*' wildcard only).
static bool matchGlob(const std::string &pattern, const std::string &text)
{
    size_t pi = 0, ti = 0, starPos = std::string::npos, matchPos = 0;
    while (ti < text.size())
    {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti]))
        {
            ++pi; ++ti;
        }
        else if (pi < pattern.size() && pattern[pi] == '*')
        {
            starPos = pi++;
            matchPos = ti;
        }
        else if (starPos != std::string::npos)
        {
            pi = starPos + 1;
            ti = ++matchPos;
        }
        else
        {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*')
        ++pi;
    return pi == pattern.size();
}

// Expands a single config pattern to a sorted list of .tpp file paths.
static std::vector<std::filesystem::path> expandPattern(const std::filesystem::path &baseDir, const std::string &pattern)
{
    std::filesystem::path patPath(pattern);
    std::string filename = patPath.filename().string();
    std::filesystem::path parentDir = baseDir / patPath.parent_path();

    if (filename.find('*') == std::string::npos)
    {
        return {baseDir / pattern};
    }

    std::vector<std::filesystem::path> results;
    if (std::filesystem::is_directory(parentDir))
    {
        for (const auto &entry : std::filesystem::directory_iterator(parentDir))
        {
            if (!entry.is_regular_file())
                continue;
            auto name = entry.path().filename().string();
            if (name.size() < 4 || name.substr(name.size() - 4) != ".tpp")
                continue;
            if (matchGlob(filename, name))
                results.push_back(entry.path());
        }
        std::sort(results.begin(), results.end());
    }
    return results;
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

        bool bHasContent = false;

        for (const auto &file : std::filesystem::directory_iterator(entry.path()))
        {
            if (file.is_regular_file())
            {
                bHasContent = true;
                break;
            }
        }

        if (!bHasContent)
        {
            std::cerr << "Test case directory " << entry.path() << " is empty, skipping." << std::endl;
            continue;
        }

        auto &testCase = testCases.emplace_back();
        testCase.name = entry.path().filename().string();

        // Read tpp-config.json to determine which files are types vs templates
        std::filesystem::path configPath = entry.path() / "tpp-config.json";
        std::ifstream configFile(configPath);
        if (!configFile.is_open())
        {
            std::cerr << "No tpp-config.json found in " << entry.path() << std::endl;
            failed = true;
            continue;
        }
        nlohmann::json config;
        try
        {
            config = nlohmann::json::parse(std::string(
                (std::istreambuf_iterator<char>(configFile)),
                std::istreambuf_iterator<char>()));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid tpp-config.json in " << entry.path() << ": " << e.what() << std::endl;
            failed = true;
            continue;
        }

        for (const auto &pattern : config.value("types", nlohmann::json::array()))
        {
            for (const auto &filePath : expandPattern(entry.path(), pattern.get<std::string>()))
            {
                std::string relPath = std::filesystem::relative(filePath, entry.path()).string();
                std::string url = testCase.name + "/" + relPath;
                testCase.sources.push_back({url, readFile(filePath), true});
            }
        }
        for (const auto &pattern : config.value("templates", nlohmann::json::array()))
        {
            for (const auto &filePath : expandPattern(entry.path(), pattern.get<std::string>()))
            {
                std::string relPath = std::filesystem::relative(filePath, entry.path()).string();
                std::string url = testCase.name + "/" + relPath;
                testCase.sources.push_back({url, readFile(filePath), false});
            }
        }

        // Read input.json
        std::ifstream inputFile(entry.path() / "input.json");
        if (!inputFile.is_open())
        {
            std::cerr << "Failed to open input.json in " << entry.path() << std::endl;
            failed = true;
            continue;
        }
        testCase.input = nlohmann::json::parse(
            std::string((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>()));

        // Determine success/failure expectation
        std::ifstream expectedOutputFile(entry.path() / "expected_output.txt");
        if (expectedOutputFile.is_open())
        {
            testCase.expectSuccess = true;
            testCase.expectedOutput = std::string(
                (std::istreambuf_iterator<char>(expectedOutputFile)), std::istreambuf_iterator<char>());
        }
        else
        {
            testCase.expectSuccess = false;
            std::ifstream expectedDiagnosticsFile(entry.path() / "expected_diagnostics.json");
            if (expectedDiagnosticsFile.is_open())
            {
                auto j = nlohmann::json::parse(std::string(
                    (std::istreambuf_iterator<char>(expectedDiagnosticsFile)),
                    std::istreambuf_iterator<char>()));
                testCase.expectedDiagnostics = j.get<std::vector<tpp::DiagnosticLSPMessage>>();
            }
            std::ifstream expectedErrorsFile(entry.path() / "expected_errors.json");
            if (expectedErrorsFile.is_open())
            {
                auto j = nlohmann::json::parse(std::string(
                    (std::istreambuf_iterator<char>(expectedErrorsFile)),
                    std::istreambuf_iterator<char>()));
                tErrors errs = j.get<tErrors>();
                testCase.getFunctionError = errs.getFunctionError;
                testCase.renderError = errs.renderError;
            }
        }

        if (testCase.sources.empty())
        {
            std::cerr << "No .tpp source files found in " << entry.path() << std::endl;
            failed = true;
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
    const tTestCase &testCase = GetParam();

    tpp::Compiler compiler;

    // Per-source-file diagnostics, indexed in the same order as testCase.sources
    std::vector<tpp::DiagnosticLSPMessage> fileDiags;
    fileDiags.reserve(testCase.sources.size());
    for (const auto &src : testCase.sources)
    {
        fileDiags.push_back(tpp::DiagnosticLSPMessage(src.url));
    }

    // Feed each source to the compiler
    for (size_t i = 0; i < testCase.sources.size(); ++i)
    {
        const auto &src = testCase.sources[i];
        if (src.isTypes)
            compiler.add_types(src.content, fileDiags[i].diagnostics);
        else
            compiler.add_templates(src.content, fileDiags[i].diagnostics);
    }

    tpp::CompilerOutput output;
    std::string getFunctionError;
    std::string renderError;
    std::string renderedOutput;
    tpp::FunctionSymbol functionSymbol;

    const bool isSuccess = compiler.compile(output) &&
                           output.get_function("main", functionSymbol, getFunctionError) &&
                           functionSymbol.render(testCase.input, renderedOutput, renderError);

    EXPECT_EQ(isSuccess, testCase.expectSuccess) << "Test case: " << testCase.name;
    if (isSuccess)
    {
        EXPECT_EQ(renderedOutput, testCase.expectedOutput) << "Test case: " << testCase.name;
    }
    else
    {
        std::vector<tpp::DiagnosticLSPMessage> actualDiagnostics;
        for (const auto &d : fileDiags)
        {
            if (!d.diagnostics.empty())
                actualDiagnostics.push_back(d);
        }
        EXPECT_EQ(actualDiagnostics, testCase.expectedDiagnostics)
            << "Test case: " << testCase.name
            << "\nActual diagnostics: " << nlohmann::json(actualDiagnostics).dump(2);
        EXPECT_EQ(getFunctionError, testCase.getFunctionError) << "Test case: " << testCase.name;
        EXPECT_EQ(renderError, testCase.renderError) << "Test case: " << testCase.name;
    }
}

struct tCLIOutput
{
    bool success = false;
    std::string output;
    std::vector<std::string> diagnostics;
};

tCLIOutput runCommand(const std::string &command)
{
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result += buffer.data();
    }
    int status = pclose(pipe);
    tCLIOutput output;
    // clang-format off
#if defined(_WIN32) || defined(_WIN64)
#define get_exit_status(status) status
#else
#define get_exit_status(status) ((WIFEXITED(status)) ? WEXITSTATUS(status) : -1)
#endif
    // clang-format on
    if (get_exit_status(status) == 0)
    {
        output.success = true;
        output.output = result;
    }
    else
    {
        output.success = false;
        // split diagnostics by line
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line))
        {
            output.diagnostics.push_back(line);
        }
    }
    return output;
}

TEST_P(AcceptanceTest, CompareCompileByCLI)
{
    auto testCase = GetParam();

    auto cliOutput = runCommand(TPP_EXE " " + std::filesystem::absolute("TestCases/" + testCase.name).string() + " 2>&1");

    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> fileDiags;
    fileDiags.reserve(testCase.sources.size());
    for (const auto &src : testCase.sources)
    {
        if (src.isTypes)
        {
            compiler.add_types(src.content, fileDiags.emplace_back(src.url).diagnostics);
        }
        else
        {
            compiler.add_templates(src.content, fileDiags.emplace_back(src.url).diagnostics);
        }
    }
    tpp::CompilerOutput expectedOutput;
    bool expectedSuccess = compiler.compile(expectedOutput);

    EXPECT_EQ(cliOutput.success, expectedSuccess) << "Test case: " << testCase.name;

    if (cliOutput.success != expectedSuccess)
    {
        return;
    }

    if (expectedSuccess)
    {
        tpp::CompilerOutput actualOutput = nlohmann::json::parse(cliOutput.output);
        EXPECT_EQ(actualOutput, expectedOutput) << "Test case: " << testCase.name
                                                << "\nExpected output: " << nlohmann::json(expectedOutput).dump(2)
                                                << "\nActual output: " << nlohmann::json(actualOutput).dump(2);
    }
    else
    {
        // convert expected diagnostics to gcc-style strings and compare with CLI output
        std::vector<std::string> expectedDiagStrings;
        for (const auto &fileDiags : testCase.expectedDiagnostics)
        {
            auto copy = fileDiags;
            copy.uri = std::filesystem::absolute("TestCases/" + copy.uri).string();
            for (const auto &d : copy.toGCCDiagnostics())
            {
                expectedDiagStrings.push_back(d);
            }
        }
        EXPECT_EQ(cliOutput.diagnostics, expectedDiagStrings)
            << "Test case: " << testCase.name
            << "\nActual diagnostics: " << nlohmann::json(cliOutput.diagnostics).dump(2);
    }
}

INSTANTIATE_TEST_SUITE_P(AcceptanceTests, AcceptanceTest, ::testing::ValuesIn(GetTestCases()), [](const testing::TestParamInfo<AcceptanceTest::ParamType> &info)
                         { return info.param.name; });

class AcceptanceTestOnlyPositives : public ::testing::TestWithParam<tTestCase>
{
};

TEST_P(AcceptanceTestOnlyPositives, CompareRenderByCLI)
{
    auto testCase = GetParam();

    if (!testCase.expectSuccess)
    {
        // count as success if compilation fails, we will check diagnostics in the previous test
        return;
    }

    auto sCommand = TPP_EXE " " + std::filesystem::absolute("TestCases/" + testCase.name).string() + " | " RENDER_TPP_EXE " main '" + testCase.input.dump() + "' 2>&1";
    auto cliOutput = runCommand(sCommand);
    EXPECT_TRUE(cliOutput.success) << "Test case: " << testCase.name
                                   << "\nCommand: " << sCommand
                                   << "\nCLI output: " << cliOutput.output
                                   << "\nCLI diagnostics: " << nlohmann::json(cliOutput.diagnostics).dump(2);
    if (!cliOutput.success)
    {
        return;
    }
    EXPECT_EQ(cliOutput.output, testCase.expectedOutput) << "Test case: " << testCase.name
                                                         << "\nExpected output: " << testCase.expectedOutput
                                                         << "\nActual output: " << cliOutput.output;
}

std::vector<tTestCase> GetPositiveTestCases()
{
    std::vector<tTestCase> allCases = GetTestCases();
    std::vector<tTestCase> positiveCases;
    for (const auto &testCase : allCases)
    {
        if (testCase.expectSuccess)
        {
            positiveCases.push_back(testCase);
        }
    }
    return positiveCases;
}

INSTANTIATE_TEST_SUITE_P(AcceptanceTestOnlyPositives, AcceptanceTestOnlyPositives, ::testing::ValuesIn(GetPositiveTestCases()), [](const testing::TestParamInfo<AcceptanceTestOnlyPositives::ParamType> &info)
                         { return info.param.name; });
