#include <tpp/Compiler.h>
#include <tpp/Instruction.h>
#include <CodegenHelpers.h>
#include "defs.h"
#include <fstream>
#include <iostream>
#include <map>

// usage: tpp2cpp [options]
// options:
// -h, --help: print usage info
// -v, --verbose: print verbose output (for debugging tpp2cpp itself)
// --log <file>: also log output to the specified file (for debugging tpp2cpp itself)
// -ns --namespace <name>: wrap generated code in a namespace (default: none)
// -t, --types: produces a header file with type definitions
// -fun, --functions: produces a header file rendering the template functions as cpp functions
// -impl, --implementation: produces a cpp file with the implementations of the functions declared in the functions header (only needed if -fun/--functions is used)
// -i <file>, --include <file>: files to include at the top of the generated code (can be specified multiple times)
// --input <file>: file to read intermediate representation from (in json format, in the same format as tpp's output) instead of stdin
// This executable renders type definitions and template functions from JSON (in the same format as tpp's output) to C++ code, and prints it to stdout.
// intermediate representation is expected to be passed via stdin as json.

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
    bool verbose = false;
    std::ofstream logFile;
    std::string namespaceName;
    Mode mode = Mode::None;
    bool externalRuntime = false;
    std::vector<std::string> includes;
    tpp::IR input;
    tpp2cpp(int argc, char *argv[]);
    ~tpp2cpp();
    void run();
    void log(const std::string &message);
};

int main(int argc, char *argv[])
{
    tpp2cpp app(argc, argv);
    app.run();
    return EXIT_SUCCESS;
}

void printUsage()
{
    std::cout << "Usage: tpp2cpp [options]\n"
                 "Options:\n"
                 "  -h, --help: print usage info\n"
                 "  -v, --verbose: print verbose output (for debugging tpp2cpp itself)\n"
                 "  --log <file>: also log output to the specified file (for debugging tpp2cpp itself)\n"
                 "  -ns --namespace <name>: wrap generated code in a namespace (default: none)\n"
                 "  -t, --types: produces a header file with type definitions\n"
                 "  -fun, --functions: produces a header file rendering the template functions as cpp functions\n"
                 "  -impl, --implementation: produces a cpp file with the implementations of the functions declared in the functions header (only needed if -fun/--functions is used)\n"
                 "  -i <file>, --include <file>: files to include at the top of the generated code (can be specified multiple times)\n"
                 "  --input <file>: file to read intermediate representation from (in json format, in the same format as tpp's output) instead of stdin\n";
}

tpp2cpp::tpp2cpp(int argc, char *argv[])
{
    std::string inputFileName;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            exit(EXIT_SUCCESS);
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg == "--log")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--log option requires a file argument\n";
                exit(EXIT_FAILURE);
            }
            logFile.open(argv[++i]);
            if (!logFile.is_open())
            {
                std::cerr << "Could not open log file: " << argv[i] << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (arg == "-ns" || arg == "--namespace")
        {
            if (i + 1 >= argc)
            {
                std::cerr << arg << " option requires a name argument\n";
                exit(EXIT_FAILURE);
            }
            namespaceName = argv[++i];
        }
        else if (arg == "-t" || arg == "--types")
        {
            if (mode != Mode::None)
            {
                std::cerr << "Only one of -t/--types, -fun/--functions and -impl/--implementation can be specified\n";
                exit(EXIT_FAILURE);
            }
            mode = Mode::Types;
        }
        else if (arg == "-fun" || arg == "--functions")
        {
            if (mode != Mode::None)
            {
                std::cerr << "Only one of -t/--types, -fun/--functions and -impl/--implementation can be specified\n";
                exit(EXIT_FAILURE);
            }
            mode = Mode::Functions;
        }
        else if (arg == "-impl" || arg == "--implementation")
        {
            if (mode != Mode::None)
            {
                std::cerr << "Only one of -t/--types, -fun/--functions and -impl/--implementation can be specified\n";
                exit(EXIT_FAILURE);
            }
            mode = Mode::Implementation;
        }
        else if (arg == "-runtime" || arg == "--runtime")
        {
            if (mode != Mode::None)
            {
                std::cerr << "Only one of -t/--types, -fun/--functions, -impl/--implementation and -runtime can be specified\n";
                exit(EXIT_FAILURE);
            }
            mode = Mode::Runtime;
        }
        else if (arg == "--extern-runtime")
        {
            externalRuntime = true;
        }
        else if (arg == "-i" || arg == "--include")
        {
            if (i + 1 >= argc)
            {
                std::cerr << arg << " option requires a file argument\n";
                exit(EXIT_FAILURE);
            }
            includes.push_back(argv[++i]);
        }
        else if (arg == "--input")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--input option requires a file argument\n";
                exit(EXIT_FAILURE);
            }
            inputFileName = argv[++i];
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\nUse -h or --help for usage info.\n";
            exit(EXIT_FAILURE);
        }
    }
    // read input from file or stdin
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

