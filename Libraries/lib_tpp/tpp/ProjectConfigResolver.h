#pragma once

#include <filesystem>
#include <vector>

#include <nlohmann/json.hpp>

namespace tpp
{
    struct ResolvedProjectSource
    {
        std::filesystem::path path;
        bool isTypes = false;
    };

    struct ResolvedProjectInputs
    {
        std::filesystem::path projectRoot;
        std::filesystem::path configPath;
        nlohmann::json config;
        std::vector<ResolvedProjectSource> sourceFiles;
        std::vector<std::filesystem::path> policyFiles;
    };

    nlohmann::json read_project_config(const std::filesystem::path &projectRoot);
    ResolvedProjectInputs resolve_project_inputs(const std::filesystem::path &projectRoot,
                                                 const nlohmann::json &config);
    ResolvedProjectInputs load_project_inputs(const std::filesystem::path &projectRoot);
}