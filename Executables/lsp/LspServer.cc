#include "LspServer.h"
#include "LspDefinitions.h"
#include "SemanticTokens.h"
#include "FoldingRanges.h"
#include "JumpToDefinition.h"
#include "Autocomplete.h"
#include "Preview.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <algorithm>

namespace tpp
{

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string uriDecode(const std::string &s);
static std::string uriToFsPath(const std::string &uri);

static nlohmann::json makeResponse(const nlohmann::json &id, const nlohmann::json &result)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

static nlohmann::json makeError(const nlohmann::json &id, int code, const std::string &msg)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", msg}}}};
}

static nlohmann::json makeNotification(const std::string &method, const nlohmann::json &params)
{
    return {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
}

// ── Construction ─────────────────────────────────────────────────────────────

LspServer::LspServer() = default;

// ── Main dispatch ─────────────────────────────────────────────────────────────

nlohmann::json LspServer::handle(const std::string &rawMessage)
{
    nlohmann::json msg;
    try { msg = nlohmann::json::parse(rawMessage); }
    catch (...) { return makeError(nullptr, -32700, "Parse error"); }

    const auto &method = msg.value("method", std::string{});
    const auto &id     = msg.contains("id") ? msg["id"] : nlohmann::json(nullptr);
    const auto &params = msg.contains("params") ? msg["params"] : nlohmann::json::object();

    // Notifications side-channel: some handlers produce extra notifications
    // (e.g. publishDiagnostics). We batch them after the main response.
    // For simplicity, we return only one JSON object per call; extra notifications
    // are written directly to stdout via the static helper below.
    // (The transport in Main.cc only writes the return value. We handle multi-message
    // output by buffering and writing in sequence here.)

    if (method == "initialize")
        return makeResponse(id, onInitialize(params));

    if (method == "initialized")
    {
        std::vector<nlohmann::json> notifs;
        onInitialized(params, notifs);
        for (const auto &n : notifs)
        {
            std::string body = n.dump();
            std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
            std::cout.flush();
        }
        return nullptr; // notification, no response
    }

    if (method == "shutdown")
        return makeResponse(id, onShutdown());

    if (method == "exit")
    {
        std::exit(shutdown_ ? 0 : 1);
    }

    if (!initialized_)
        return makeError(id, -32002, "Server not initialized");

    if (method == "workspace/didChangeWatchedFiles")
    {
        std::vector<nlohmann::json> notifs;
        onDidChangeWatchedFiles(params, notifs);
        for (const auto &n : notifs)
        {
            std::string body = n.dump();
            std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
            std::cout.flush();
        }
        return nullptr;
    }

    if (method == "textDocument/didOpen")
    {
        std::vector<nlohmann::json> notifs;
        onDidOpen(params, notifs);
        // Write diagnostics notifications directly to stdout
        for (const auto &n : notifs)
        {
            std::string body = n.dump();
            std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
            std::cout.flush();
        }
        return nullptr;
    }

    if (method == "textDocument/didChange")
    {
        std::vector<nlohmann::json> notifs;
        onDidChange(params, notifs);
        for (const auto &n : notifs)
        {
            std::string body = n.dump();
            std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
            std::cout.flush();
        }
        return nullptr;
    }

    if (method == "textDocument/didClose")
    {
        onDidClose(params);
        return nullptr;
    }

    if (method == "textDocument/semanticTokens/full")
        return makeResponse(id, onSemanticTokensFull(params));

    if (method == "textDocument/foldingRange")
        return makeResponse(id, onFoldingRange(params));

    if (method == "textDocument/definition")
        return makeResponse(id, onDefinition(params));

    if (method == "textDocument/completion")
        return makeResponse(id, onCompletion(params));

    if (method == "tpp/renderPreview")
        return makeResponse(id, onRenderPreview(params));

    // Unknown request — return null result (not an error, just unimplemented)
    if (!id.is_null())
        return makeResponse(id, nullptr);
    return nullptr;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

nlohmann::json LspServer::onInitialize(const nlohmann::json &params)
{
    // Collect workspace roots
    std::vector<std::string> roots;
    if (params.contains("rootUri") && params["rootUri"].is_string())
        roots.push_back(params["rootUri"].get<std::string>());
    if (params.contains("workspaceFolders") && params["workspaceFolders"].is_array())
        for (const auto &f : params["workspaceFolders"])
            if (f.contains("uri") && f["uri"].is_string())
                roots.push_back(f["uri"].get<std::string>());

    scanWorkspace(roots);
    initialized_ = true;

    return {
        {"capabilities", {
            {"textDocumentSync", 1}, // Full sync
            {"semanticTokensProvider", {
                {"legend", {
                    {"tokenTypes", lsp::tokenTypeLegend()},
                    {"tokenModifiers", nlohmann::json::array()}
                }},
                {"full", true}
            }},
            {"foldingRangeProvider", true},
            {"definitionProvider", true},
            {"completionProvider", {
                {"triggerCharacters", nlohmann::json::array({"@", "."})}
            }}
        }},
        {"serverInfo", {{"name", "tpp-lsp"}, {"version", "0.1.0"}}}
    };
}

void LspServer::onInitialized(const nlohmann::json &, std::vector<nlohmann::json> &outNotifs)
{
    // Register a file watcher so we're notified when tpp-config.json is created
    // or modified anywhere in the workspace, allowing us to pick up new projects
    // without requiring a window reload.
    nlohmann::json reg = {
        {"id", "tpp-lsp-watch-configs"},
        {"method", "client/registerCapability"},
        {"jsonrpc", "2.0"},
        {"params", {
            {"registrations", nlohmann::json::array({
                {
                    {"id", "tpp-config-watcher"},
                    {"method", "workspace/didChangeWatchedFiles"},
                    {"registerOptions", {
                        {"watchers", nlohmann::json::array({
                            {{"globPattern", "**/tpp-config.json"}}
                        })}
                    }}
                }
            })}
        }}
    };
    outNotifs.push_back(std::move(reg));
}

nlohmann::json LspServer::onShutdown()
{
    shutdown_ = true;
    return nullptr;
}

// ── File watch ────────────────────────────────────────────────────────────────

void LspServer::onDidChangeWatchedFiles(const nlohmann::json &params,
                                         std::vector<nlohmann::json> &outNotifs)
{
    // Called when tpp-config.json is created or modified in the workspace.
    // Create a new project for configs we haven't seen, or reload existing ones.
    const auto &changes = params.value("changes", nlohmann::json::array());
    for (const auto &change : changes)
    {
        std::string uri = uriDecode(change.value("uri", ""));
        if (uri.empty()) continue;
        fs::path cfgPath = uriToFsPath(uri);
        if (cfgPath.filename() != "tpp-config.json") continue;

        // Check if we already have a project for this config.
        TppProject *existing = nullptr;
        for (auto &p : projects_)
            if (fs::weakly_canonical(p->configPath()) == fs::weakly_canonical(cfgPath))
                { existing = p.get(); break; }

        if (existing)
        {
            existing->reloadConfig();
            existing->recompile();
            publishDiagnostics(*existing, outNotifs);
        }
        else
        {
            std::error_code ec;
            if (!fs::exists(cfgPath, ec) || ec) continue;
            auto project = std::make_unique<TppProject>(cfgPath);

            // Migrate any standalone-buffer files that belong to this new project.
            for (auto it = standaloneBuffer_.begin(); it != standaloneBuffer_.end(); )
            {
                if (project->ownsUri(it->first))
                {
                    project->setDirty(it->first, it->second);
                    it = standaloneBuffer_.erase(it);
                }
                else ++it;
            }

            project->recompile();
            publishDiagnostics(*project, outNotifs);
            projects_.push_back(std::move(project));
        }
    }
}

// ── Workspace scan ────────────────────────────────────────────────────────────

// Percent-decode a URI path (e.g. C%2B%2B → C++)
static std::string uriDecode(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '%' && i + 2 < s.size())
        {
            auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hexVal(s[i+1]), lo = hexVal(s[i+2]);
            if (hi >= 0 && lo >= 0) { out += (char)((hi << 4) | lo); i += 2; continue; }
        }
        out += s[i];
    }
    return out;
}