tpp2cpp::~tpp2cpp()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

void tpp2cpp::log(const std::string &message)
{
    if (verbose)
    {
        std::cout << message << std::endl;
    }
    if (logFile.is_open())
    {
        logFile << message << std::endl;
    }
}

nlohmann::json to_render_cpp_type_input(const tpp::IR &iRep,
                                        const std::vector<std::string> &includes,
                                        const std::string &namespaceName);

nlohmann::json to_render_cpp_functions_input(const tpp::IR &iRep,
                                             const std::vector<std::string> &includes,
                                             const std::string &namespaceName);

static nlohmann::json buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::vector<std::string> &includes,
    const std::string &namespaceName);

void tpp2cpp::run()
{
    tpp::Compiler compiler;
    tpp::IR iRep;

    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(2);

    compiler.add_types(types_content, diagnostics.emplace_back("defs.tpp.types").diagnostics);
    compiler.add_templates(template_content, diagnostics.emplace_back("defs.tpp").diagnostics);

    log("Compiling templates...");

    if (!compiler.compile(iRep))
    {
        for (const auto &msg : diagnostics)
        {
            for (auto &gccDiagnostic : msg.toGCCDiagnostics())
            {
                log(gccDiagnostic);
                if (!verbose)
                {
                    std::cerr << gccDiagnostic << std::endl;
                }
            }
        }
        exit(EXIT_FAILURE);
    }

    tpp::FunctionSymbol functionSymbol;
    std::string functionName;
    std::function<nlohmann::json(const tpp::IR &, const std::vector<std::string> &)> toInputFunction;

    switch (mode)
    {
    case Mode::None:
    {
        printUsage();
        exit(EXIT_FAILURE);
        break;
    }
    case Mode::Types:
    {
        functionName = "render_cpp_types";
        auto ns = namespaceName;
        toInputFunction = [ns](const tpp::IR &co, const std::vector<std::string> &inc) {
            return to_render_cpp_type_input(co, inc, ns);
        };
        break;
    }
    case Mode::Functions:
    {
        functionName = "render_cpp_functions";
        auto ns = namespaceName;
        toInputFunction = [ns](const tpp::IR &co, const std::vector<std::string> &inc) {
            return to_render_cpp_functions_input(co, inc, ns);
        };
        break;
    }
    case Mode::Implementation:
    {
        // Native instruction IR rendering — render directly without interpreter
        std::string functionPrefix = namespaceName.empty() ? "" : "";
        nlohmann::json ctx = buildFunctionsContext(input, functionPrefix, includes, namespaceName);
        if (externalRuntime)
            ctx["externalRuntime"] = true;

        std::string implFunctionName = "render_cpp_native_implementation";
        tpp::FunctionSymbol implFn;
        std::string implError;
        std::string implOutput;
        if (!(iRep.get_function(implFunctionName, implFn, implError) && implFn.render(ctx, implOutput, implError)))
        {
            std::string errorMessage = "defs.tpp: error: Failed to render output: " + implError;
            log(errorMessage);
            if (!verbose) std::cerr << errorMessage << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << codegen::reindent(implOutput) << std::endl;
        return;
    }
    case Mode::Runtime:
    {
        // Generate standalone runtime header with helper functions and policy definitions
        std::string functionPrefix = namespaceName.empty() ? "" : "";
        nlohmann::json ctx = buildFunctionsContext(input, functionPrefix, includes, namespaceName);
        ctx["externalRuntime"] = false;

        std::string rtFunctionName = "render_cpp_runtime";
        tpp::FunctionSymbol rtFn;
        std::string rtError;
        std::string rtOutput;
        if (!(iRep.get_function(rtFunctionName, rtFn, rtError) && rtFn.render(ctx, rtOutput, rtError)))
        {
            std::string errorMessage = "defs.tpp: error: Failed to render runtime: " + rtError;
            log(errorMessage);
            if (!verbose) std::cerr << errorMessage << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << codegen::reindent(rtOutput) << std::endl;
        return;
    }
    }

    std::string renderedOutput;
    std::string error;

    if (!(iRep.get_function(functionName, functionSymbol, error) && functionSymbol.render(toInputFunction(input, includes), renderedOutput, error)))
    {
        std::string errorMessage = "defs.tpp: error: Failed to render output: " + error;
        log(errorMessage);
        if (!verbose)
        {
            std::cerr << errorMessage << std::endl;
        }
        exit(EXIT_FAILURE);
    }

    std::cout << renderedOutput << std::endl;
}

static std::string typeRefToCppString(const tpp::TypeRef &t)
{
    if (std::holds_alternative<tpp::StringType>(t)) return "std::string";
    if (std::holds_alternative<tpp::IntType>(t)) return "int";
    if (std::holds_alternative<tpp::BoolType>(t)) return "bool";
    if (auto *lt = std::get_if<std::shared_ptr<tpp::ListType>>(&t))
        return "std::vector<" + typeRefToCppString((*lt)->elementType) + ">";
    if (auto *ot = std::get_if<std::shared_ptr<tpp::OptionalType>>(&t))
        return "std::optional<" + typeRefToCppString((*ot)->innerType) + ">";
    if (auto *nt = std::get_if<tpp::NamedType>(&t))
        return nt->name;
    return "";
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

nlohmann::json to_render_cpp_type_input(const tpp::IR &iRep,
                                        const std::vector<std::string> &includes,
                                        const std::string &namespaceName)
{
    nlohmann::json result;
    result["includes"] = includes;
    if (!namespaceName.empty())
        result["namespaceName"] = namespaceName;
    nlohmann::json typesJson = nlohmann::json::array();

    // Emit enums first (often used as field types in structs)
    for (const auto &e : iRep.types.enums)
    {
        nlohmann::json enumJson;
        enumJson["name"] = e.name;
        if (!e.doc.empty()) enumJson["doc"] = e.doc;
        nlohmann::json variants = nlohmann::json::array();
        int variantIndex = 0;
        for (const auto &v : e.variants)
        {
            nlohmann::json variantJson;
            variantJson["tag"] = v.tag;
            variantJson["index"] = variantIndex++;
            variantJson["isRecursivePayload"] = v.recursive && v.payload.has_value();
            if (v.payload.has_value())
                variantJson["payloadType"] = typeRefToCppString(*v.payload);
            if (!v.doc.empty()) variantJson["doc"] = v.doc;
            variants.push_back(variantJson);
        }
        enumJson["variants"] = variants;
        enumJson["definition"] = toCppStringLiteral(iRep.raw_typedefs);
        typesJson.push_back({{"Enum", enumJson}});
    }

    for (const auto &s : iRep.types.structs)
    {
        nlohmann::json structJson;
        structJson["name"] = s.name;
        if (!s.doc.empty()) structJson["doc"] = s.doc;
        nlohmann::json fields = nlohmann::json::array();
        for (const auto &f : s.fields)
        {
            nlohmann::json field;
            bool isOpt = std::holds_alternative<std::shared_ptr<tpp::OptionalType>>(f.type);
            field["name"] = f.name;
            field["isOptional"] = isOpt;
            if (!f.doc.empty()) field["doc"] = f.doc;
            if (f.recursive)
            {
                std::string innerT;
                if (isOpt)
                {
                    auto *ot = std::get_if<std::shared_ptr<tpp::OptionalType>>(&f.type);
                    innerT = typeRefToCppString((*ot)->innerType);
                }
                else
                {
                    innerT = typeRefToCppString(f.type);
                }
                field["type"] = "std::unique_ptr<" + innerT + ">";
                field["recursiveInnerType"] = innerT;
            }
            else
            {
                field["type"] = typeRefToCppString(f.type);
                if (isOpt)
                {
                    auto *ot = std::get_if<std::shared_ptr<tpp::OptionalType>>(&f.type);
                    field["innerType"] = typeRefToCppString((*ot)->innerType);
                }
            }
            fields.push_back(field);
        }
        structJson["fields"] = fields;
        structJson["definition"] = toCppStringLiteral(iRep.raw_typedefs);
        typesJson.push_back({{"Struct", structJson}});
    }

    result["types"] = typesJson;
    return result;
}

static nlohmann::json buildFunctionsInput(const tpp::IR &iRep,
                                          const std::vector<std::string> &includes,
                                          const std::string &namespaceName)
{
    nlohmann::json result;
    result["includes"] = includes;
    if (!namespaceName.empty())
        result["namespaceName"] = namespaceName;
    result["iRepJson"] = toCppStringLiteral(nlohmann::json(iRep).dump());
    result["functionPrefix"] = "";
    nlohmann::json functions = nlohmann::json::array();
    for (const auto &f : iRep.functions)
    {
        nlohmann::json func;
        func["name"] = f.name;
        if (!f.doc.empty()) func["doc"] = f.doc;
        nlohmann::json params = nlohmann::json::array();
        for (const auto &p : f.params)
        {
            nlohmann::json param;
            param["name"] = p.name;
            param["type"] = typeRefToCppString(p.type);
            params.push_back(param);
        }
        func["params"] = params;
        functions.push_back(func);
    }
    result["functions"] = functions;
    return result;
}

nlohmann::json to_render_cpp_functions_input(const tpp::IR &iRep,
                                             const std::vector<std::string> &includes,
                                             const std::string &namespaceName)
{
    return buildFunctionsInput(iRep, includes, namespaceName);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build RenderFunctionsInput context from instruction IR (native rendering)
// ═══════════════════════════════════════════════════════════════════════════════

static nlohmann::json buildFunctionsContext(
    const tpp::IR &ir, const std::string &functionPrefix,
    const std::vector<std::string> &includes,
    const std::string &namespaceName)
{
    bool hasPol = !ir.policies.all().empty();

    // Build field metadata for recursive/optional path adjustments
    struct FieldMeta { bool recursive; bool optional; };
    std::map<std::string, FieldMeta> fieldMeta;
    for (const auto &s : ir.types.structs)
        for (const auto &f : s.fields)
            fieldMeta[f.name] = {
                f.recursive,
                std::holds_alternative<std::shared_ptr<tpp::OptionalType>>(f.type)
            };

    codegen::ConvertConfig cfg;
    cfg.nullLiteral = "";  // Use truthiness checks (works for both optional and unique_ptr)
    cfg.functionPrefix = functionPrefix;
    cfg.callNeedsTry = false;
    cfg.transformCallArg = [&fieldMeta](const std::string &p, const tpp::TypeRef &) -> std::string {
        auto dot = p.rfind('.');
        if (dot == std::string::npos) return p;
        std::string fieldName = p.substr(dot + 1);
        auto it = fieldMeta.find(fieldName);
        if (it == fieldMeta.end()) return p;
        if (it->second.recursive || it->second.optional)
            return "*" + p;
        return p;
    };
    cfg.makeSpecSetupLines = [](int s, int numCols, const std::string &spec) {
        nlohmann::json lines = nlohmann::json::array();
        if (spec.empty()) return lines;
        if (spec.size() == 1)
        {
            lines.push_back(
                "std::fill(_spec" + std::to_string(s)
                + ".begin(), _spec" + std::to_string(s)
                + ".end(), '" + std::string(1, spec[0]) + "');");
        }
        else
        {
            for (size_t ci = 0; ci < spec.size() && ci < (size_t)numCols; ++ci)
                lines.push_back(
                    "_spec" + std::to_string(s) + "["
                    + std::to_string(ci) + "] = '"
                    + std::string(1, spec[ci]) + "';");
        }
        return lines;
    };

    nlohmann::json functions = nlohmann::json::array();
    for (const auto &fn : ir.instructionFunctions)
    {
        std::string paramsStr;
        std::string argsPassStr;
        for (size_t i = 0; i < fn.params.size(); ++i)
        {
            if (i > 0) { paramsStr += ", "; argsPassStr += ", "; }
            paramsStr += "typename tpp::ArgType<" + typeRefToCppString(fn.params[i].type) + ">::type " + fn.params[i].name;
            argsPassStr += fn.params[i].name;
        }

        std::string paramsStrWithPolicy = paramsStr;
        if (hasPol)
        {
            if (!paramsStrWithPolicy.empty()) paramsStrWithPolicy += ", ";
            paramsStrWithPolicy += "const std::string& _policy";
            if (!argsPassStr.empty()) argsPassStr += ", ";
            argsPassStr += "\"\"";
        }

        int scope = 0;
        nlohmann::json body = nlohmann::json::array();
        for (const auto &instr : fn.body)
            body.push_back(codegen::convertInstruction(instr, "_sb", fn.policy, scope, ir, cfg));

        functions.push_back({
            {"name", fn.name},
            {"paramsStr", paramsStr},
            {"paramsStrWithPolicy", paramsStrWithPolicy},
            {"argsPassStr", argsPassStr},
            {"body", body}
        });
    }

    bool needsDispatch = false;
    if (hasPol)
        for (const auto &f : functions)
            if (codegen::bodyHasRuntimePolicy(f["body"])) { needsDispatch = true; break; }

    nlohmann::json result = {
        {"functions", functions},
        {"hasPolicies", hasPol},
        {"needsApplyDispatch", needsDispatch},
        {"policies", hasPol ? codegen::buildPolicyContext(ir, "std::nullopt") : nlohmann::json::array()},
        {"functionPrefix", functionPrefix},
        {"includes", includes},
        {"staticModifier", ""}
    };
    if (!namespaceName.empty())
        result["namespaceName"] = namespaceName;
    return result;
}