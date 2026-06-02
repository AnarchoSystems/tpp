#include <gtest/gtest.h>

#include "TppProject.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

class TempProjectDir
{
public:
    TempProjectDir()
    {
        const auto uniqueId = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = fs::temp_directory_path() / ("tpp-workspace-project-" + uniqueId);
        fs::create_directories(path_);
    }

    ~TempProjectDir()
    {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    const fs::path &path() const
    {
        return path_;
    }

private:
    fs::path path_;
};

void writeFile(const fs::path &path, const std::string &content)
{
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open()) << "Failed to open " << path;
    out << content;
}

} // namespace

TEST(WorkspaceProjectTest, DependsOnWatchedPathRejectsSiblingWithMatchingPrefix)
{
    TempProjectDir tempDir;
    const fs::path projectRoot = tempDir.path() / "project";
    const fs::path siblingRoot = tempDir.path() / "project-other";

    fs::create_directories(projectRoot);
    fs::create_directories(siblingRoot);
    writeFile(projectRoot / "tpp-config.json", "{\n  \"types\": [],\n  \"templates\": []\n}\n");

    tpp::WorkspaceProject project(projectRoot / "tpp-config.json");

    EXPECT_TRUE(project.dependsOnWatchedPath(projectRoot / "nested" / "file.tpp"));
    EXPECT_FALSE(project.dependsOnWatchedPath(siblingRoot / "nested" / "file.tpp"));
    EXPECT_FALSE(project.dependsOnWatchedPath(siblingRoot / "nested" / "file.txt"));
    EXPECT_FALSE(project.dependsOnWatchedPath(siblingRoot / "nested" / "file.json"));
}