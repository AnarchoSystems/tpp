#pragma once

#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <tpp/Diagnostic.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>

namespace tpp
{
    // One TppProject corresponds to one tpp-config.json on disk.
    // It owns the Compiler, the IR, and an in-memory dirty buffer
    // for open files that have unsaved changes.
    class TppProject
    {
    public:
        explicit TppProject(std::filesystem::path configPath);

        // Re-read the config file and re-resolve file lists.
        // Returns false if the config file can't be parsed.
        bool reloadConfig();

        // Re-compile using on-disk content plus any dirty-buffer overrides.
        // Returns false if compilation produced hard errors.
        bool recompile();

        // Mark a file as dirty (in-memory edit). Pass empty text to clear.
        void setDirty(const std::string &uri, const std::string &text);
        void clearDirty(const std::string &uri);

        // Returns true if the given file URI belongs to this project.
        bool ownsUri(const std::string &uri) const;

        // Diagnostics produced by the last recompile(), keyed by URI.
        const std::map<std::string, std::vector<Diagnostic>> &diagnostics() const { return diagnostics_; }

        const IR &output() const { return output_; }
        const std::filesystem::path &configPath() const { return configPath_; }
        const std::filesystem::path &root() const { return root_; }

        // All URIs (types + templates) belonging to this project.
        const std::set<std::string> &uris() const { return uris_; }

        // Returns true if the given URI is a type definition file (not a template).
        bool isTypeUri(const std::string &uri) const { return typeUris_.count(uri) > 0; }

        // Returns the current content for a URI (dirty buffer first, else disk).
        std::string getContent(const std::string &uri) const;

    private:
        std::filesystem::path configPath_;
        std::filesystem::path root_; // directory containing the config

        std::vector<std::filesystem::path> typeFiles_;
        std::vector<std::filesystem::path> templateFiles_;
        std::vector<std::filesystem::path> policyFiles_;
        std::set<std::string> uris_;
        std::set<std::string> typeUris_;

        std::map<std::string, std::string> dirtyBuffer_; // URI -> text

        Compiler compiler_;
        IR output_;
        std::map<std::string, std::vector<Diagnostic>> diagnostics_;

        std::string readFile(const std::filesystem::path &path) const;
        std::string pathToUri(const std::filesystem::path &path) const;
        std::filesystem::path uriToPath(const std::string &uri) const;
        std::vector<std::filesystem::path> expandGlob(const std::string &pattern) const;
    };
}
