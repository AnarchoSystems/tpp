#include <tpp/Compiler.h>
#include <fstream>
#include <sstream>
#include <iostream>

std::string readFile(const std::string &fileName)
{
    std::ifstream file(fileName);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open file: " + fileName);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string toGoodConstexprString(const std::string &content)
{
    std::string escapedContent = "\"";
    for (char c : content)
    {
        if (c == '\n')
        {
            escapedContent += "\\n\"\n\""; // preserve runtime newline; split source line for readability
        }
        else
        {
            if (c == '\\' || c == '"')
            {
                escapedContent += '\\';
            }
            escapedContent += c;
        }
    }
    return escapedContent + "\"";
}

int main()
{
    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(2);
    auto typesContent = readFile(TYPES_TPP);
    if (!typesContent.empty())
        compiler.add_types(typesContent, diagnostics.emplace_back(TYPES_TPP).diagnostics);
    auto fileContent = readFile(DEFS_TPP);
    compiler.add_templates(fileContent, diagnostics.emplace_back(DEFS_TPP).diagnostics);
    tpp::IR output;
    if (!compiler.compile(output))
    {
        for (const auto &msg : diagnostics)
        {
            for (auto &gccDiagnostic : msg.toGCCDiagnostics())
            {
                std::cerr << gccDiagnostic << std::endl;
            }
        }
        return EXIT_FAILURE;
    }
    tpp::FunctionSymbol functionSymbol;
    std::string error;
    if (!output.get_function("render_templates", functionSymbol, error))
    {
        std::cerr << DEFS_TPP ": error: " << error << std::endl;
        return EXIT_FAILURE;
    }
    std::string renderedCode;
    std::vector<std::string> input = {toGoodConstexprString(readFile(TYPES_TPP)), toGoodConstexprString(fileContent)};
    if (!functionSymbol.render(nlohmann::json(input), renderedCode, error))
    {
        std::cerr << DEFS_TPP ": error: " << error << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << renderedCode << std::endl;
    return EXIT_SUCCESS;
}