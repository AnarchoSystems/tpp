#include "TestUtils.h"
#include <tpp/IR.h>
#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <fstream>
#include <vector>

// ── Path collection ──────────────────────────────────────────────────────────

static void collectPaths(const nlohmann::json &j, const std::string &prefix,
                         std::set<std::string> &out)
{
    if (j.is_object())
    {
        for (auto it = j.begin(); it != j.end(); ++it)
        {
            std::string path = prefix.empty() ? it.key() : prefix + "." + it.key();
            out.insert(path);
            collectPaths(it.value(), path, out);
        }
    }
    else if (j.is_array())
    {
        for (const auto &el : j)
            collectPaths(el, prefix + "[]", out);
    }
}

// ── Excluded paths (version fields change on every bump) ─────────────────────

static const std::set<std::string> kExcludedPaths = {
    "versionMajor", "versionMinor", "versionPatch"};

// ── Generated code shape checks ──────────────────────────────────────────────

static std::vector<std::filesystem::path> collectGeneratedImplFiles()
{
    const std::filesystem::path tppExePath(TPP_EXE);
    const auto buildDir = tppExePath.parent_path().parent_path();
    const auto generatedDir = buildDir / "Test";

    if (!std::filesystem::exists(generatedDir))
        return {};

    constexpr std::string_view kSuffix = "_implementation.cc";
    std::vector<std::filesystem::path> result;
    for (const auto &entry : std::filesystem::directory_iterator(generatedDir))
    {
        if (!entry.is_regular_file())
            continue;
        const auto fileName = entry.path().filename().string();
        if (fileName.rfind("error_", 0) == 0)
            continue;
        if (fileName.size() >= kSuffix.size() &&
            fileName.rfind(kSuffix) == fileName.size() - kSuffix.size())
        {
            result.push_back(entry.path());
        }
    }
    return result;
}

static void checkPatternInImplFiles(const std::string &pattern)
{
    const auto generatedImplFiles = collectGeneratedImplFiles();

    ASSERT_FALSE(generatedImplFiles.empty())
        << "No generated *_implementation.cc files found";

    std::vector<std::filesystem::path> missing;
    for (const auto &implPath : generatedImplFiles)
    {
        std::ifstream in(implPath);
        ASSERT_TRUE(in.good()) << "Cannot read generated implementation: " << implPath;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (content.find(pattern) == std::string::npos)
            missing.push_back(implPath);
    }

    if (!missing.empty())
    {
        std::string msg = "Missing \"" + pattern + "\" in generated implementation files:\n";
        for (const auto &path : missing)
            msg += "  - " + path.string() + "\n";
        FAIL() << msg;
    }
}

static void checkPatternInSomeImplFile(const std::string &pattern)
{
    const auto generatedImplFiles = collectGeneratedImplFiles();

    ASSERT_FALSE(generatedImplFiles.empty())
        << "No generated *_implementation.cc files found";

    for (const auto &implPath : generatedImplFiles)
    {
        std::ifstream in(implPath);
        ASSERT_TRUE(in.good()) << "Cannot read generated implementation: " << implPath;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (content.find(pattern) != std::string::npos)
            return;
    }

    FAIL() << "Missing \"" << pattern << "\" in all generated implementation files";
}

static void checkPatternAbsentInImplFiles(const std::string &pattern)
{
    const auto generatedImplFiles = collectGeneratedImplFiles();

    ASSERT_FALSE(generatedImplFiles.empty())
        << "No generated *_implementation.cc files found";

    std::vector<std::filesystem::path> present;
    for (const auto &implPath : generatedImplFiles)
    {
        std::ifstream in(implPath);
        ASSERT_TRUE(in.good()) << "Cannot read generated implementation: " << implPath;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (content.find(pattern) != std::string::npos)
            present.push_back(implPath);
    }

    if (!present.empty())
    {
        std::string msg = "Unexpected \"" + pattern + "\" in generated implementation files:\n";
        for (const auto &path : present)
            msg += "  - " + path.string() + "\n";
        FAIL() << msg;
    }
}

TEST(IRStability, GeneratedImplementationContainsTakeOutput)
{
    checkPatternInImplFiles(".takeOutput(");
}

TEST(IRStability, GeneratedImplementationContainsTppWriter)
{
    checkPatternInImplFiles("tpp::Writer");
}

TEST(IRStability, GeneratedImplementationDoesNotContainTppVM)
{
    checkPatternAbsentInImplFiles("tpp::VM");
}

