#include "TestUtils.h"
#include "TestCases.h" // generated at configure time
#include <sys/wait.h>

namespace tpp
{
    void PrintTo(const Diagnostic &d, std::ostream *os)
    {
        nlohmann::json j = d;
        *os << j.dump(2);
    }
}

void PrintTo(const tTestCase &testCase, std::ostream *os)
{
    for (char c : testCase.name)
    {
        if (std::isalnum(c))
            *os << c;
        else
            *os << ' ';
    }
}

// ── File helpers ──────────────────────────────────────────────────────────────

static std::string readFile(const std::filesystem::path &path)
{
    std::ifstream f(path);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static bool matchGlob(const std::string &pattern, const std::string &text)
{
    size_t pi = 0, ti = 0, starPos = std::string::npos, matchPos = 0;
    while (ti < text.size())
    {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti]))
            { ++pi; ++ti; }
        else if (pi < pattern.size() && pattern[pi] == '*')
            { starPos = pi++; matchPos = ti; }
        else if (starPos != std::string::npos)
            { pi = starPos + 1; ti = ++matchPos; }
        else
            return false;
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

static std::vector<std::filesystem::path> expandPattern(const std::filesystem::path &baseDir,
                                                        const std::string &pattern)
{
    std::filesystem::path patPath(pattern);
    std::string filename = patPath.filename().string();
    std::filesystem::path parentDir = baseDir / patPath.parent_path();

    if (filename.find('*') == std::string::npos)
        return {baseDir / pattern};

    std::vector<std::filesystem::path> results;
    if (std::filesystem::is_directory(parentDir))
    {
        for (const auto &entry : std::filesystem::directory_iterator(parentDir))
        {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            if (name.size() < 4 || name.substr(name.size() - 4) != ".tpp") continue;
            if (matchGlob(filename, name)) results.push_back(entry.path());
        }
        std::sort(results.begin(), results.end());
    }
    return results;
}

// ── extract() ────────────────────────────────────────────────────────────────

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
            loaded.sources.push_back({name + "/" + relPath, readFile(filePath), true});
        }
    }
    for (const auto &pattern : config.value("templates", nlohmann::json::array()))
    {
        for (const auto &filePath : expandPattern(path, pattern.get<std::string>()))
        {
            std::string relPath = std::filesystem::relative(filePath, path).string();
            loaded.sources.push_back({name + "/" + relPath, readFile(filePath), false});
        }
    }
    for (const auto &policyEntry : config.value("replacement-policies", nlohmann::json::array()))
    {
        std::ifstream pf(path / policyEntry.get<std::string>());
        if (pf.is_open())
        {
            try { loaded.policies.push_back(nlohmann::json::parse(
                    std::string((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>()))); }
            catch (...) {}
        }
    }

    // Input: prefer previews[0].input in tpp-config.json, fall back to input.json
    if (config.contains("previews") && config["previews"].is_array() &&
        !config["previews"].empty() && config["previews"][0].contains("input"))
    {
        // Always use the inline value as-is (string, object, or array)
        loaded.input = config["previews"][0]["input"];
    }
    else
    {
        std::ifstream inputFile(path / "input.json");
        if (!inputFile.is_open())
            throw std::runtime_error("Failed to open input.json in " + path.string());
        loaded.input = nlohmann::json::parse(
            std::string((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>()));
    }

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
        std::ifstream diagFile(path / "expected_diagnostics.json");
        if (diagFile.is_open())
        {
            auto j = nlohmann::json::parse(std::string(
                (std::istreambuf_iterator<char>(diagFile)), std::istreambuf_iterator<char>()));
            loaded.expectedDiagnostics = j.get<std::vector<tpp::DiagnosticLSPMessage>>();
        }
        std::ifstream errFile(path / "expected_errors.json");
        if (errFile.is_open())
        {
            auto j = nlohmann::json::parse(std::string(
                (std::istreambuf_iterator<char>(errFile)), std::istreambuf_iterator<char>()));
            tErrors errs = j.get<tErrors>();
            loaded.getFunctionError = errs.getFunctionError;
            loaded.renderError = errs.renderError;
        }
    }

    if (loaded.sources.empty())
        throw std::runtime_error("No .tpp source files found in " + path.string());

    return loaded;
}

// ── Test case discovery ───────────────────────────────────────────────────────

static std::vector<tTestCase> parseCaseList(const char* const names[], bool expectSuccess)
{
    std::vector<tTestCase> cases;
    for (size_t i = 0; names[i] != nullptr; ++i)
    {
        tTestCase tc;
        tc.name = names[i];
        tc.path = std::filesystem::path("TestCases") / names[i];
        tc.expectSuccess = expectSuccess;
        cases.push_back(std::move(tc));
    }
    return cases;
}

std::vector<tTestCase> GetTestCases()
{
    auto all = parseCaseList(kTppSuccessCases, true);
    auto failures = parseCaseList(kTppFailureCases, false);
    all.insert(all.end(), failures.begin(), failures.end());
    return all;
}

std::vector<tTestCase> GetPositiveTestCases()
{
    return parseCaseList(kTppSuccessCases, true);
}

// ── CLI runner ────────────────────────────────────────────────────────────────

tCLIOutput runCommand(const std::string &command)
{
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();
    int status = pclose(pipe);
    tCLIOutput output;
#if defined(_WIN32) || defined(_WIN64)
    int exitCode = status;
#else
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    if (exitCode == 0)
    {
        output.success = true;
        output.output = result;
    }
    else
    {
        output.success = false;
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line))
            output.diagnostics.push_back(line);
    }
    return output;
}
