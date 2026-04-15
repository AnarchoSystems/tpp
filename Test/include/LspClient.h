#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <sys/types.h>

namespace tpp_test
{

// Minimal LSP JSON-RPC 2.0 client that communicates with a tpp-lsp subprocess
// over stdin/stdout pipes.  Content-Length framing is handled internally.
//
// Notifications (e.g. textDocument/publishDiagnostics) that arrive while
// waiting for a request response are buffered and returned by the methods that
// are expected to trigger them (initialize, didOpen).

class LspClient
{
public:
    // Spawn the tpp-lsp executable.  Throws std::runtime_error on failure.
    explicit LspClient(const std::string &lspExePath);

    // Kill the subprocess and close pipes.
    ~LspClient();

    // Send "initialize" + "initialized".  Returns any notifications the server
    // emits immediately after initialization (typically none for tpp-lsp).
    std::vector<nlohmann::json> initialize(const std::string &rootUri);

    // Send textDocument/didOpen.  Returns all publishDiagnostics notifications
    // received in response (the server recompiles and publishes for each URI it
    // owns in the opened file's project).
    std::vector<nlohmann::json> didOpen(const std::string &uri,
                                         const std::string &content);

    // Send workspace/didChangeWatchedFiles and return resulting notifications.
    std::vector<nlohmann::json> didChangeWatchedFiles(const nlohmann::json &changes);

    // Send a generic request and return the response's "result" field.
    // Any notifications that arrive while waiting for the response are
    // silently discarded.  On an LSP error response, returns {"error": msg}.
    nlohmann::json request(const std::string &method,
                           const nlohmann::json &params = nlohmann::json::object());

    // Send "shutdown" + "exit" and wait for the process to exit.
    void shutdown();

private:
    int toChild_[2]   = {-1, -1}; // parent writes [1], child reads [0]
    int fromChild_[2] = {-1, -1}; // child writes [1], parent reads [0]
    pid_t childPid_ = -1;
    int nextId_ = 1;

    void        sendMsg(const nlohmann::json &msg);
    nlohmann::json readMsg();
    bool        tryReadMsg(nlohmann::json &out, int timeoutMs);
    std::vector<nlohmann::json> drainNotifications(int timeoutMs = 300);
};

} // namespace tpp_test
