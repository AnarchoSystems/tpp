#include "LspClient.h"
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

namespace tpp_test
{

// ── Construction / destruction ────────────────────────────────────────────────

LspClient::LspClient(const std::string &lspExePath)
{
    if (pipe(toChild_) != 0)
        throw std::runtime_error("pipe() failed for toChild");
    if (pipe(fromChild_) != 0)
    {
        close(toChild_[0]); close(toChild_[1]);
        throw std::runtime_error("pipe() failed for fromChild");
    }

    childPid_ = fork();
    if (childPid_ < 0)
    {
        close(toChild_[0]);  close(toChild_[1]);
        close(fromChild_[0]); close(fromChild_[1]);
        throw std::runtime_error("fork() failed");
    }

    if (childPid_ == 0)
    {
        // ── Child: wire up stdin/stdout to the pipes ──────────────────────────
        dup2(toChild_[0],   STDIN_FILENO);
        dup2(fromChild_[1], STDOUT_FILENO);

        // Redirect stderr to /dev/null so tpp-lsp log output doesn't pollute tests
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }

        close(toChild_[0]);   close(toChild_[1]);
        close(fromChild_[0]); close(fromChild_[1]);

        execl(lspExePath.c_str(), lspExePath.c_str(), static_cast<char *>(nullptr));
        _exit(1); // execl only returns on failure
    }

    // ── Parent: close the child-side ends ────────────────────────────────────
    close(toChild_[0]);   toChild_[0]   = -1;
    close(fromChild_[1]); fromChild_[1] = -1;
}

LspClient::~LspClient()
{
    if (childPid_ > 0)
    {
        kill(childPid_, SIGTERM);
        waitpid(childPid_, nullptr, 0);
        childPid_ = -1;
    }
    if (toChild_[1]   >= 0) { close(toChild_[1]);   toChild_[1]   = -1; }
    if (fromChild_[0] >= 0) { close(fromChild_[0]); fromChild_[0] = -1; }
}

// ── Low-level transport ───────────────────────────────────────────────────────

void LspClient::sendMsg(const nlohmann::json &msg)
{
    std::string body   = msg.dump();
    std::string frame  = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    const char *ptr    = frame.c_str();
    size_t remaining   = frame.size();
    while (remaining > 0)
    {
        ssize_t n = write(toChild_[1], ptr, remaining);
        if (n <= 0) throw std::runtime_error("write() to tpp-lsp pipe failed");
        ptr       += n;
        remaining -= static_cast<size_t>(n);
    }
}

nlohmann::json LspClient::readMsg()
{
    // ── Read headers ─────────────────────────────────────────────────────────
    size_t contentLength = 0;
    while (true)
    {
        std::string line;
        while (true)
        {
            char c;
            ssize_t n = read(fromChild_[0], &c, 1);
            if (n <= 0) throw std::runtime_error("read() from tpp-lsp pipe failed");
            if (c == '\n') break;
            if (c != '\r') line += c;
        }
        if (line.empty()) break; // blank line = end of headers
        const std::string prefix = "Content-Length: ";
        if (line.rfind(prefix, 0) == 0)
            contentLength = std::stoull(line.substr(prefix.size()));
    }
    if (contentLength == 0)
        throw std::runtime_error("LSP message with missing or zero Content-Length");

    // ── Read body ─────────────────────────────────────────────────────────────
    std::string body(contentLength, '\0');
    size_t received = 0;
    while (received < contentLength)
    {
        ssize_t n = read(fromChild_[0], &body[received], contentLength - received);
        if (n <= 0) throw std::runtime_error("read() body from tpp-lsp pipe failed");
        received += static_cast<size_t>(n);
    }
    return nlohmann::json::parse(body);
}

bool LspClient::tryReadMsg(nlohmann::json &out, int timeoutMs)
{
    struct pollfd pfd{};
    pfd.fd     = fromChild_[0];
    pfd.events = POLLIN;
    if (poll(&pfd, 1, timeoutMs) <= 0) return false;
    out = readMsg();
    return true;
}

std::vector<nlohmann::json> LspClient::drainNotifications(int timeoutMs)
{
    std::vector<nlohmann::json> result;
    nlohmann::json msg;
    while (tryReadMsg(msg, timeoutMs))
        result.push_back(std::move(msg));
    return result;
}

// ── High-level protocol helpers ───────────────────────────────────────────────

std::vector<nlohmann::json> LspClient::initialize(const std::string &rootUri)
{
    int id = nextId_++;
    sendMsg({
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"method",  "initialize"},
        {"params",  {
            {"rootUri",      rootUri},
            {"capabilities", nlohmann::json::object()}
        }}
    });

    // Wait for the initialize response (skip any unexpected early messages)
    while (true)
    {
        auto msg = readMsg();
        if (msg.contains("id") && !msg["id"].is_null() && msg["id"] == id)
            break;
    }

    // Send the "initialized" notification (no response expected)
    sendMsg({{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", nlohmann::json::object()}});

    // Drain any notifications the server sends immediately (tpp-lsp sends none,
    // but be defensive with a short timeout)
    return drainNotifications(200);
}

std::vector<nlohmann::json> LspClient::didOpen(const std::string &uri,
                                                 const std::string &content)
{
    // textDocument/didOpen is a notification (no id); the server responds with
    // publishDiagnostics notifications written directly to stdout.
    sendMsg({
        {"jsonrpc", "2.0"},
        {"method",  "textDocument/didOpen"},
        {"params",  {
            {"textDocument", {
                {"uri",        uri},
                {"languageId", "tpp"},
                {"version",    1},
                {"text",       content}
            }}
        }}
    });
    return drainNotifications(300);
}

nlohmann::json LspClient::request(const std::string &method,
                                   const nlohmann::json &params)
{
    int id = nextId_++;
    sendMsg({
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"method",  method},
        {"params",  params}
    });

    // Read until we receive the response for our request id
    while (true)
    {
        auto msg = readMsg();
        if (msg.contains("id") && !msg["id"].is_null() && msg["id"] == id)
        {
            if (msg.contains("error"))
                return {{"error", msg["error"].value("message", "unknown error")}};
            return msg.value("result", nlohmann::json{});
        }
        // Ignore interleaved notifications
    }
}

void LspClient::shutdown()
{
    try
    {
        request("shutdown");
        sendMsg({{"jsonrpc", "2.0"}, {"method", "exit"}, {"params", nlohmann::json::object()}});
    }
    catch (...) {} // best-effort
}

} // namespace tpp_test
