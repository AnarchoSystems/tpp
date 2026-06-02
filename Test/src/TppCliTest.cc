#include "TestUtils.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

struct TemporaryProjectDirectory
{
    std::filesystem::path path;

    TemporaryProjectDirectory()
    {
        const auto uniqueSuffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        path = std::filesystem::temp_directory_path() / ("tpp-print-inputs-" + uniqueSuffix);
        std::filesystem::create_directories(path);
    }

    ~TemporaryProjectDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

void writeFile(const std::filesystem::path &path, const std::string &content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << content;
}

std::vector<std::string> splitLines(const std::string &output)
{
    std::vector<std::string> lines;
    std::stringstream stream(output);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

TEST(TppCliTest, PrintInputsEmitsResolvedSourcesAndPoliciesInStableOrder)
{
    TemporaryProjectDirectory project;
    writeFile(project.path / "tpp-config.json", R"json({
  "types": ["types/*.tpp"],
  "templates": ["templates/*.tpp"],
  "replacement-policies": ["policies/*.policy.json"]
})json");

    writeFile(project.path / "types/beta.tpp", "struct Beta { value: int }");
    writeFile(project.path / "types/alpha.tpp", "struct Alpha { value: int }");
    writeFile(project.path / "templates/zeta.tpp", "zeta");
    writeFile(project.path / "templates/alpha.tpp", "alpha");
    writeFile(project.path / "policies/beta.policy.json", "[]");
    writeFile(project.path / "policies/alpha.policy.json", "[]");

    const auto cliOutput = runCommandDirect({TPP_EXE, "--print-inputs", project.path.string()});

    ASSERT_TRUE(cliOutput.success)
        << "Output: " << cliOutput.output
        << "\nDiagnostics: " << nlohmann::json(cliOutput.diagnostics).dump(2);

    const std::vector<std::string> expected = {
        (project.path / "tpp-config.json").lexically_normal().string(),
        (project.path / "types/alpha.tpp").lexically_normal().string(),
        (project.path / "types/beta.tpp").lexically_normal().string(),
        (project.path / "templates/alpha.tpp").lexically_normal().string(),
        (project.path / "templates/zeta.tpp").lexically_normal().string(),
        (project.path / "policies/alpha.policy.json").lexically_normal().string(),
        (project.path / "policies/beta.policy.json").lexically_normal().string(),
    };

    EXPECT_EQ(splitLines(cliOutput.output), expected);
}

} // anonymous namespace