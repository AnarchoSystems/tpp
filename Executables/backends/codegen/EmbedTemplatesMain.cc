// EmbedTemplatesMain.cc — shared build-time helper for all tpp backends.
//
// Usage: tpp-embed-templates <types-file> <defs-file>
//
// Compiles the type definitions and template definitions, then renders
// the `render_templates` function to produce a C++ header with constexpr
// strings.  Used by tpp2cpp, tpp2java, tpp2swift, make-stat-test, etc.

#include <tpp/Compiler.h>
#include <fstream>
#include <sstream>
#include <iostream>

static std::string readFile(const std::string &fileName)
{
    std::ifstream file(fileName);
    if (!file.is_open())
        throw std::runtime_error("Could not open file: " + fileName);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::string toGoodConstexprString(const std::string &content)
{
    std::string escaped = "\"";
    for (char c : content)
    {
        if (c == '\n')
            escaped += "\\n\"\n\"";
        else
        {
            if (c == '\\' || c == '"')
                escaped += '\\';
            escaped += c;
        }
    }
    return escaped + "\"";
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: tpp-embed-templates <types-file> <defs-file>\n";
        return EXIT_FAILURE;
    }
    const std::string typesPath = argv[1];
    const std::string defsPath  = argv[2];

    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(2);

    auto typesContent = readFile(typesPath);
    if (!typesContent.empty())
        compiler.add_types(typesContent, diagnostics.emplace_back(typesPath).diagnostics);

    auto defsContent = readFile(defsPath);
    compiler.add_templates(defsContent, diagnostics.emplace_back(defsPath).diagnostics);

    tpp::IR output;
    if (!compiler.compile(output))
    {
        for (const auto &msg : diagnostics)
            for (auto &gccDiag : msg.toGCCDiagnostics())
                std::cerr << gccDiag << std::endl;
        return EXIT_FAILURE;
    }

    tpp::FunctionSymbol fn;
    std::string error;
    if (!output.get_function("render_templates", fn, error))
    {
        std::cerr << defsPath << ": error: " << error << std::endl;
        return EXIT_FAILURE;
    }

    std::string rendered;
    std::vector<std::string> input = {
        toGoodConstexprString(readFile(typesPath)),
        toGoodConstexprString(defsContent)};
    if (!fn.render(nlohmann::json(input), rendered, error))
    {
        std::cerr << defsPath << ": error: " << error << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << rendered << std::endl;
    return EXIT_SUCCESS;
}
