#include "TestUtils.h"
#include "TestCaseIO.h"
#include <tpp/ProjectConfigResolver.h>

#include <array>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if !defined(_WIN32)
extern char **environ;
#endif

namespace tpp {
void PrintTo(const Diagnostic &d, std::ostream *os) {
    nlohmann::json j = d;
    *os << j.dump(2);
}
} // namespace tpp

void PrintTo(const tTestCase &testCase, std::ostream *os) {
    for (char c : testCase.name) {
        if (std::isalnum(c)) {
            *os << c;
        } else {
            *os << ' ';
        }
    }
}

struct TestCaseExpectations {
    bool expectSuccess = false;
    std::optional<std::string> expectedOutput;
    std::vector<tpp::DiagnosticLSPMessage> expectedDiagnostics;
    std::string getFunctionError;
    std::string renderError;
    std::vector<LspTokenSpec> lspTokenSpecs;
    std::vector<LspFoldingSpec> lspFoldingSpecs;
    std::vector<LspDefinitionSpec> lspDefinitionSpecs;
    std::vector<LspHoverSpec> lspHoverSpecs;
};

static void loadLspSpecs(const nlohmann::json &lspJson,
                         const std::string &testCaseName,
                         TestCaseExpectations &expectations) {
    if (lspJson.contains("semantic_tokens")) {
        LspTokenSpec spec;
        spec.testCase = testCaseName;
        spec.file = lspJson.at("semantic_tokens").at("file").get<std::string>();
        for (const auto &token : lspJson.at("semantic_tokens").at("expected_tokens")) {
            spec.expected.push_back({token.at("line").get<int>(),
                                     token.at("character").get<int>(),
                                     token.at("length").get<int>(),
                                     token.at("type").get<std::string>()});
        }
        expectations.lspTokenSpecs.push_back(std::move(spec));
    }

    if (lspJson.contains("folding_ranges")) {
        LspFoldingSpec spec;
        spec.testCase = testCaseName;
        spec.file = lspJson.at("folding_ranges").at("file").get<std::string>();
        for (const auto &range : lspJson.at("folding_ranges").at("expected")) {
            spec.expected.push_back({range.at("startLine").get<int>(),
                                     range.at("endLine").get<int>()});
        }
        expectations.lspFoldingSpecs.push_back(std::move(spec));
    }

    if (lspJson.contains("go_to_definition") && lspJson.at("go_to_definition").is_array()) {
        for (const auto &definition : lspJson.at("go_to_definition")) {
            LspDefinitionSpec spec;
            spec.name = definition.at("name").get<std::string>();
            spec.testCase = testCaseName;
            spec.file = definition.at("file").get<std::string>();
            spec.line = definition.at("line").get<int>();
            spec.character = definition.at("character").get<int>();
            if (definition.contains("expected_file")) {
                spec.expected_file = definition.at("expected_file").get<std::string>();
            }
            if (definition.contains("expected_line")) {
                spec.expected_line = definition.at("expected_line").get<int>();
            }
            if (definition.contains("expected_character")) {
                spec.expected_character = definition.at("expected_character").get<int>();
            }
            expectations.lspDefinitionSpecs.push_back(std::move(spec));
        }
    }

    if (lspJson.contains("hover") && lspJson.at("hover").is_array()) {
        for (const auto &hover : lspJson.at("hover")) {
            LspHoverSpec spec;
            spec.name = hover.at("name").get<std::string>();
            spec.testCase = testCaseName;
            spec.file = hover.at("file").get<std::string>();
            spec.line = hover.at("line").get<int>();
            spec.character = hover.at("character").get<int>();
            for (const auto &expected : hover.at("expected_contains")) {
                spec.expected_contains.push_back(expected.get<std::string>());
            }
            expectations.lspHoverSpecs.push_back(std::move(spec));
        }
    }
}

static TestCaseExpectations loadCaseExpectations(const std::filesystem::path &path,
                                                 const std::string &testCaseName) {
    TestCaseExpectations expectations;
    auto specJson = test_support::load_test_case_spec(path);

    if (const auto expectedOutput = test_support::load_expected_output(path, specJson);
        expectedOutput.has_value()) {
        expectations.expectSuccess = true;
        expectations.expectedOutput = *expectedOutput;
    } else {
        expectations.expectSuccess = specJson.value("expect_success", false);
    }

    if (specJson.contains("expected_diagnostics")) {
        expectations.expectedDiagnostics =
            specJson.at("expected_diagnostics").get<std::vector<tpp::DiagnosticLSPMessage>>();
    }

    if (specJson.contains("expected_errors")) {
        const tErrors errors = specJson.at("expected_errors").get<tErrors>();
        expectations.getFunctionError = errors.getFunctionError;
        expectations.renderError = errors.renderError;
    }

    if (specJson.contains("lsp")) {
        loadLspSpecs(specJson.at("lsp"), testCaseName, expectations);
    }

    return expectations;
}