static std::string uriToFsPath(const std::string &uri)
{
    std::string decoded = uriDecode(uri);
    if (decoded.rfind("file://", 0) == 0)
        return decoded.substr(7);
    return decoded;
}

static void findConfigsRecursive(const fs::path &dir,
                                 std::vector<fs::path> &out,
                                 int depth = 0)
{
    if (depth > 8) return; // guard against deep trees
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(dir, ec))
    {
        if (!ec && entry.is_directory(ec) && !ec)
        {
            // Skip hidden dirs, node_modules, build
            auto name = entry.path().filename().string();
            if (!name.empty() && name[0] == '.') continue;
            if (name == "node_modules" || name == "build") continue;
            findConfigsRecursive(entry.path(), out, depth + 1);
        }
        else if (!ec && entry.is_regular_file(ec) && !ec)
        {
            if (entry.path().filename() == "tpp-config.json")
                out.push_back(entry.path());
        }
    }
}

void LspServer::scanWorkspace(const std::vector<std::string> &roots)
{
    projects_.clear();
    for (const auto &rootUri : roots)
    {
        fs::path rootPath = uriToFsPath(rootUri);
        std::vector<fs::path> configs;
        findConfigsRecursive(rootPath, configs);
        for (const auto &cfg : configs)
        {
            auto project = std::make_unique<TppProject>(cfg);
            project->recompile();
            projects_.push_back(std::move(project));
        }
    }
}

