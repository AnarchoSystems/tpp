// usage: make-stat-test <input_folder>
// Reads .tpp files from input_folder according to tpp-config.json, compiles them,
// extracts the main function's parameter info, and renders a test file (to stdout).

#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <tpp/ProjectConfigResolver.h>
#include <tpp/Runtime.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>

#include "TestCaseIO.h"
#include "make_stat_test_types.h"
#include "make_stat_test_functions.h"

static std::string typeKindToCppStringNs(const tpp::TypeKind &t, const std::string &ns)
{
    switch (t.value.index())
    {
    case 0: return "std::string";                // Str
    case 1: return "int";                        // Int
    case 2: return "bool";                       // Bool
    case 3: {                                    // Named
        auto &name = std::get<3>(t.value);
        return ns.empty() ? name : (ns + "::" + name);
    }
    case 4:                                      // List
        return "std::vector<" + typeKindToCppStringNs(*std::get<4>(t.value), ns) + ">";
    case 5:                                      // Optional
        return "std::optional<" + typeKindToCppStringNs(*std::get<5>(t.value), ns) + ">";
    default: return "";
    }
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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: make-stat-test <input_folder>" << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path testDir(argv[1]);
    std::string testName = testDir.filename().string();

    // Read and resolve tpp-config.json
    tpp::ResolvedProjectInputs inputs;
    try { inputs = tpp::load_project_inputs(testDir); }
    catch (const std::exception &e)
    {
        std::cerr << testDir.string() << ": error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    const auto &config = inputs.config;
    const auto &configPath = inputs.configPath;

    // Collect .tpp files in config order: types first, then templates
    tpp::TppProject project;
    for (const auto &source : inputs.sourceFiles)
    {
        std::string content = test_support::read_text_file(source.path);
        if (source.isTypes)
            project.add_type_source(std::move(content), source.path.string());
        else
            project.add_template_source(std::move(content), source.path.string());
    }

    // Load replacement policies (same as tpp/Main.cc)
    for (const auto &policyPath : inputs.policyFiles)
    {
        project.add_policy_source(test_support::read_text_file(policyPath), policyPath.string());
    }

    const auto compileResult = tpp::compile(project);
    if (!compileResult)
    {
        for (const auto &msg : compileResult.diagnostics)
            for (const auto &d : msg.toGCCDiagnostics())
                std::cerr << d << std::endl;
        return EXIT_FAILURE;
    }
    const auto &iRep = compileResult.ir;

    // Read function selection and input from previews[0]
    const auto &previews = config.value("previews", nlohmann::json::array());
    if (previews.empty())
    {
        std::cerr << configPath.string() << ": error: no previews[0] found" << std::endl;
        return EXIT_FAILURE;
    }
    const auto &preview = previews[0];

    std::string templateName = preview.value("template", "main");
    std::vector<std::string> signature;
    if (preview.contains("signature") && preview["signature"].is_array())
    {
        for (const auto &entry : preview["signature"])
            signature.push_back(entry.get<std::string>());
    }

    const tpp::FunctionDef *mainFunc = nullptr;
    std::string error;
    const bool lookupOk = signature.empty()
        ? tpp::get_function(iRep, templateName, mainFunc, error)
        : tpp::get_function(iRep, templateName, signature, mainFunc, error);
    if (!lookupOk)
    {
        std::cerr << testDir.string() << ": error: " << error << std::endl;
        return EXIT_FAILURE;
    }

    if (!preview.contains("input"))
    {
        std::cerr << configPath.string() << ": error: no previews[0].input found" << std::endl;
        return EXIT_FAILURE;
    }
    nlohmann::json inputJson = preview.at("input");

    const auto expectedOutput = test_support::load_expected_output(testDir);
    if (!expectedOutput.has_value())
    {
        std::cerr << testDir.string()
                  << ": error: no expected output found (expected_output.txt or test-case.json expected_output)"
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::string expectedRaw = *expectedOutput;
    if (!expectedRaw.empty() && expectedRaw.back() == '\n')
        expectedRaw.pop_back();

    // Build Defs
    Defs defs;
    defs.testName = testName;
    defs.expectedOutput = toGoodConstexprString(expectedRaw);
    defs.iRepJson = toGoodConstexprString(nlohmann::json(iRep).dump());
    defs.previewFunctionName = templateName;
    defs.previewSignature = signature;
    defs.hasPreviewSignature = !signature.empty();

    const auto &params = mainFunc->params;
    if (params.size() == 1)
    {
        Input inp;
        inp.type = typeKindToCppStringNs(*params[0].type, testName);
        nlohmann::json value;
        bool isList = (params[0].type->value.index() == 4); // List variant
        if (isList)
        {
            // list param: unwrap double-wrapped array (unary-list convention)
            if (inputJson.is_array() && inputJson.size() == 1 && inputJson[0].is_array())
                value = inputJson[0];
            else
                value = inputJson;
        }
        else
        {
            // non-list param: unwrap single-element array wrapper
            if (inputJson.is_array() && inputJson.size() == 1)
                value = inputJson[0];
            else
                value = inputJson;
        }
        inp.value = value.dump();
        defs.inputs.push_back(inp);
    }
    else
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            Input inp;
            inp.type = typeKindToCppStringNs(*params[i].type, testName);
            inp.value = inputJson[i].dump();
            defs.inputs.push_back(inp);
        }
    }

    std::cout << render_test(defs);
    return EXIT_SUCCESS;
}