// ── extract() ────────────────────────────────────────────────────────────────

tLoadedTestCase tTestCase::extract() const {
    tLoadedTestCase loaded;
    loaded.name = name;
    const auto expectations = loadCaseExpectations(path, name);
    const auto inputs = tpp::load_project_inputs(path);

    for (const auto &source : inputs.sourceFiles) {
        std::string relPath = std::filesystem::relative(source.path, path).string();
        loaded.sources.push_back({name + "/" + relPath, test_support::read_text_file(source.path), source.isTypes});
    }
    for (const auto &policyPath : inputs.policyFiles) {
        if (std::filesystem::is_regular_file(policyPath)) {
            loaded.policies.push_back(test_support::read_text_file(policyPath));
        }
    }

    // Preview selection: read template/signature/input from previews[0].
    const auto &config = inputs.config;
    if (config.contains("previews") && config["previews"].is_array() &&
        !config["previews"].empty()) {
        const auto &preview = config["previews"][0];
        loaded.previewTemplateName = preview.value("template", "main");
        if (preview.contains("signature") && preview["signature"].is_array()) {
            for (const auto &entry : preview["signature"]) {
                loaded.previewSignature.push_back(entry.get<std::string>());
            }
        }

        if (preview.contains("input")) {
            loaded.input = preview["input"];
        } else {
            loaded.input = nlohmann::json::object();
        }
    } else {
        loaded.input = nlohmann::json::object();
    }

    loaded.expectSuccess = expectations.expectSuccess;
    if (expectations.expectedOutput.has_value()) {
        loaded.expectedOutput = *expectations.expectedOutput;
    }
    loaded.expectedDiagnostics = expectations.expectedDiagnostics;
    loaded.getFunctionError = expectations.getFunctionError;
    loaded.renderError = expectations.renderError;

    if (loaded.sources.empty()) {
        throw std::runtime_error("No .tpp source files found in " + path.string());
    }

    return loaded;
}

// ── Test case discovery ───────────────────────────────────────────────────────

static std::vector<tTestCase> scanTestCases() {
    std::vector<tTestCase> cases;
    const std::filesystem::path root = "TestCases";
    if (!std::filesystem::is_directory(root)) {
        return cases;
    }

    for (const auto &entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }

        const auto path = entry.path();
        if (!std::filesystem::exists(path / "tpp-config.json")) {
            continue;
        }

        tTestCase testCase;
        testCase.name = path.filename().string();
        testCase.path = path;
        testCase.expectSuccess = loadCaseExpectations(path, testCase.name).expectSuccess;
        cases.push_back(std::move(testCase));
    }

    std::sort(cases.begin(), cases.end(), [](const tTestCase &left, const tTestCase &right) {
        return left.name < right.name;
    });
    return cases;
}

static const std::vector<tTestCase> &allTestCases() {
    static const std::vector<tTestCase> cases = scanTestCases();
    return cases;
}

std::vector<tTestCase> GetTestCases() {
    return allTestCases();
}

std::vector<tTestCase> GetDiagnosticTestCases() {
    std::vector<tTestCase> cases;
    for (const auto &testCase : allTestCases()) {
        const auto expectations = loadCaseExpectations(testCase.path, testCase.name);
        if (!expectations.expectSuccess && !expectations.expectedDiagnostics.empty()) {
            cases.push_back(testCase);
        }
    }
    return cases;
}

std::vector<tTestCase> GetNegativeTestCases() {
    std::vector<tTestCase> cases;
    for (const auto &testCase : allTestCases()) {
        if (!testCase.expectSuccess) {
            cases.push_back(testCase);
        }
    }
    return cases;
}

std::vector<tTestCase> GetPositiveTestCases() {
    std::vector<tTestCase> cases;
    for (const auto &testCase : allTestCases()) {
        if (testCase.expectSuccess) {
            cases.push_back(testCase);
        }
    }
    return cases;
}

