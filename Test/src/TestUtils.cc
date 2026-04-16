#include "TestUtils.h"
#include "TestCases.h" // generated at configure time
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>

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
        std::filesystem::path policyPath = path / policyEntry.get<std::string>();
        if (std::filesystem::is_regular_file(policyPath))
            loaded.policies.push_back(readFile(policyPath));
    }

    // Input: read from previews[0].input in tpp-config.json
    if (config.contains("previews") && config["previews"].is_array() &&
        !config["previews"].empty() && config["previews"][0].contains("input"))
    {
        loaded.input = config["previews"][0]["input"];
    }
    else 
    {
        loaded.input = nlohmann::json::object();
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

std::vector<tTestCase> GetDiagnosticTestCases()
{
    return parseCaseList(kTppDiagnosticCases, false);
}

std::vector<tTestCase> GetNegativeTestCases()
{
    return parseCaseList(kTppFailureCases, false);
}

std::vector<tTestCase> GetPositiveTestCases()
{
    return parseCaseList(kTppSuccessCases, true);
}

// ── CLI runner ────────────────────────────────────────────────────────────────
// agent went into full nervous breakdown because of a hang
// code works, so whatever...
static tCLIOutput runCommandArgv(const char *const argv[])
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
        throw std::runtime_error("pipe() failed!");

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    // stdin → /dev/null, stdout+stderr → our pipe write end
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&fa, pipefd[1], STDERR_FILENO);
    // Close the original pipe fds inside the spawned process
    posix_spawn_file_actions_addclose(&fa, pipefd[0]);
    posix_spawn_file_actions_addclose(&fa, pipefd[1]);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
#if defined(__APPLE__)
    // Close every fd the child would otherwise inherit except those set by fa above.
    // This is the key fix: VS Code TestMate keeps open write-ends of pipes that
    // it created to capture test output; those would otherwise prevent our pipe
    // from reaching EOF and cause the parent read() to block indefinitely.
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT);
#endif

    extern char **environ;
    pid_t pid = -1;
    int err = posix_spawn(&pid, argv[0], &fa, &attr,
                          const_cast<char *const *>(argv), environ);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&attr);
    close(pipefd[1]); // parent doesn't write

    if (err != 0) {
        close(pipefd[0]);
        throw std::runtime_error(std::string("posix_spawn failed: ") + strerror(err));
    }

    std::string result;
    {
        std::array<char, 4096> buf;
        ssize_t n;
        while ((n = read(pipefd[0], buf.data(), buf.size())) > 0)
            result.append(buf.data(), static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    tCLIOutput output;
    if (exitCode == 0) { output.success = true; output.output = result; }
    else {
        output.success = false;
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line))
            output.diagnostics.push_back(line);
    }
    return output;
}

tCLIOutput runCommand(const std::string &command)
{
    const char *argv[] = {"/bin/sh", "-c", command.c_str(), nullptr};
    return runCommandArgv(argv);
}

tCLIOutput runCommandDirect(const std::vector<std::string> &args)
{
    std::vector<const char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    return runCommandArgv(argv.data());
}
