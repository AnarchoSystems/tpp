#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <tpp/Compiler.h>

// ── Diagnostic pretty-printer (must be in tpp namespace) ─────────────────────

namespace tpp
{
    void PrintTo(const Diagnostic &d, std::ostream *os);
}

// ── Data structures ───────────────────────────────────────────────────────────

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
    std::vector<std::string> policies;
    std::string previewTemplateName = "main";
    std::vector<std::string> previewSignature;
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

void PrintTo(const tTestCase &testCase, std::ostream *os);

struct tErrors
{
    std::string getFunctionError;
    std::string renderError;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(tErrors, getFunctionError, renderError)
};

struct LspExpectedToken
{
    int line = 0;
    int character = 0;
    int length = 0;
    std::string type;
};

struct LspTokenSpec
{
    std::string testCase;
    std::string file;
    std::vector<LspExpectedToken> expected;
};

struct LspFoldingSpec
{
    std::string testCase;
    std::string file;
    std::vector<std::pair<int, int>> expected; // {startLine, endLine}
};

struct LspDefinitionSpec
{
    std::string name;
    std::string testCase;
    std::string file;
    int line = 0;
    int character = 0;
    std::optional<std::string> expected_file; // absent = expect no result
    int expected_line = -1;
    int expected_character = -1;
};

struct LspHoverSpec
{
    std::string name;
    std::string testCase;
    std::string file;
    int line = 0;
    int character = 0;
    std::vector<std::string> expected_contains;
};

// ── CLI helpers ───────────────────────────────────────────────────────────────

struct tCLIOutput
{
    bool success = false;
    std::string output;
    std::vector<std::string> diagnostics;
};

tCLIOutput runCommand(const std::string &command);
tCLIOutput runCommandDirect(const std::vector<std::string> &argv);
tCLIOutput runCommandDirect(const std::vector<std::string> &argv, const std::string &stdinData);

// ── Test case discovery (runtime filesystem scan) ─────────────────────────────

std::vector<tTestCase> GetTestCases();
std::vector<tTestCase> GetPositiveTestCases();
std::vector<tTestCase> GetNegativeTestCases();
std::vector<tTestCase> GetDiagnosticTestCases();
std::vector<LspTokenSpec> GetLspTokenSpecs();
std::vector<LspFoldingSpec> GetLspFoldingSpecs();
std::vector<LspDefinitionSpec> GetLspDefinitionSpecs();
std::vector<LspHoverSpec> GetLspHoverSpecs();