std::vector<LspTokenSpec> GetLspTokenSpecs() {
    std::vector<LspTokenSpec> specs;
    for (const auto &testCase : allTestCases()) {
        auto expectations = loadCaseExpectations(testCase.path, testCase.name);
        specs.insert(specs.end(),
                     expectations.lspTokenSpecs.begin(),
                     expectations.lspTokenSpecs.end());
    }
    return specs;
}

std::vector<LspFoldingSpec> GetLspFoldingSpecs() {
    std::vector<LspFoldingSpec> specs;
    for (const auto &testCase : allTestCases()) {
        auto expectations = loadCaseExpectations(testCase.path, testCase.name);
        specs.insert(specs.end(),
                     expectations.lspFoldingSpecs.begin(),
                     expectations.lspFoldingSpecs.end());
    }
    return specs;
}

std::vector<LspDefinitionSpec> GetLspDefinitionSpecs() {
    std::vector<LspDefinitionSpec> specs;
    for (const auto &testCase : allTestCases()) {
        auto expectations = loadCaseExpectations(testCase.path, testCase.name);
        specs.insert(specs.end(),
                     expectations.lspDefinitionSpecs.begin(),
                     expectations.lspDefinitionSpecs.end());
    }
    return specs;
}

std::vector<LspHoverSpec> GetLspHoverSpecs() {
    std::vector<LspHoverSpec> specs;
    for (const auto &testCase : allTestCases()) {
        auto expectations = loadCaseExpectations(testCase.path, testCase.name);
        specs.insert(specs.end(),
                     expectations.lspHoverSpecs.begin(),
                     expectations.lspHoverSpecs.end());
    }
    return specs;
}

// ── CLI runner ────────────────────────────────────────────────────────────────
namespace
{

std::string normalize_cli_text(std::string text)
{
    std::string normalized;
    normalized.reserve(text.size());

    for (size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\r') {
            if (index + 1 < text.size() && text[index + 1] == '\n') {
                continue;
            }
            normalized.push_back('\n');
            continue;
        }
        normalized.push_back(text[index]);
    }

    return normalized;
}

tCLIOutput make_cli_output(int exitCode, std::string result)
{
    result = normalize_cli_text(std::move(result));

    tCLIOutput output;
    if (exitCode == 0) {
        output.success = true;
        output.output = std::move(result);
        return output;
    }

    output.success = false;
    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
        output.diagnostics.push_back(line);
    }
    return output;
}

#if defined(_WIN32)

std::wstring widen(const std::string &value)
{
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                             static_cast<int>(value.size()),
                                             nullptr, 0);
    if (required <= 0) {
        throw std::runtime_error("MultiByteToWideChar() failed");
    }

    std::wstring result(static_cast<size_t>(required), L'\0');
    const int converted = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                              static_cast<int>(value.size()),
                                              result.data(), required);
    if (converted != required) {
        throw std::runtime_error("MultiByteToWideChar() produced incomplete output");
    }

    return result;
}

std::string format_windows_error(const std::string &context, DWORD errorCode)
{
    LPWSTR rawMessage = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&rawMessage),
        0,
        nullptr);

    std::string message = context + " (error " + std::to_string(errorCode) + ")";
    if (size == 0 || rawMessage == nullptr) {
        return message;
    }

    std::wstring wideMessage(rawMessage, rawMessage + size);
    LocalFree(rawMessage);

    while (!wideMessage.empty() &&
           (wideMessage.back() == L'\r' || wideMessage.back() == L'\n' || wideMessage.back() == L' ')) {
        wideMessage.pop_back();
    }

    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideMessage.c_str(),
                                             static_cast<int>(wideMessage.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) {
        return message;
    }

    std::string utf8Message(static_cast<size_t>(utf8Size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideMessage.c_str(),
                        static_cast<int>(wideMessage.size()),
                        utf8Message.data(), utf8Size, nullptr, nullptr);
    return message + ": " + utf8Message;
}

