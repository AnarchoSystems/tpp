#include <tpp/IR.h>
#include <tpp/Rendering.h>
#include <CodegenHelpers.h>
#include "defs.h"
#include <fstream>
#include <iostream>
#include <map>

// usage: tpp2cpp <command> [options]
// commands:
//   types     — produces a header file with type definitions
//   functions — produces a header file rendering the template functions as C++ functions
//   impl      — produces a .cc file with the implementations of the functions declared by 'functions'
//   runtime   — produces a standalone runtime header with helper functions and policy definitions
// options:
//   -ns <name>       wrap generated code in a namespace
//   -i <file>        files to include at the top of the generated code (repeatable)
//   --input <file>   read IR JSON from file instead of stdin
//   --extern-runtime suppress inlining runtime helpers in impl output

enum Mode
{
    None,
    Types,
    Functions,
    Implementation,
    Runtime
};

struct tpp2cpp
{
    std::string namespaceName;
    Mode mode = Mode::None;
    bool externalRuntime = false;
    std::vector<std::string> includes;
    tpp::IR input;
    tpp2cpp(int argc, char *argv[]);
    void run();
};

int main(int argc, char *argv[])
{
    tpp2cpp app(argc, argv);
    app.run();
    return EXIT_SUCCESS;
}

void printUsage()
{
    std::cout << "Usage: tpp2cpp <command> [options]\n"
                 "\n"
                 "Commands:\n"
                 "  types      Produce a header with C++ type definitions\n"
                 "  functions  Produce a header with C++ function declarations\n"
                 "  impl       Produce a .cc with function implementations\n"
                 "  runtime    Produce a standalone runtime helpers header\n"
                 "\n"
                 "Options:\n"
                 "  -ns <name>        Wrap generated code in a namespace\n"
                 "  -i <file>         Include file at top of generated code (repeatable)\n"
                 "  --input <file>    Read IR JSON from file instead of stdin\n"
                 "  --extern-runtime  Suppress inlining runtime helpers\n"
                 "  -h, --help        Print this message\n";
}

tpp2cpp::tpp2cpp(int argc, char *argv[])
{
    static const std::map<std::string, Mode> commands = {
        {"types", Mode::Types},
        {"functions", Mode::Functions},
        {"impl", Mode::Implementation},
        {"runtime", Mode::Runtime}
    };
    auto cli = codegen::parseBackendCommandLine(argc, argv, commands, printUsage, true);
    namespaceName = std::move(cli.namespaceName);
    mode = cli.mode;
    externalRuntime = cli.externalRuntime;
    includes = std::move(cli.includes);
    input = codegen::loadIRInputOrExit(cli.inputFile);
}

static nlohmann::json to_render_cpp_type_input(const tpp::IR &iRep,
                                               const std::vector<std::string> &includes,
                                               const std::string &namespaceName);

static nlohmann::json to_render_cpp_functions_input(const tpp::IR &iRep,
                                                    const std::vector<std::string> &includes,
                                                    const std::string &namespaceName);

static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::vector<std::string> &includes,
    const std::string &namespaceName);

static const tpp::IR &mainIR()
{
    static const tpp::IR result =
        nlohmann::json::parse(defs_ir_json).get<tpp::IR>();
    return result;
}

void tpp2cpp::run()
{
    const auto &iRep = mainIR();

    auto renderFunction = [&](const std::string &fnName, const nlohmann::json &ctx, bool reindentOutput = false) {
        const tpp::FunctionDef *fn = nullptr;
        std::string error, output;
        if (!(tpp::get_function(iRep, fnName, fn, error) && tpp::render_function(iRep, *fn, ctx, output, error)))
        {
            std::cerr << "defs.tpp: error: Failed to render " << fnName << ": " << error << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << (reindentOutput ? codegen::reindent(output) : output) << std::endl;
    };

    switch (mode)
    {
    case Mode::None:
        printUsage();
        exit(EXIT_FAILURE);
        break;
    case Mode::Types:
        renderFunction("render_cpp_types", to_render_cpp_type_input(input, includes, namespaceName));
        break;
    case Mode::Functions:
        renderFunction("render_cpp_functions", to_render_cpp_functions_input(input, includes, namespaceName));
        break;
    case Mode::Implementation:
    {
        auto ctx = buildFunctionsContext(input, "", includes, namespaceName);
        if (externalRuntime)
            ctx.externalRuntime = true;
        renderFunction("render_cpp_native_implementation", nlohmann::json(ctx), true);
        break;
    }
    case Mode::Runtime:
    {
        auto ctx = buildFunctionsContext(input, "", includes, namespaceName);
        ctx.externalRuntime = false;
        renderFunction("render_cpp_runtime", nlohmann::json(ctx), true);
        break;
    }
    }
}

static std::string toCppStringLiteral(const std::string &s)
{
    std::string result = "\"";
    for (char c : s)
    {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else result += c;
    }
    return result + "\"";
}

static nlohmann::json to_render_cpp_type_input(const tpp::IR &iRep,
                                               const std::vector<std::string> &includes,
                                               const std::string &namespaceName)
{
    codegen::CodegenInput ctx = codegen::buildCodegenInput(iRep);
    for (auto &s : ctx.structs)
        s.rawTypedefs = toCppStringLiteral(s.rawTypedefs);
    for (auto &e : ctx.enums)
        e.rawTypedefs = toCppStringLiteral(e.rawTypedefs);
    nlohmann::json ns = namespaceName.empty() ? nlohmann::json() : nlohmann::json(namespaceName);
    return nlohmann::json::array({nlohmann::json(ctx), nlohmann::json(includes), ns});
}

static nlohmann::json buildFunctionsInputJson(const tpp::IR &iRep,
                                          const std::vector<std::string> &includes,
                                          const std::string &namespaceName)
{
    codegen::CodegenInput ctx = codegen::buildCodegenInput(iRep);
    nlohmann::json ns = namespaceName.empty() ? nlohmann::json() : nlohmann::json(namespaceName);
    return nlohmann::json::array({nlohmann::json(ctx), nlohmann::json(includes), ns, ""});
}

static nlohmann::json to_render_cpp_functions_input(const tpp::IR &iRep,
                                                    const std::vector<std::string> &includes,
                                                    const std::string &namespaceName)
{
    return buildFunctionsInputJson(iRep, includes, namespaceName);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR (native rendering)
// ═══════════════════════════════════════════════════════════════════════════════

static codegen::RenderFunctionsInput buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::vector<std::string> &includes,
    const std::string &namespaceName)
{
    return codegen::buildFunctionsContext(
        ir, functionPrefix, namespaceName, codegen::BuildFunctionsConfig::forCpp(includes));
}