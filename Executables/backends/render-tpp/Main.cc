#include <tpp/CompilerOutput.h>
#include <iostream>

// usage: render-tpp <template> <input>
// compiler output is expected to be passed via stdin as json.

using namespace tpp;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: render-tpp <template> <input>" << std::endl;
        return 1;
    }

    std::string templateName = argv[1];
    nlohmann::json inputData = nlohmann::json::parse(argv[2]);

    // read stdin into a string
    std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());

    try
    {
        nlohmann::json json = nlohmann::json::parse(input);
        CompilerOutput compilerOutput = json.get<CompilerOutput>();
        FunctionSymbol functionSymbol;
        std::string error;
        if (!compilerOutput.get_function(templateName, functionSymbol, error))
        {
            std::cerr << "Error: " << error << std::endl;
            return 1;
        }
        std::string output;
        if (!functionSymbol.render(inputData, output, error))
        {
            std::cerr << "Error: " << error << std::endl;
            return 1;
        }
        std::cout << output;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}