std::wstring quote_windows_arg(const std::string &arg)
{
    const std::wstring wideArg = widen(arg);
    const bool needsQuotes = wideArg.empty() ||
                             wideArg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
    if (!needsQuotes) {
        return wideArg;
    }

    std::wstring quoted = L"\"";
    size_t backslashCount = 0;
    for (wchar_t ch : wideArg) {
        if (ch == L'\\') {
            ++backslashCount;
            continue;
        }

        if (ch == L'"') {
            quoted.append(backslashCount * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashCount = 0;
            continue;
        }

        quoted.append(backslashCount, L'\\');
        backslashCount = 0;
        quoted.push_back(ch);
    }

    quoted.append(backslashCount * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring build_windows_command_line(const std::vector<std::string> &args)
{
    std::wstring commandLine;
    for (size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            commandLine.push_back(L' ');
        }
        commandLine += quote_windows_arg(args[index]);
    }
    return commandLine;
}

class ScopedHandle
{
  public:
    ScopedHandle() = default;

    explicit ScopedHandle(HANDLE handle)
        : handle_(handle)
    {
    }

    ~ScopedHandle()
    {
        reset();
    }

    ScopedHandle(const ScopedHandle &) = delete;
    ScopedHandle &operator=(const ScopedHandle &) = delete;

    ScopedHandle(ScopedHandle &&other) noexcept
        : handle_(other.release())
    {
    }

    ScopedHandle &operator=(ScopedHandle &&other) noexcept
    {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    HANDLE get() const
    {
        return handle_;
    }

    HANDLE release()
    {
        HANDLE released = handle_;
        handle_ = nullptr;
        return released;
    }

    void reset(HANDLE handle = nullptr)
    {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

  private:
    HANDLE handle_ = nullptr;
};

tCLIOutput run_command_windows(const std::vector<std::string> &args,
                               const std::string *stdinData = nullptr)
{
    SECURITY_ATTRIBUTES securityAttributes = {};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE stdoutReadRaw = nullptr;
    HANDLE stdoutWriteRaw = nullptr;
    if (!CreatePipe(&stdoutReadRaw, &stdoutWriteRaw, &securityAttributes, 0)) {
        throw std::runtime_error(format_windows_error("CreatePipe(stdout) failed", GetLastError()));
    }
    ScopedHandle stdoutRead(stdoutReadRaw);
    ScopedHandle stdoutWrite(stdoutWriteRaw);
    if (!SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error(format_windows_error("SetHandleInformation(stdoutRead) failed", GetLastError()));
    }

    ScopedHandle stdinRead;
    ScopedHandle stdinWrite;
    if (stdinData != nullptr) {
        HANDLE stdinReadRaw = nullptr;
        HANDLE stdinWriteRaw = nullptr;
        if (!CreatePipe(&stdinReadRaw, &stdinWriteRaw, &securityAttributes, 0)) {
            throw std::runtime_error(format_windows_error("CreatePipe(stdin) failed", GetLastError()));
        }
        stdinRead.reset(stdinReadRaw);
        stdinWrite.reset(stdinWriteRaw);
        if (!SetHandleInformation(stdinWrite.get(), HANDLE_FLAG_INHERIT, 0)) {
            throw std::runtime_error(format_windows_error("SetHandleInformation(stdinWrite) failed", GetLastError()));
        }
    }

    ScopedHandle nullInput(CreateFileW(L"NUL", GENERIC_READ,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (nullInput.get() == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(format_windows_error("CreateFileW(NUL) failed", GetLastError()));
    }

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = stdinRead.get() != nullptr ? stdinRead.get() : nullInput.get();
    startupInfo.hStdOutput = stdoutWrite.get();
    startupInfo.hStdError = stdoutWrite.get();

    PROCESS_INFORMATION processInfo = {};
    std::wstring commandLine = build_windows_command_line(args);
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    const BOOL created = CreateProcessW(nullptr,
                                        mutableCommandLine.data(),
                                        nullptr,
                                        nullptr,
                                        TRUE,
                                        0,
                                        nullptr,
                                        nullptr,
                                        &startupInfo,
                                        &processInfo);
    if (!created) {
        throw std::runtime_error(format_windows_error("CreateProcessW() failed", GetLastError()));
    }

    ScopedHandle process(processInfo.hProcess);
    ScopedHandle thread(processInfo.hThread);
    stdoutWrite.reset();
    stdinRead.reset();
    nullInput.reset();

    if (stdinData != nullptr) {
        size_t offset = 0;
        while (offset < stdinData->size()) {
            const DWORD remaining = static_cast<DWORD>(
                std::min<size_t>(stdinData->size() - offset, 64 * 1024));
            DWORD written = 0;
            if (!WriteFile(stdinWrite.get(), stdinData->data() + offset, remaining, &written, nullptr)) {
                const DWORD error = GetLastError();
                if (error != ERROR_BROKEN_PIPE) {
                    throw std::runtime_error(format_windows_error("WriteFile(stdin) failed", error));
                }
                break;
            }
            if (written == 0) {
                break;
            }
            offset += written;
        }
        stdinWrite.reset();
    }

    std::string result;
    std::array<char, 4096> buffer = {};
    while (true) {
        DWORD bytesRead = 0;
        if (!ReadFile(stdoutRead.get(), buffer.data(),
                      static_cast<DWORD>(buffer.size()), &bytesRead, nullptr)) {
            const DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                break;
            }
            throw std::runtime_error(format_windows_error("ReadFile(stdout) failed", error));
        }
        if (bytesRead == 0) {
            break;
        }
        result.append(buffer.data(), bytesRead);
    }
    stdoutRead.reset();

    WaitForSingleObject(process.get(), INFINITE);
    DWORD exitCode = EXIT_FAILURE;
    if (!GetExitCodeProcess(process.get(), &exitCode)) {
        throw std::runtime_error(format_windows_error("GetExitCodeProcess() failed", GetLastError()));
    }

    return make_cli_output(static_cast<int>(exitCode), std::move(result));
}

#else

// agent went into full nervous breakdown because of a hang
// code works, so whatever...
static tCLIOutput runCommandArgv(const char *const argv[], const std::string *stdinData = nullptr) {
    int stdoutPipe[2];
    if (pipe(stdoutPipe) == -1) {
        throw std::runtime_error("pipe() failed!");
    }

    int stdinPipe[2] = {-1, -1};
    if (stdinData && pipe(stdinPipe) == -1) {
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        throw std::runtime_error("pipe() failed!");
    }

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (stdinData) {
        posix_spawn_file_actions_adddup2(&fa, stdinPipe[0], STDIN_FILENO);
        posix_spawn_file_actions_addclose(&fa, stdinPipe[0]);
        posix_spawn_file_actions_addclose(&fa, stdinPipe[1]);
    } else {
        // stdin → /dev/null when the caller does not provide input.
        posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    }
    posix_spawn_file_actions_adddup2(&fa, stdoutPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&fa, stdoutPipe[1], STDERR_FILENO);
    // Close the original pipe fds inside the spawned process
    posix_spawn_file_actions_addclose(&fa, stdoutPipe[0]);
    posix_spawn_file_actions_addclose(&fa, stdoutPipe[1]);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
#if defined(__APPLE__)
    // Close every fd the child would otherwise inherit except those set by fa above.
    // This is the key fix: VS Code TestMate keeps open write-ends of pipes that
    // it created to capture test output; those would otherwise prevent our pipe
    // from reaching EOF and cause the parent read() to block indefinitely.
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT);
#endif

    pid_t pid = -1;
    int err = posix_spawn(&pid, argv[0], &fa, &attr,
                          const_cast<char *const *>(argv), environ);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&attr);
    close(stdoutPipe[1]);
    if (stdinData) {
        close(stdinPipe[0]);
    }

    if (err != 0) {
        close(stdoutPipe[0]);
        if (stdinData) {
            close(stdinPipe[1]);
        }
        throw std::runtime_error(std::string("posix_spawn failed: ") + strerror(err));
    }

    if (stdinData) {
        size_t offset = 0;
        while (offset < stdinData->size()) {
            ssize_t written = write(stdinPipe[1], stdinData->data() + offset, stdinData->size() - offset);
            if (written <= 0) {
                break;
            }
            offset += static_cast<size_t>(written);
        }
        close(stdinPipe[1]);
    }

    std::string result;
    {
        std::array<char, 4096> buf;
        ssize_t n;
        while ((n = read(stdoutPipe[0], buf.data(), buf.size())) > 0) {
            result.append(buf.data(), static_cast<size_t>(n));
        }
    }
    close(stdoutPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return make_cli_output(exitCode, std::move(result));
}

#endif

} // namespace

#if defined(_WIN32)

tCLIOutput runCommand(const std::string &command) {
    return run_command_windows({"cmd.exe", "/d", "/s", "/c", command});
}

tCLIOutput runCommandDirect(const std::vector<std::string> &args) {
    return run_command_windows(args);
}

tCLIOutput runCommandDirect(const std::vector<std::string> &args, const std::string &stdinData) {
    return run_command_windows(args, &stdinData);
}

#else

tCLIOutput runCommand(const std::string &command) {
    const char *argv[] = {"/bin/sh", "-c", command.c_str(), nullptr};
    return runCommandArgv(argv);
}

tCLIOutput runCommandDirect(const std::vector<std::string> &args) {
    std::vector<const char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &a : args) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);
    return runCommandArgv(argv.data());
}

tCLIOutput runCommandDirect(const std::vector<std::string> &args, const std::string &stdinData) {
    std::vector<const char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &a : args) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);
    return runCommandArgv(argv.data(), &stdinData);
}

#endif
