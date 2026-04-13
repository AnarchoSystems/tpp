#include "TestUtils.h"
#include <tpp/IR.h>
#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <fstream>

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
    "versionMajor", "versionMinor", "versionPatch"
};

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

        // Load snapshot
        std::ifstream snapFile(snapshotPath);
        ASSERT_TRUE(snapFile.good()) << "Cannot read " << snapshotPath;
        nlohmann::json expectedIR;
        snapFile >> expectedIR;

        // Compile in-process
        auto loaded = tc.extract();
        tpp::Compiler compiler;
        std::vector<tpp::DiagnosticLSPMessage> fileDiags;
        for (const auto &src : loaded.sources)
        {
            tpp::DiagnosticLSPMessage dm(src.url);
            if (src.isTypes)
                compiler.add_types(src.content, dm.diagnostics);
            else
                compiler.add_templates(src.content, dm.diagnostics);
            fileDiags.push_back(std::move(dm));
        }
        for (const auto &pol : loaded.policies)
        {
            std::string err;
            compiler.add_policy(pol, err);
        }

        tpp::IR output;
        bool ok = compiler.compile(output);
        ASSERT_TRUE(ok) << "Failed to compile test case: " << tc.name;

        nlohmann::json actualIR = output;

        // Collect key paths
        std::set<std::string> expectedPaths, actualPaths;
        collectPaths(expectedIR, "", expectedPaths);
        collectPaths(actualIR, "", actualPaths);

        allExpectedPaths.insert(expectedPaths.begin(), expectedPaths.end());
        allActualPaths.insert(actualPaths.begin(), actualPaths.end());
        ++snapshotsChecked;
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
