#include "TppProject.h"
#include "tpp/ToolingCompile.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace tpp
{

    namespace fs = std::filesystem;

    static bool hasSuffix(const std::string &value, const std::string &suffix)
    {
        return value.size() >= suffix.size() &&
               value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static bool isUnescapedFileUriChar(unsigned char ch)
    {
        return std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~' ||
               ch == '/' || ch == ':';
    }

    static std::string percentEncode(const std::string &input)
    {
        static constexpr char kHex[] = "0123456789ABCDEF";

        std::string encoded;
        encoded.reserve(input.size());
        for (unsigned char ch : input)
        {
            if (isUnescapedFileUriChar(ch))
            {
                encoded.push_back(static_cast<char>(ch));
                continue;
            }

            encoded.push_back('%');
            encoded.push_back(kHex[ch >> 4]);
            encoded.push_back(kHex[ch & 0x0F]);
        }
        return encoded;
    }

    static std::string percentDecode(const std::string &input)
    {
        std::string decoded;
        decoded.reserve(input.size());
        for (size_t index = 0; index < input.size(); ++index)
        {
            if (input[index] == '%' && index + 2 < input.size())
            {
                auto hexValue = [](char ch) -> int
                {
                    if (ch >= '0' && ch <= '9') return ch - '0';
                    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                    return -1;
                };

                int high = hexValue(input[index + 1]);
                int low = hexValue(input[index + 2]);
                if (high >= 0 && low >= 0)
                {
                    decoded.push_back(static_cast<char>((high << 4) | low));
                    index += 2;
                    continue;
                }
            }
            decoded.push_back(input[index]);
        }
        return decoded;
    }

    static fs::path normalizePath(const fs::path &path)
    {
        std::error_code ec;
        fs::path normalized = fs::weakly_canonical(path, ec);
        if (ec)
            normalized = path.lexically_normal();
        return normalized;
    }

    static bool isPathWithinRoot(const fs::path &path, const fs::path &root)
    {
        auto pathIt = path.begin();
        auto rootIt = root.begin();
        for (; rootIt != root.end(); ++rootIt, ++pathIt)
        {
            if (pathIt == path.end() || *pathIt != *rootIt)
                return false;
        }
        return true;
    }

    WorkspaceProject::WorkspaceProject(fs::path configPath)
        : configPath_(std::move(configPath))
        , root_(configPath_.parent_path())
    {
        reloadConfig();
    }

    WorkspaceProject::~WorkspaceProject() = default;

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
        namedTypeLocations_.clear();
        structFieldLocations_.clear();
        output_ = {};

        // Process type files
        for (const auto &path : typeFiles_)
        {
            std::string uri = pathToUri(path);
            project_.add_type_source(getContent(uri), uri);
        }

        // Process template files
        for (const auto &path : templateFiles_)
        {
            std::string uri = pathToUri(path);
            project_.add_template_source(getContent(uri), uri);
        }

        // Load replacement policies
        for (const auto &path : policyFiles_)
        {
            std::string uri = pathToUri(path);
            project_.add_policy_source(getContent(uri), uri);
        }

        CompileOptions options;
        options.includeSourceRanges = true;
        std::vector<DiagnosticLSPMessage> diagnostics;
        ToolingSymbolIndex symbolIndex;
        const bool success = tpp::compile_for_tooling(project_, output_, diagnostics, symbolIndex, options);

        for (const auto &message : diagnostics)
            diagnostics_[message.uri] = message.diagnostics;

        namedTypeLocations_.clear();
        for (const auto &[name, location] : symbolIndex.namedTypeLocations)
            namedTypeLocations_[name] = {location.uri, location.range};

        structFieldLocations_.clear();
        for (const auto &[structName, fieldLocations] : symbolIndex.structFieldLocations)
        {
            auto &projectFieldLocations = structFieldLocations_[structName];
            for (const auto &[fieldName, location] : fieldLocations)
                projectFieldLocations[fieldName] = {location.uri, location.range};
        }

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
        if (!isPathWithinRoot(normalizedPath, normalizedRoot))
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
        std::error_code ec;
        fs::path abs = fs::weakly_canonical(path, ec);
        if (ec) abs = path;
        std::string s = abs.string();
        std::replace(s.begin(), s.end(), '\\', '/');
        return "file://" + percentEncode(s);
    }

    fs::path WorkspaceProject::uriToPath(const std::string &uri) const
    {
        if (uri.rfind("file://", 0) == 0)
            return fs::path(percentDecode(uri.substr(7)));
        return fs::path(percentDecode(uri));
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

    const StructDef *WorkspaceProject::find_struct(std::string_view name) const noexcept
    {
        for (const auto &structDef : output_.structs)
        {
            if (structDef.name == name)
                return &structDef;
        }
        return nullptr;
    }

    const EnumDef *WorkspaceProject::find_enum(std::string_view name) const noexcept
    {
        for (const auto &enumDef : output_.enums)
        {
            if (enumDef.name == name)
                return &enumDef;
        }
        return nullptr;
    }

    const FieldDef *WorkspaceProject::find_struct_field(std::string_view structName,
                                                        std::string_view fieldName) const noexcept
    {
        const auto *structDef = find_struct(structName);
        if (!structDef)
            return nullptr;

        for (const auto &field : structDef->fields)
        {
            if (field.name == fieldName)
                return &field;
        }
        return nullptr;
    }

    std::vector<const FunctionDef *> WorkspaceProject::find_template_overloads(std::string_view name) const
    {
        std::vector<const FunctionDef *> overloads;
        for (const auto &function : output_.functions)
        {
            if (function.name == name)
                overloads.push_back(&function);
        }
        return overloads;
    }

    bool WorkspaceProject::has_named_type(std::string_view name) const noexcept
    {
        return find_struct(name) != nullptr || find_enum(name) != nullptr;
    }

    std::optional<ProjectSourceLocation> WorkspaceProject::find_named_type_location(std::string_view name) const
    {
        auto it = namedTypeLocations_.find(std::string(name));
        if (it == namedTypeLocations_.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<ProjectSourceLocation> WorkspaceProject::find_struct_field_location(std::string_view structName,
                                                                                      std::string_view fieldName) const
    {
        auto structIt = structFieldLocations_.find(std::string(structName));
        if (structIt == structFieldLocations_.end())
            return std::nullopt;

        auto fieldIt = structIt->second.find(std::string(fieldName));
        if (fieldIt == structIt->second.end())
            return std::nullopt;

        return fieldIt->second;
    }

} // namespace tpp