// ── Project lookup ────────────────────────────────────────────────────────────

TppProject *LspServer::projectFor(const std::string &uri)
{
    // Normalize: VS Code percent-encodes paths (e.g. C%2B%2B), but stored URIs
    // are built from filesystem paths without encoding.
    std::string normUri = uriDecode(uri);

    // O(P) scan – number of projects is small
    for (auto &p : projects_)
        if (p->ownsUri(normUri))
            return p.get();

    // Fallback: walk up directories from the file path to find the nearest
    // config that has been loaded.
    fs::path filePath = uriToFsPath(normUri);
    fs::path dir = filePath.parent_path();
    while (!dir.empty() && dir != dir.parent_path())
    {
        for (auto &p : projects_)
            if (p->root() == dir)
                return p.get();
        dir = dir.parent_path();
    }
    return nullptr;
}

// ── Diagnostics publish ───────────────────────────────────────────────────────

void LspServer::publishDiagnostics(const TppProject &project,
                                   std::vector<nlohmann::json> &notifications)
{
    // Publish for every URI in the project (including those with no diagnostics,
    // so previous errors are cleared).
    for (const auto &uri : project.uris())
    {
        nlohmann::json diagArray = nlohmann::json::array();
        auto it = project.diagnostics().find(uri);
        if (it != project.diagnostics().end())
            for (const auto &d : it->second)
                diagArray.push_back(d);

        notifications.push_back(makeNotification("textDocument/publishDiagnostics",
                                                 {{"uri", uri}, {"diagnostics", diagArray}}));
    }
}

// ── Document sync ─────────────────────────────────────────────────────────────

void LspServer::onDidOpen(const nlohmann::json &params, std::vector<nlohmann::json> &outNotifs)
{
    const auto &doc = params.at("textDocument");
    std::string uri  = uriDecode(doc.at("uri").get<std::string>());
    std::string text = doc.at("text").get<std::string>();

    TppProject *proj = projectFor(uri);
    if (!proj)
    {
        // File not part of any known project — try creating a project for it if
        // a tpp-config.json exists in an ancestor directory.
        fs::path filePath = uriToFsPath(uri);
        fs::path dir = filePath.parent_path();
        while (!dir.empty() && dir != dir.parent_path())
        {
            fs::path cfg = dir / "tpp-config.json";
            std::error_code ec;
            if (fs::exists(cfg, ec) && !ec)
            {
                auto project = std::make_unique<TppProject>(cfg);
                project->setDirty(uri, text);
                project->recompile();
                publishDiagnostics(*project, outNotifs);
                projects_.push_back(std::move(project));
                return;
            }
            dir = dir.parent_path();
        }
        // No config found — store in standalone buffer for syntax-only features
        standaloneBuffer_[uri] = text;
        // Emit a warning diagnostic so the user knows why LSP features are missing
        auto ext = filePath.extension().string();
        bool isTppFile = (ext == ".tpp") ||
                         (filePath.filename().string().find(".tpp.types") != std::string::npos);
        if (isTppFile)
        {
            nlohmann::json diag = {
                {"range", {{"start", {{"line", 0}, {"character", 0}}},
                           {"end",   {{"line", 0}, {"character", 0}}}}},
                {"severity", 2}, // Warning
                {"source", "tpp"},
                {"message", "No tpp-config.json found in any ancestor directory. "
                             "Jump-to-definition, completions, and diagnostics require a tpp-config.json "
                             "with \"types\" and \"templates\" glob arrays."}
            };
            outNotifs.push_back(makeNotification("textDocument/publishDiagnostics",
                {{"uri", uri}, {"diagnostics", nlohmann::json::array({diag})}}));
        }
        return;
    }

    proj->setDirty(uri, text);
    proj->recompile();
    publishDiagnostics(*proj, outNotifs);
}

