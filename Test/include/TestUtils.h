#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
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

void PrintTo(const tTestCase &testCase, std::ostream *os);

struct tErrors
{
    std::string getFunctionError;
    std::string renderError;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(tErrors, getFunctionError, renderError)
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

// ── Test case discovery (driven by configure-time compile definitions) ────────

std::vector<tTestCase> GetTestCases();
std::vector<tTestCase> GetPositiveTestCases();
std::vector<tTestCase> GetNegativeTestCases();
std::vector<tTestCase> GetDiagnosticTestCases();
