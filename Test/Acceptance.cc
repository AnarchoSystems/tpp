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

struct tLoadedTestCase
{
    std::string name;
    std::vector<SourceFile> sources;
    std::vector<nlohmann::json> policies;
    nlohmann::json input;
    bool expectSuccess = true;
    std::string expectedOutput;
    std::vector<tpp::DiagnosticLSPMessage> expectedDiagnostics;
    std::string getFunctionError;
    std::string renderError;
};

struct tTestCase
{
    std::string name;
    std::filesystem::path path;
    bool expectSuccess = true;

    tLoadedTestCase extract() const;
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

tLoadedTestCase tTestCase::extract() const
{
    tLoadedTestCase loaded;
    loaded.name = name;

    std::ifstream configFile(path / "tpp-config.json");
    if (!configFile.is_open())
        throw std::runtime_error("No tpp-config.json found in " + path.string());
    nlohmann::json config;
    try
    {
        config = nlohmann::json::parse(std::string(
            (std::istreambuf_iterator<char>(configFile)),
            std::istreambuf_iterator<char>()));
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Invalid tpp-config.json in " + path.string() + ": " + e.what());
    }

    for (const auto &pattern : config.value("types", nlohmann::json::array()))
    {
        for (const auto &filePath : expandPattern(path, pattern.get<std::string>()))
        {
            std::string relPath = std::filesystem::relative(filePath, path).string();
            std::string url = name + "/" + relPath;
            loaded.sources.push_back({url, readFile(filePath), true});
        }
    }
    for (const auto &pattern : config.value("templates", nlohmann::json::array()))
    {
        for (const auto &filePath : expandPattern(path, pattern.get<std::string>()))
        {
            std::string relPath = std::filesystem::relative(filePath, path).string();
            std::string url = name + "/" + relPath;
            loaded.sources.push_back({url, readFile(filePath), false});
        }
    }
    for (const auto &policyEntry : config.value("replacement-policies", nlohmann::json::array()))
    {
        std::filesystem::path policyPath = path / policyEntry.get<std::string>();
        std::ifstream pf(policyPath);
        if (pf.is_open())
        {
            try
            {
                loaded.policies.push_back(nlohmann::json::parse(
                    std::string((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>())));
            }
            catch (...) {}
        }
    }

    std::ifstream inputFile(path / "input.json");
    if (!inputFile.is_open())
        throw std::runtime_error("Failed to open input.json in " + path.string());
    loaded.input = nlohmann::json::parse(
        std::string((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>()));

    std::ifstream expectedOutputFile(path / "expected_output.txt");
    if (expectedOutputFile.is_open())
    {
        loaded.expectSuccess = true;
        loaded.expectedOutput = std::string(
            (std::istreambuf_iterator<char>(expectedOutputFile)), std::istreambuf_iterator<char>());
    }
    else
    {
        loaded.expectSuccess = false;
        std::ifstream expectedDiagnosticsFile(path / "expected_diagnostics.json");
        if (expectedDiagnosticsFile.is_open())
        {
            auto j = nlohmann::json::parse(std::string(
                (std::istreambuf_iterator<char>(expectedDiagnosticsFile)),
                std::istreambuf_iterator<char>()));
            loaded.expectedDiagnostics = j.get<std::vector<tpp::DiagnosticLSPMessage>>();
        }
        std::ifstream expectedErrorsFile(path / "expected_errors.json");
        if (expectedErrorsFile.is_open())
        {
            auto j = nlohmann::json::parse(std::string(
                (std::istreambuf_iterator<char>(expectedErrorsFile)),
                std::istreambuf_iterator<char>()));
            tErrors errs = j.get<tErrors>();
            loaded.getFunctionError = errs.getFunctionError;
            loaded.renderError = errs.renderError;
        }
    }

    if (loaded.sources.empty())
        throw std::runtime_error("No .tpp source files found in " + path.string());

    return loaded;
}

std::vector<tTestCase> GetTestCases()
{
    std::vector<tTestCase> testCases;

    for (const auto &entry : std::filesystem::directory_iterator("TestCases"))
    {
        if (!entry.is_directory())
            continue;
        if (!std::filesystem::exists(entry.path() / "tpp-config.json"))
            continue;
        auto &tc = testCases.emplace_back();
        tc.name = entry.path().filename().string();
        tc.path = entry.path();
        tc.expectSuccess = std::filesystem::exists(entry.path() / "expected_output.txt");
    }

    return testCases;
}

class AcceptanceTest : public ::testing::TestWithParam<tTestCase>
{
};

TEST_P(AcceptanceTest, RunTestCase)
{
    const auto testCase = GetParam().extract();

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

    // Load policies
    for (const auto &pol : testCase.policies)
    {
        std::string policyError;
        compiler.add_policy(pol, policyError);
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
    const auto testCase = GetParam().extract();

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
    for (const auto &pol : testCase.policies)
    {
        std::string policyError;
        compiler.add_policy(pol, policyError);
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
    const auto testCase = GetParam().extract();

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