void LspServer::onDidChange(const nlohmann::json &params, std::vector<nlohmann::json> &outNotifs)
{
    const auto &doc = params.at("textDocument");
    std::string uri  = uriDecode(doc.at("uri").get<std::string>());

    // Full sync: take the last content change.
    const auto &changes = params.at("contentChanges");
    if (changes.empty()) return;
    std::string text = changes.back().at("text").get<std::string>();

    TppProject *proj = projectFor(uri);
    if (!proj)
    {
        // Mirror onDidOpen: if a tpp-config.json now exists in an ancestor dir
        // (e.g. the user created the config after opening the file), pick it up.
        fs::path filePath = uriToFsPath(uri);
        fs::path dir = filePath.parent_path();
        while (!dir.empty() && dir != dir.parent_path())
        {
            fs::path cfg = dir / "tpp-config.json";
            std::error_code ec;
            if (fs::exists(cfg, ec) && !ec)
            {
                auto project = std::make_unique<TppProject>(cfg);
                project->setDirty(uri, text);
                project->recompile();
                publishDiagnostics(*project, outNotifs);
                projects_.push_back(std::move(project));
                return;
            }
            dir = dir.parent_path();
        }
        standaloneBuffer_[uri] = text;
        return;
    }

    proj->setDirty(uri, text);
    proj->recompile();
    publishDiagnostics(*proj, outNotifs);
}

void LspServer::onDidClose(const nlohmann::json &params)
{
    std::string uri = uriDecode(params.at("textDocument").at("uri").get<std::string>());
    TppProject *proj = projectFor(uri);
    if (proj)
        proj->clearDirty(uri);
    else
        standaloneBuffer_.erase(uri);
}

// ── Feature handlers (delegate to sub-modules) ───────────────────────────────

nlohmann::json LspServer::onSemanticTokensFull(const nlohmann::json &params)
{
    std::string uri = uriDecode(params.at("textDocument").at("uri").get<std::string>());
    TppProject *proj = projectFor(uri);
    if (proj) return computeSemanticTokens(uri, *proj);

    // No project — use standalone buffer or read from disk
    auto it = standaloneBuffer_.find(uri);
    if (it != standaloneBuffer_.end())
        return computeSemanticTokensFromText(uri, it->second);
    std::string fsPath = uriToFsPath(uri);
    std::ifstream f(fsPath);
    if (f) {
        std::ostringstream ss; ss << f.rdbuf();
        return computeSemanticTokensFromText(uri, ss.str());
    }
    return {{"data", nlohmann::json::array()}};
}

nlohmann::json LspServer::onFoldingRange(const nlohmann::json &params)
{
    std::string uri = uriDecode(params.at("textDocument").at("uri").get<std::string>());
    TppProject *proj = projectFor(uri);
    if (!proj) return nlohmann::json::array();
    return computeFoldingRanges(uri, *proj);
}

nlohmann::json LspServer::onDefinition(const nlohmann::json &params)
{
    std::string uri = uriDecode(params.at("textDocument").at("uri").get<std::string>());
    TppProject *proj = projectFor(uri);
    if (!proj) return nullptr;
    int line = params.at("position").at("line").get<int>();
    int character = params.at("position").at("character").get<int>();
    return jumpToDefinition(uri, line, character, *proj);
}

nlohmann::json LspServer::onCompletion(const nlohmann::json &params)
{
    std::string uri = uriDecode(params.at("textDocument").at("uri").get<std::string>());
    TppProject *proj = projectFor(uri);
    if (!proj) return nlohmann::json::array();
    int line = params.at("position").at("line").get<int>();
    int character = params.at("position").at("character").get<int>();
    return computeCompletions(uri, line, character, *proj);
}

nlohmann::json LspServer::onRenderPreview(const nlohmann::json &params)
{
    // Find the project that owns this config path.
    TppProject *proj = nullptr;
    if (params.contains("configPath") && params["configPath"].is_string())
    {
        std::string configUri = params["configPath"].get<std::string>();
        // configPath may be a file URI or a filesystem path; try both.
        for (auto &p : projects_)
        {
            std::string cu = "file://" + p->configPath().string();
            if (cu == configUri || p->configPath().string() == configUri)
            {
                proj = p.get();
                break;
            }
        }
    }
    return renderPreview(params, proj);
}

} // namespace tpp
