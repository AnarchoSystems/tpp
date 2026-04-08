#include "LspServer.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <sstream>

// Standard LSP JSON-RPC 2.0 transport over stdin/stdout.
// Each message is preceded by "Content-Length: N\r\n\r\n" headers.

static bool readMessage(std::string &out)
{
    size_t contentLength = 0;
    bool gotLength = false;

    // Read headers until blank line
    while (true)
    {
        std::string line;
        if (!std::getline(std::cin, line))
            return false;
        // Strip trailing CR
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            break; // blank line separates headers from body
        const std::string prefix = "Content-Length: ";
        if (line.rfind(prefix, 0) == 0)
        {
            contentLength = std::stoull(line.substr(prefix.size()));
            gotLength = true;
        }
    }

    if (!gotLength || contentLength == 0)
        return false;

    out.resize(contentLength);
    if (!std::cin.read(&out[0], (std::streamsize)contentLength))
        return false;
    return true;
}

static void writeMessage(const std::string &body)
{
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

int main()
{
    // Disable sync for speed; we own stdin/stdout completely.
    std::ios_base::sync_with_stdio(false);

    tpp::LspServer server;

    std::string raw;
    while (readMessage(raw))
    {
        nlohmann::json response = server.handle(raw);
        if (!response.is_null())
        {
            writeMessage(response.dump());
        }
    }
}
