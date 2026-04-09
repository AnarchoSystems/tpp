#include "TppProject.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace tpp
{

    namespace fs = std::filesystem;

    TppProject::TppProject(fs::path configPath)
        : configPath_(std::move(configPath))
        , root_(configPath_.parent_path())
    {
        reloadConfig();
    }

    bool TppProject::reloadConfig()
    {
        typeFiles_.clear();
        templateFiles_.clear();
        policyFiles_.clear();
        uris_.clear();
        typeUris_.clear();

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

        return true;
    }

    bool TppProject::recompile()
    {
        compiler_.clear_types();
        compiler_.clear_templates();
        compiler_.clear_policies();
        diagnostics_.clear();

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

            auto &diags = diagnostics_[uri];
            compiler_.add_types(text, diags);
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

            auto &diags = diagnostics_[uri];
            compiler_.add_templates(text, diags);
        }

        // Load replacement policies
        for (const auto &path : policyFiles_)
        {
            std::string text = readFile(path);
            if (text.empty()) continue;
            try
            {
                nlohmann::json pj = nlohmann::json::parse(text);
                std::string err;
                compiler_.add_policy(pj, err); // silently ignore invalid policies
            }
            catch (...) {}
        }

        CompilerOutput newOutput;
        bool ok = compiler_.compile(newOutput);
        output_ = std::move(newOutput);
        return ok;
    }

    void TppProject::setDirty(const std::string &uri, const std::string &text)
    {
        dirtyBuffer_[uri] = text;
    }

    void TppProject::clearDirty(const std::string &uri)
    {
        dirtyBuffer_.erase(uri);
    }

    bool TppProject::ownsUri(const std::string &uri) const
    {
        return uris_.count(uri) > 0;
    }

    std::string TppProject::readFile(const fs::path &path) const
    {
        std::ifstream f(path);
        if (!f) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    std::string TppProject::pathToUri(const fs::path &path) const
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

    fs::path TppProject::uriToPath(const std::string &uri) const
    {
        if (uri.rfind("file://", 0) == 0)
            return fs::path(uri.substr(7));
        return fs::path(uri);
    }

    std::vector<fs::path> TppProject::expandGlob(const std::string &pattern) const
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

    std::string TppProject::getContent(const std::string &uri) const
    {
        auto it = dirtyBuffer_.find(uri);
        if (it != dirtyBuffer_.end())
            return it->second;
        return readFile(uriToPath(uri));
    }

} // namespace tpp
