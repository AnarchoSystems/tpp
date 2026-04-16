#include "TppProject.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace tpp
{

    namespace fs = std::filesystem;

    static bool hasSuffix(const std::string &value, const std::string &suffix)
    {
        return value.size() >= suffix.size() &&
               value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static fs::path normalizePath(const fs::path &path)
    {
        std::error_code ec;
        fs::path normalized = fs::weakly_canonical(path, ec);
        if (ec)
            normalized = path.lexically_normal();
        return normalized;
    }

    WorkspaceProject::WorkspaceProject(fs::path configPath)
        : configPath_(std::move(configPath))
        , root_(configPath_.parent_path())
    {
        reloadConfig();
    }

    bool WorkspaceProject::reloadConfig()
    {
        typeFiles_.clear();
        templateFiles_.clear();
        policyFiles_.clear();
        uris_.clear();
        typeUris_.clear();
        policyUris_.clear();

        std::ifstream f(configPath_);
        if (!f)
            return false;

        nlohmann::json cfg;
        try { cfg = nlohmann::json::parse(f); }
        catch (...) { return false; }

        auto expand = [&](const std::string &key, std::vector<fs::path> &out)
        {
            if (!cfg.contains(key) || !cfg[key].is_array())
                return;
            for (const auto &item : cfg[key])
            {
                if (!item.is_string()) continue;
                for (auto &p : expandGlob(item.get<std::string>()))
                    out.push_back(p);
            }
        };

        expand("types", typeFiles_);
        expand("templates", templateFiles_);
        expand("replacement-policies", policyFiles_);

        for (const auto &p : typeFiles_)    { uris_.insert(pathToUri(p)); typeUris_.insert(pathToUri(p)); }
        for (const auto &p : templateFiles_) uris_.insert(pathToUri(p));
        for (const auto &p : policyFiles_)  { uris_.insert(pathToUri(p)); policyUris_.insert(pathToUri(p)); }

        return true;
    }

    bool WorkspaceProject::recompile()
    {
        project_.clear();
        diagnostics_.clear();
        analyzed_ = {};
        output_ = {};

        // Process type files
        for (const auto &path : typeFiles_)
        {
            std::string uri = pathToUri(path);
            std::string text;
            auto it = dirtyBuffer_.find(uri);
            if (it != dirtyBuffer_.end())
                text = it->second;
            else
                text = readFile(path);

            project_.add_type_source(std::move(text), uri);
        }

        // Process template files
        for (const auto &path : templateFiles_)
        {
            std::string uri = pathToUri(path);
            std::string text;
            auto it = dirtyBuffer_.find(uri);
            if (it != dirtyBuffer_.end())
                text = it->second;
            else
                text = readFile(path);

            project_.add_template_source(std::move(text), uri);
        }

        // Load replacement policies
        for (const auto &path : policyFiles_)
        {
            std::string uri = pathToUri(path);
            project_.add_policy_source(readFile(path), uri);
        }

        CompileOptions options;
        options.includeSourceRanges = true;
        std::vector<DiagnosticLSPMessage> diagnostics;
        LexedProject lexed;
        ParsedProject parsed;
        const bool success = tpp::lex(project_, lexed, diagnostics, options) &&
                     tpp::parse(lexed, parsed, diagnostics, options) &&
                     tpp::analyze(parsed, analyzed_, diagnostics, options) &&
                     tpp::compile(analyzed_, output_, diagnostics, options);

        for (const auto &message : diagnostics)
            diagnostics_[message.uri] = message.diagnostics;

        return success;
    }

    void WorkspaceProject::setDirty(const std::string &uri, const std::string &text)
    {
        dirtyBuffer_[uri] = text;
    }

    void WorkspaceProject::clearDirty(const std::string &uri)
    {
        dirtyBuffer_.erase(uri);
    }

    bool WorkspaceProject::ownsUri(const std::string &uri) const
    {
        return uris_.count(uri) > 0;
    }

    bool WorkspaceProject::dependsOnWatchedPath(const fs::path &path) const
    {
        fs::path normalizedPath = normalizePath(path);
        if (normalizedPath == normalizePath(configPath_))
            return true;

        auto isKnownDependency = [&](const std::vector<fs::path> &paths)
        {
            return std::any_of(paths.begin(), paths.end(), [&](const fs::path &candidate)
            {
                return normalizePath(candidate) == normalizedPath;
            });
        };

        if (isKnownDependency(typeFiles_) ||
            isKnownDependency(templateFiles_) ||
            isKnownDependency(policyFiles_))
            return true;

        fs::path normalizedRoot = normalizePath(root_);
        auto rootString = normalizedRoot.string();
        auto pathString = normalizedPath.string();
        if (pathString.size() < rootString.size() ||
            pathString.compare(0, rootString.size(), rootString) != 0)
            return false;

        std::string fileName = normalizedPath.filename().string();
        return normalizedPath.extension() == ".tpp" ||
               normalizedPath.extension() == ".json" ||
               hasSuffix(fileName, ".tpp.types");
    }

    std::optional<size_t> WorkspaceProject::typeFileIndex(const std::string &uri) const
    {
        for (size_t index = 0; index < typeFiles_.size(); ++index)
            if (pathToUri(typeFiles_[index]) == uri)
                return index;
        return std::nullopt;
    }

    std::string WorkspaceProject::readFile(const fs::path &path) const
    {
        std::ifstream f(path);
        if (!f) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    std::string WorkspaceProject::pathToUri(const fs::path &path) const
    {
        // Produce a file:// URI from an absolute path.
        // Canonicalise to absolute first.
        std::error_code ec;
        fs::path abs = fs::weakly_canonical(path, ec);
        if (ec) abs = path;
        std::string s = abs.string();
        // Replace backslashes on Windows (no-op on POSIX)
        std::replace(s.begin(), s.end(), '\\', '/');
        return "file://" + s;
    }

    fs::path WorkspaceProject::uriToPath(const std::string &uri) const
    {
        if (uri.rfind("file://", 0) == 0)
            return fs::path(uri.substr(7));
        return fs::path(uri);
    }

    std::vector<fs::path> WorkspaceProject::expandGlob(const std::string &pattern) const
    {
        fs::path patPath(pattern);
        std::string filename = patPath.filename().string();
        fs::path parentDir = root_ / patPath.parent_path();

        if (filename.find('*') == std::string::npos)
        {
            return {root_ / pattern};
        }

        // Simple glob: match filename with '*' wildcard.
        auto matchGlob = [](const std::string &pat, const std::string &text) -> bool
        {
            size_t pi = 0, ti = 0, starPos = std::string::npos, matchPos = 0;
            while (ti < text.size())
            {
                if (pi < pat.size() && (pat[pi] == '?' || pat[pi] == text[ti]))
                {
                    ++pi; ++ti;
                }
                else if (pi < pat.size() && pat[pi] == '*')
                {
                    starPos = pi++;
                    matchPos = ti;
                }
                else if (starPos != std::string::npos)
                {
                    pi = starPos + 1;
                    ti = ++matchPos;
                }
                else return false;
            }
            while (pi < pat.size() && pat[pi] == '*') ++pi;
            return pi == pat.size();
        };

        std::vector<fs::path> results;
        std::error_code ec;
        if (fs::is_directory(parentDir, ec))
        {
            for (const auto &entry : fs::directory_iterator(parentDir, ec))
            {
                if (!entry.is_regular_file()) continue;
                auto name = entry.path().filename().string();
                if (matchGlob(filename, name))
                    results.push_back(entry.path());
            }
            std::sort(results.begin(), results.end());
        }
        return results;
    }

    std::string WorkspaceProject::getContent(const std::string &uri) const
    {
        auto it = dirtyBuffer_.find(uri);
        if (it != dirtyBuffer_.end())
            return it->second;
        return readFile(uriToPath(uri));
    }

} // namespace tpp
