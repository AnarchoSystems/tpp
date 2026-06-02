// make-swift-test — generates a Swift test harness from a tpp test case directory.
//
// Usage: make-swift-test <input_folder>
//
// Reads .tpp files from input_folder according to tpp-config.json, compiles them,
// extracts the main function's parameter info, and renders a Test.swift file (to stdout).

#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <tpp/ProjectConfigResolver.h>
#include <tpp/Runtime.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>

#include "TestCaseIO.h"
#include "make_swift_test_types.h"
#include "make_swift_test_functions.h"

// ═══════════════════════════════════════════════════════════════════════════════
// TypeRef → Swift type name
// ═══════════════════════════════════════════════════════════════════════════════

static std::string qualifySwiftTypeName(const std::string &name, const std::string &namespaceName)
{
    if (namespaceName.empty())
        return name;
    return namespaceName + "." + name;
}

static std::string typeKindToSwiftType(const tpp::TypeKind &t, const std::string &namespaceName = "")
{
    switch (t.value.index())
    {
    case 0: return "String";
    case 1: return "Int";
    case 2: return "Bool";
    case 3: return qualifySwiftTypeName(std::get<3>(t.value), namespaceName);
    case 4: return "[" + typeKindToSwiftType(*std::get<4>(t.value), namespaceName) + "]";
    case 5: return typeKindToSwiftType(*std::get<5>(t.value), namespaceName) + "?";
    default: return "Any";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// String literal escaping
// ═══════════════════════════════════════════════════════════════════════════════

static std::string swiftStringLiteral(const std::string &s)
{
    std::string result = "\"";
    for (char c : s)
    {
        if (c == '"')       result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (c == '\0') result += "\\0";
        else result += c;
    }
    return result + "\"";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Generate a Swift parse line for a parameter
// ═══════════════════════════════════════════════════════════════════════════════

static std::string buildSwiftDecodeLine(const std::string &name, const tpp::TypeKind &type,
                                        const std::string &jsonLiteral,
                                        const std::string &namespaceName = "")
{
    std::string swiftType = typeKindToSwiftType(type, namespaceName);

    if (type.value.index() == 5)
    {
        std::string innerType = typeKindToSwiftType(*std::get<5>(type.value), namespaceName);
        return "let " + name + ": " + swiftType + " = " + jsonLiteral +
               ".data(using: .utf8).flatMap { try? JSONDecoder().decode(" +
               innerType + ".self, from: $0) }";
    }

    return "guard let " + name + " = " + jsonLiteral +
           ".data(using: .utf8).flatMap({ try? JSONDecoder().decode(" +
           swiftType + ".self, from: $0) }) else { fputs(\"Failed to parse parameter " +
           name + "\\n\", stderr); exit(1) }";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char *argv[])
{
    bool fragmentMode = false;
    std::string namespaceName;
    std::string runnerName;
    int argIndex = 1;
    if (argc >= 5 && std::string(argv[1]) == "--fragment")
    {
        fragmentMode = true;
        namespaceName = argv[2];
        runnerName = argv[3];
        argIndex = 4;
    }

    if (argc != argIndex + 1)
    {
        std::cerr << "Usage: make-swift-test [--fragment <namespace> <runner>] <input_folder>" << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path testDir(argv[argIndex]);
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

    // Compile the test case
    tpp::TppProject project;
    for (const auto &source : inputs.sourceFiles)
    {
        std::string content = test_support::read_text_file(source.path);
        if (source.isTypes) project.add_type_source(std::move(content), source.path.string());
        else                project.add_template_source(std::move(content), source.path.string());
    }

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
    const auto &ir = compileResult.ir;

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

    const tpp::FunctionDef *mainFunction = nullptr;
    std::string error;
    const bool lookupOk = signature.empty()
        ? tpp::get_function(ir, templateName, mainFunction, error)
        : tpp::get_function(ir, templateName, signature, mainFunction, error);
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

    // Build SwiftTestDef
    SwiftTestDef defs;
    defs.testName = testName;
    defs.namespaceName = namespaceName;
    defs.runnerName = runnerName;
    defs.expectedOutputLiteral = swiftStringLiteral(expectedRaw);

    const auto &params = mainFunction->params;
    std::string callArgs;

    if (params.size() == 1)
    {
        nlohmann::json value;
        bool isList = params[0].type->value.index() == 4;
        if (isList)
        {
            if (inputJson.is_array() && inputJson.size() == 1 && inputJson[0].is_array())
                value = inputJson[0];
            else
                value = inputJson;
        }
        else
        {
            if (inputJson.is_array() && inputJson.size() == 1)
                value = inputJson[0];
            else
                value = inputJson;
        }
        std::string jsonLit = swiftStringLiteral(value.dump());
        SwiftTestParam p;
        p.name = params[0].name;
        p.parseLine = buildSwiftDecodeLine(p.name, *params[0].type, jsonLit, namespaceName);
        defs.params.push_back(p);
        callArgs = p.name;
    }
    else
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            std::string jsonLit = swiftStringLiteral(inputJson[i].dump());
            SwiftTestParam p;
            p.name = params[i].name;
            p.parseLine = buildSwiftDecodeLine(p.name, *params[i].type, jsonLit, namespaceName);
            defs.params.push_back(p);
            if (i > 0) callArgs += ", ";
            callArgs += p.name;
        }
    }
    defs.callArgs = callArgs;
    defs.hasPolicies = !ir.policies.empty();

    if (fragmentMode)
        std::cout << render_fragment(defs);
    else
        std::cout << render_test(defs);
    return EXIT_SUCCESS;
}
