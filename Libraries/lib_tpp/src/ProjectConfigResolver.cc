#include <tpp/ProjectConfigResolver.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;

namespace tpp
{
    static bool matchGlob(const std::string &pattern, const std::string &text)
    {
        size_t patternIndex = 0;
        size_t textIndex = 0;
        size_t starIndex = std::string::npos;
        size_t matchIndex = 0;

        while (textIndex < text.size())
        {
            if (patternIndex < pattern.size() &&
                (pattern[patternIndex] == '?' || pattern[patternIndex] == text[textIndex]))
            {
                ++patternIndex;
                ++textIndex;
            }
            else if (patternIndex < pattern.size() && pattern[patternIndex] == '*')
            {
                starIndex = patternIndex++;
                matchIndex = textIndex;
            }
            else if (starIndex != std::string::npos)
            {
                patternIndex = starIndex + 1;
                textIndex = ++matchIndex;
            }
            else
            {
                return false;
            }
        }

        while (patternIndex < pattern.size() && pattern[patternIndex] == '*')
            ++patternIndex;

        return patternIndex == pattern.size();
    }

    static bool hasAllowedSourceExtension(const std::string &name, bool /*isTypes*/)
    {
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".tpp")
            return true;
        return false;
    }

    static std::vector<fs::path> expandConfigPattern(const fs::path &projectRoot,
                                                     const std::string &pattern,
                                                     bool isTypes,
                                                     bool restrictToTppSources)
    {
        const fs::path patternPath(pattern);
        const std::string filename = patternPath.filename().string();
        const fs::path parentDir = projectRoot / patternPath.parent_path();

        if (filename.find('*') == std::string::npos)
            return {(projectRoot / patternPath).lexically_normal()};

        std::vector<fs::path> results;
        if (!fs::is_directory(parentDir))
            return results;

        for (const auto &entry : fs::directory_iterator(parentDir))
        {
            if (!entry.is_regular_file())
                continue;

            const std::string entryName = entry.path().filename().string();
            if (restrictToTppSources && !hasAllowedSourceExtension(entryName, isTypes))
                continue;
            if (!matchGlob(filename, entryName))
                continue;

            results.push_back(entry.path().lexically_normal());
        }

        std::sort(results.begin(), results.end());
        return results;
    }

    nlohmann::json read_project_config(const fs::path &projectRoot)
    {
        const fs::path configPath = projectRoot / "tpp-config.json";
        std::ifstream configFile(configPath);
        if (!configFile.is_open())
            throw std::runtime_error(configPath.string() + ": tpp-config.json not found");

        try
        {
            return nlohmann::json::parse(std::string((std::istreambuf_iterator<char>(configFile)),
                                                     std::istreambuf_iterator<char>()));
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(configPath.string() + ": invalid JSON: " + e.what());
        }
    }

    ResolvedProjectInputs resolve_project_inputs(const fs::path &projectRoot,
                                                 const nlohmann::json &config)
    {
        ResolvedProjectInputs inputs;
        inputs.projectRoot = projectRoot.lexically_normal();
        inputs.configPath = (inputs.projectRoot / "tpp-config.json").lexically_normal();
        inputs.config = config;

        for (const auto &pattern : config.value("types", nlohmann::json::array()))
        {
            for (const auto &path : expandConfigPattern(inputs.projectRoot,
                                                        pattern.get<std::string>(),
                                                        true,
                                                        true))
            {
                inputs.sourceFiles.push_back({path, true});
            }
        }

        for (const auto &pattern : config.value("templates", nlohmann::json::array()))
        {
            for (const auto &path : expandConfigPattern(inputs.projectRoot,
                                                        pattern.get<std::string>(),
                                                        false,
                                                        true))
            {
                inputs.sourceFiles.push_back({path, false});
            }
        }

        for (const auto &pattern : config.value("replacement-policies", nlohmann::json::array()))
        {
            for (const auto &path : expandConfigPattern(inputs.projectRoot,
                                                        pattern.get<std::string>(),
                                                        false,
                                                        false))
            {
                inputs.policyFiles.push_back(path);
            }
        }

        return inputs;
    }

    ResolvedProjectInputs load_project_inputs(const fs::path &projectRoot)
    {
        return resolve_project_inputs(projectRoot, read_project_config(projectRoot));
    }
}