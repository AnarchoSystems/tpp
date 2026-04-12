// make-swift-test — generates a Swift test harness from a tpp test case directory.
//
// Usage: make-swift-test <input_folder>
//
// Reads .tpp files from input_folder according to tpp-config.json, compiles them,
// extracts the main function's parameter info, and renders a Test.swift file (to stdout).

#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "make_swift_test_types.h"
#include "make_swift_test_functions.h"

static std::string readFile(const std::filesystem::path &path)
{
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

static bool matchGlob(const std::string &pattern, const std::string &text)
{
    size_t pi = 0, ti = 0, starPos = std::string::npos, matchPos = 0;
    while (ti < text.size())
    {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti]))
            { ++pi; ++ti; }
        else if (pi < pattern.size() && pattern[pi] == '*')
            { starPos = pi++; matchPos = ti; }
        else if (starPos != std::string::npos)
            { pi = starPos + 1; ti = ++matchPos; }
        else
            return false;
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

static std::vector<std::filesystem::path> expandPattern(const std::filesystem::path &baseDir,
                                                        const std::string &pattern)
{
    std::filesystem::path patPath(pattern);
    std::string filename = patPath.filename().string();
    std::filesystem::path parentDir = baseDir / patPath.parent_path();
    if (filename.find('*') == std::string::npos)
        return {baseDir / pattern};
    std::vector<std::filesystem::path> results;
    if (std::filesystem::is_directory(parentDir))
    {
        for (const auto &entry : std::filesystem::directory_iterator(parentDir))
        {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            if (name.size() < 4 || name.substr(name.size() - 4) != ".tpp") continue;
            if (matchGlob(filename, name)) results.push_back(entry.path());
        }
        std::sort(results.begin(), results.end());
    }
    return results;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TypeRef → Swift type name
// ═══════════════════════════════════════════════════════════════════════════════

static std::string typeKindToSwiftType(const tpp::TypeKind &t)
{
    switch (t.value.index())
    {
    case 0: return "String";
    case 1: return "Int";
    case 2: return "Bool";
    case 3: return std::get<3>(t.value);
    case 4: return "[" + typeKindToSwiftType(*std::get<4>(t.value)) + "]";
    case 5: return typeKindToSwiftType(*std::get<5>(t.value)) + "?";
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
                                        const std::string &jsonLiteral)
{
    std::string swiftType = typeKindToSwiftType(type);

    if (type.value.index() == 5)
    {
        std::string innerType = typeKindToSwiftType(*std::get<5>(type.value));
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
    if (argc != 2)
    {
        std::cerr << "Usage: make-swift-test <input_folder>" << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path testDir(argv[1]);
    std::string testName = testDir.filename().string();

    // Read and parse tpp-config.json
    std::filesystem::path configPath = testDir / "tpp-config.json";
    nlohmann::json config;
    try { config = nlohmann::json::parse(readFile(configPath)); }
    catch (const std::exception &e)
    {
        std::cerr << configPath.string() << ": error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Compile the test case
    struct FileEntry { std::filesystem::path path; bool isTypes; };
    std::vector<FileEntry> fileEntries;
    for (const auto &p : config.value("types", nlohmann::json::array()))
        for (const auto &f : expandPattern(testDir, p.get<std::string>()))
            fileEntries.push_back({f, true});
    for (const auto &p : config.value("templates", nlohmann::json::array()))
        for (const auto &f : expandPattern(testDir, p.get<std::string>()))
            fileEntries.push_back({f, false});

    tpp::Compiler compiler;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    diagnostics.reserve(fileEntries.size());
    for (const auto &fe : fileEntries)
    {
        std::string content = readFile(fe.path);
        auto &msg = diagnostics.emplace_back(fe.path.string());
        if (fe.isTypes) compiler.add_types(content, msg.diagnostics);
        else            compiler.add_templates(content, msg.diagnostics);
    }

    for (const auto &pe : config.value("replacement-policies", nlohmann::json::array()))
    {
        nlohmann::json pj = nlohmann::json::parse(readFile(testDir / pe.get<std::string>()));
        std::string err;
        if (!compiler.add_policy(pj, err))
        {
            std::cerr << testDir.string() << ": error: " << err << std::endl;
            return EXIT_FAILURE;
        }
    }

    tpp::IR ir;
    if (!compiler.compile(ir))
    {
        for (const auto &msg : diagnostics)
            for (const auto &d : msg.toGCCDiagnostics())
                std::cerr << d << std::endl;
        return EXIT_FAILURE;
    }

    // Find the main function
    const tpp::FunctionDef *mainFunction = nullptr;
    for (const auto &fn : ir.functions)
    {
        if (fn.name == "main") { mainFunction = &fn; break; }
    }
    if (!mainFunction)
    {
        std::cerr << testDir.string() << ": error: function 'main' not found" << std::endl;
        return EXIT_FAILURE;
    }

    // Read input from previews[0].input
    const auto &previews = config.value("previews", nlohmann::json::array());
    if (previews.empty() || !previews[0].contains("input"))
    {
        std::cerr << configPath.string() << ": error: no previews[0].input found" << std::endl;
        return EXIT_FAILURE;
    }
    nlohmann::json inputJson = previews[0].at("input");

    // Read expected_output.txt
    std::string expectedRaw = readFile(testDir / "expected_output.txt");
    if (!expectedRaw.empty() && expectedRaw.back() == '\n')
        expectedRaw.pop_back();

    // Build SwiftTestDef
    SwiftTestDef defs;
    defs.testName = testName;
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
        p.parseLine = buildSwiftDecodeLine(p.name, *params[0].type, jsonLit);
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
            p.parseLine = buildSwiftDecodeLine(p.name, *params[i].type, jsonLit);
            defs.params.push_back(p);
            if (i > 0) callArgs += ", ";
            callArgs += p.name;
        }
    }
    defs.callArgs = callArgs;
    defs.hasPolicies = !ir.policies.empty();

    std::cout << render_test(defs);
    return EXIT_SUCCESS;
}
