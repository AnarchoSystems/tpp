#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace test_support
{
    inline std::string read_text_file(const std::filesystem::path &path)
    {
        std::ifstream file(path);
        return std::string(std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>());
    }

    inline nlohmann::json load_test_case_spec(const std::filesystem::path &testDir)
    {
        const auto specPath = testDir / "test-case.json";
        if (!std::filesystem::exists(specPath))
            throw std::runtime_error("Missing test-case.json in " + testDir.string());
        return nlohmann::json::parse(read_text_file(specPath));
    }

    inline std::optional<std::string> load_expected_output(const std::filesystem::path &testDir,
                                                           const nlohmann::json &specJson)
    {
        const auto expectedOutputPath = testDir / "expected_output.txt";
        const bool hasExpectedOutputFile = std::filesystem::exists(expectedOutputPath);
        const bool hasExpectedOutputJson = specJson.contains("expected_output");

        if (hasExpectedOutputFile && hasExpectedOutputJson)
        {
            throw std::runtime_error(
                "expected output is defined in both expected_output.txt and test-case.json in " +
                testDir.string());
        }

        if (hasExpectedOutputFile)
            return read_text_file(expectedOutputPath);

        if (hasExpectedOutputJson)
            return specJson.at("expected_output").get<std::string>();

        return std::nullopt;
    }

    inline std::optional<std::string> load_expected_output(const std::filesystem::path &testDir)
    {
        const auto specJson = load_test_case_spec(testDir);
        return load_expected_output(testDir, specJson);
    }
}