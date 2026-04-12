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
    std::string inputFileName;

    // First pass: find the subcommand (first non-option argument)
    int cmdIndex = 0;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            exit(EXIT_SUCCESS);
        }
        if (arg[0] != '-')
        {
            cmdIndex = i;
            break;
        }
        // Skip value for options that take one
        if (arg == "-ns" || arg == "-i" || arg == "--input")
            ++i;
    }

    if (cmdIndex == 0)
    {
        printUsage();
        exit(EXIT_FAILURE);
    }

    std::string cmd = argv[cmdIndex];
    static const std::map<std::string, Mode> commands = {
        {"types", Mode::Types},
        {"functions", Mode::Functions},
        {"impl", Mode::Implementation},
        {"runtime", Mode::Runtime}
    };
    auto it = commands.find(cmd);
    if (it == commands.end())
    {
        std::cerr << "Unknown command: " << cmd << "\nUse -h for usage info.\n";
        exit(EXIT_FAILURE);
    }
    mode = it->second;

    // Second pass: parse options (skip the subcommand)
    for (int i = 1; i < argc; ++i)
    {
        if (i == cmdIndex) continue;
        std::string arg = argv[i];
        if (arg == "-ns")
        {
            if (i + 1 >= argc) { std::cerr << "-ns requires a name argument\n"; exit(EXIT_FAILURE); }
            namespaceName = argv[++i];
        }
        else if (arg == "-i")
        {
            if (i + 1 >= argc) { std::cerr << "-i requires a file argument\n"; exit(EXIT_FAILURE); }
            includes.push_back(argv[++i]);
        }
        else if (arg == "--input")
        {
            if (i + 1 >= argc) { std::cerr << "--input requires a file argument\n"; exit(EXIT_FAILURE); }
            inputFileName = argv[++i];
        }
        else if (arg == "--extern-runtime")
        {
            externalRuntime = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\nUse -h for usage info.\n";
            exit(EXIT_FAILURE);
        }
    }

    // Read input from file or stdin
    std::string inputJson;
    if (!inputFileName.empty())
    {
        std::ifstream inputFile(inputFileName);
        if (!inputFile.is_open())
        {
            std::cerr << "Could not open input file: " << inputFileName << std::endl;
            exit(EXIT_FAILURE);
        }
        inputJson.assign((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
    }
    else
    {
        inputJson.assign((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    }
    try
    {
        input = nlohmann::json::parse(inputJson).get<tpp::IR>();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse input JSON: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
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

// Extract a single type definition (struct or enum) from the raw typedefs string.
// Includes any preceding doc comments (lines starting with //).
static std::string extractTypeDefinition(const std::string &raw,
                                         const std::string &keyword,
                                         const std::string &name)
{
    std::string marker = keyword + " " + name;
    auto pos = raw.find(marker);
    if (pos == std::string::npos) return "";

    // Walk backward to include preceding doc-comment lines and blank lines
    auto start = pos;
    while (start > 0)
    {
        // find beginning of previous line
        auto prev_end = start - 1; // points at '\n' before current line
        if (prev_end == std::string::npos || raw[prev_end] != '\n') break;
        auto prev_start = raw.rfind('\n', prev_end - 1);
        prev_start = (prev_start == std::string::npos) ? 0 : prev_start + 1;
        auto line = raw.substr(prev_start, prev_end - prev_start);
        // trim leading whitespace
        auto first_non_ws = line.find_first_not_of(" \t");
        if (first_non_ws == std::string::npos)
        {
            // blank line — include it and keep going
            start = prev_start;
            continue;
        }
        if (line.substr(first_non_ws, 2) == "//")
        {
            start = prev_start;
            continue;
        }
        break;
    }

    // Strip leading blank lines from extracted text
    while (start < pos && (raw[start] == '\n' || raw[start] == '\r'))
        ++start;

    // Find closing brace
    auto brace = raw.find('{', pos);
    if (brace == std::string::npos) return "";
    int depth = 0;
    auto end = brace;
    for (; end < raw.size(); ++end)
    {
        if (raw[end] == '{') ++depth;
        else if (raw[end] == '}') { --depth; if (depth == 0) { ++end; break; } }
    }

    return raw.substr(start, end - start);
}

static nlohmann::json to_render_cpp_type_input(const tpp::IR &iRep,
                                               const std::vector<std::string> &includes,
                                               const std::string &namespaceName)
{
    codegen::CodegenInput ctx = codegen::buildCodegenInput(iRep);
    for (auto &s : ctx.structs)
        s.rawTypedefs = toCppStringLiteral(extractTypeDefinition(iRep.rawTypedefs, "struct", s.name));
    for (auto &e : ctx.enums)
        e.rawTypedefs = toCppStringLiteral(extractTypeDefinition(iRep.rawTypedefs, "enum", e.name));
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
    bool hasPol = !ir.policies.empty();

    codegen::ConvertConfig cfg;
    cfg.functionPrefix = functionPrefix;
    cfg.callNeedsTry = false;

    std::vector<codegen::RenderFunctionDef> functions;
    for (const auto &fn : ir.functions)
    {
        std::vector<codegen::ParamInfo> params;
        for (size_t i = 0; i < fn.params.size(); ++i)
            params.push_back({fn.params[i].name,
                              std::make_unique<codegen::TypeKind>(codegen::typeKindToContext(*fn.params[i].type))});

        int scope = 0;
        auto body = std::make_unique<std::vector<codegen::Instruction>>();
        for (const auto &instr : *fn.body)
            body->push_back(codegen::convertInstruction(instr, "_sb", fn.policy, scope, ir, cfg));

        codegen::RenderFunctionDef def;
        def.name = fn.name;
        def.params = std::move(params);
        def.body = std::move(body);
        functions.push_back(std::move(def));
    }

    codegen::RenderFunctionsInput result;
    result.functions = std::move(functions);
    result.hasPolicies = hasPol;
    result.policies = hasPol ? codegen::buildPolicyContext(ir, "std::nullopt") : std::vector<codegen::PolicyInfo>{};
    result.functionPrefix = functionPrefix;
    result.staticModifier = "";
    result.includes = std::vector<std::string>(includes.begin(), includes.end());
    if (!namespaceName.empty())
        result.namespaceName = namespaceName;
    return result;
}