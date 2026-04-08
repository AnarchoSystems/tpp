#pragma once

#include "TppProject.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace tpp
{
    class LspServer
    {
    public:
        LspServer();

        // Process one raw JSON-RPC message (UTF-8 string).
        // Returns the response JSON, or null if no response should be sent
        // (e.g. for notifications).
        nlohmann::json handle(const std::string &rawMessage);

    private:
        // ── State ──────────────────────────────────────────────────────
        bool initialized_ = false;
        bool shutdown_ = false;

        // One project per tpp-config.json found in the workspace roots.
        std::vector<std::unique_ptr<TppProject>> projects_;

        // Dirty buffer for files not belonging to any project (syntax-only features).
        std::unordered_map<std::string, std::string> standaloneBuffer_;

        // ── Helpers ────────────────────────────────────────────────────
        TppProject *projectFor(const std::string &uri);
        void scanWorkspace(const std::vector<std::string> &roots);
        void publishDiagnostics(const TppProject &project, std::vector<nlohmann::json> &notifications);

        // ── Request / notification handlers ────────────────────────────
        nlohmann::json onInitialize(const nlohmann::json &params);
        void           onInitialized(const nlohmann::json &params);
        nlohmann::json onShutdown();

        void onDidOpen(const nlohmann::json &params, std::vector<nlohmann::json> &outNotifs);
        void onDidChange(const nlohmann::json &params, std::vector<nlohmann::json> &outNotifs);
        void onDidClose(const nlohmann::json &params);

        nlohmann::json onSemanticTokensFull(const nlohmann::json &params);
        nlohmann::json onFoldingRange(const nlohmann::json &params);
        nlohmann::json onDefinition(const nlohmann::json &params);
        nlohmann::json onCompletion(const nlohmann::json &params);
        nlohmann::json onRenderPreview(const nlohmann::json &params);
    };
}