TEST(IRStability, GeneratedImplementationUsesNativeVmHelpers)
{
    checkPatternInSomeImplFile(".emitForEach(");
    checkPatternInSomeImplFile(".emitCapturedBlockForEach(");
    checkPatternInSomeImplFile(".emitAlignedForEach(");
    checkPatternInSomeImplFile(".emitValue(");
}

TEST(IRStability, GeneratedImplementationDoesNotUseManualVmScaffolding)
{
    checkPatternAbsentInImplFiles("std::vector<tpp::Slot> _emitFrame");
    checkPatternAbsentInImplFiles(".beginForEach(");
    checkPatternAbsentInImplFiles("std::vector<std::string> _row");
    checkPatternAbsentInImplFiles(".beginCapture(");
    checkPatternAbsentInImplFiles(".endBlockCapture(");
}

// ── Single aggregate test ────────────────────────────────────────────────────

TEST(IRStability, SchemaCheck)
{
    auto cases = GetPositiveTestCases();
    ASSERT_FALSE(cases.empty()) << "No positive test cases found";

    int snapshotsChecked = 0;
    std::set<std::string> allExpectedPaths;
    std::set<std::string> allActualPaths;

    for (const auto &tc : cases)
    {
        auto snapshotPath = tc.path / "expected_ir.json";
        if (!std::filesystem::exists(snapshotPath))
            continue;

        try
        {
            // Load snapshot
            std::ifstream snapFile(snapshotPath);
            ASSERT_TRUE(snapFile.good()) << "Cannot read " << snapshotPath;
            nlohmann::json expectedIR;
            snapFile >> expectedIR;

            // Compile in-process
            auto loaded = tc.extract();
            tpp::TppProject project;
            for (const auto &src : loaded.sources)
            {
                if (src.isTypes)
                    project.add_type_source(src.content, src.url);
                else
                    project.add_template_source(src.content, src.url);
            }
            for (size_t index = 0; index < loaded.policies.size(); ++index)
                project.add_policy_source(loaded.policies[index], loaded.name + "/policy_" + std::to_string(index) + ".json");

            const auto compileResult = tpp::compile(project);
            ASSERT_TRUE(compileResult) << "Failed to compile test case: " << tc.name;

            nlohmann::json actualIR = compileResult.ir;

            // Collect key paths
            std::set<std::string> expectedPaths, actualPaths;
            collectPaths(expectedIR, "", expectedPaths);
            collectPaths(actualIR, "", actualPaths);

            allExpectedPaths.insert(expectedPaths.begin(), expectedPaths.end());
            allActualPaths.insert(actualPaths.begin(), actualPaths.end());
            ++snapshotsChecked;
        }
        catch (const std::exception &e)
        {
            FAIL() << "IR schema check failed for " << tc.name
                   << " (" << snapshotPath << "): " << e.what();
        }
    }

    ASSERT_GT(snapshotsChecked, 0)
        << "No IR snapshots found. Run: cmake --build build --target update_ir_snapshots";

    // Remove excluded paths
    for (const auto &ex : kExcludedPaths)
    {
        allExpectedPaths.erase(ex);
        allActualPaths.erase(ex);
    }

    // Compute differences
    std::set<std::string> missingPaths; // in expected but not actual → breaking
    std::set<std::string> addedPaths;   // in actual but not expected → new feature

    std::set_difference(allExpectedPaths.begin(), allExpectedPaths.end(),
                        allActualPaths.begin(), allActualPaths.end(),
                        std::inserter(missingPaths, missingPaths.end()));
    std::set_difference(allActualPaths.begin(), allActualPaths.end(),
                        allExpectedPaths.begin(), allExpectedPaths.end(),
                        std::inserter(addedPaths, addedPaths.end()));

    if (!missingPaths.empty())
    {
        std::string msg = "IR_SCHEMA_RESULT:MAJOR\nRemoved/renamed paths:\n";
        for (const auto &p : missingPaths)
            msg += "  - " + p + "\n";
        msg += "Run: cmake --build build --target update_ir_snapshots";
        FAIL() << msg;
        return;
    }

    if (!addedPaths.empty())
    {
        std::string msg = "IR_SCHEMA_RESULT:MINOR\nNew paths:\n";
        for (const auto &p : addedPaths)
            msg += "  - " + p + "\n";
        msg += "Run: cmake --build build --target update_ir_snapshots";
        FAIL() << msg;
        return;
    }

    std::cout << "IR schema check passed (" << snapshotsChecked
              << " snapshots, " << allExpectedPaths.size() << " paths)" << std::endl;
}
