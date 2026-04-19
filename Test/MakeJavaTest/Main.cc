// make-java-test — generates a Java test harness from a tpp test case directory.
//
// Usage: make-java-test <input_folder>
//
// Reads .tpp files from input_folder according to tpp-config.json, compiles them,
// extracts the main function's parameter info, and renders a Test.java file (to stdout).

#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <tpp/Runtime.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "make_java_test_types.h"
#include "make_java_test_functions.h"

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
// TypeKind → Java type name
// ═══════════════════════════════════════════════════════════════════════════════

static std::string typeKindToJavaBoxedType(const tpp::TypeKind &t, const std::string &namespaceName = "");

static std::string qualifyJavaTypeName(const std::string &name, const std::string &namespaceName)
{
    if (namespaceName.empty())
        return name;
    return namespaceName + "." + name;
}

static std::string typeKindToJavaType(const tpp::TypeKind &t, const std::string &namespaceName = "")
{
    switch (t.value.index())
    {
    case 0: return "String";   // Str
    case 1: return "int";      // Int
    case 2: return "boolean";  // Bool
    case 3: return qualifyJavaTypeName(std::get<3>(t.value), namespaceName); // Named
    case 4: return "java.util.List<" + typeKindToJavaBoxedType(*std::get<4>(t.value), namespaceName) + ">"; // List
    case 5: return typeKindToJavaType(*std::get<5>(t.value), namespaceName); // Optional
    default: return "Object";
    }
}

static std::string typeKindToJavaBoxedType(const tpp::TypeKind &t, const std::string &namespaceName)
{
    switch (t.value.index())
    {
    case 0: return "String";   // Str
    case 1: return "Integer";  // Int
    case 2: return "Boolean";  // Bool
    case 3: return qualifyJavaTypeName(std::get<3>(t.value), namespaceName); // Named
    case 4: return "java.util.List<" + typeKindToJavaBoxedType(*std::get<4>(t.value), namespaceName) + ">"; // List
    case 5: return typeKindToJavaBoxedType(*std::get<5>(t.value), namespaceName); // Optional
    default: return "Object";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// String literal escaping
// ═══════════════════════════════════════════════════════════════════════════════

static std::string javaStringLiteral(const std::string &s)
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
// Generate a Java parse line for a parameter
// ═══════════════════════════════════════════════════════════════════════════════

static std::string buildParseLine(const std::string &name, const tpp::TypeKind &type,
                                  const std::string &jsonLiteral,
                                  const std::string &namespaceName = "")
{
    switch (type.value.index())
    {
    case 0: // Str
        return "String " + name + " = (String) new org.json.JSONTokener(" + jsonLiteral + ").nextValue();";
    case 1: // Int
        return "int " + name + " = ((Number) new org.json.JSONTokener(" + jsonLiteral + ").nextValue()).intValue();";
    case 2: // Bool
        return "boolean " + name + " = (Boolean) new org.json.JSONTokener(" + jsonLiteral + ").nextValue();";
    case 3: { // Named
        const auto qualified = qualifyJavaTypeName(std::get<3>(type.value), namespaceName);
        return qualified + " " + name + " = " + qualified + ".fromJson(new org.json.JSONObject(" + jsonLiteral + "));";
    }
    case 4: { // List
        std::string jt = typeKindToJavaType(type, namespaceName);
        const auto &elemType = *std::get<4>(type.value);
        std::string elemExpr;
        switch (elemType.value.index())
        {
        case 0: elemExpr = "_arr.getString(_i)"; break;
        case 1: elemExpr = "_arr.getInt(_i)"; break;
        case 2: elemExpr = "_arr.getBoolean(_i)"; break;
        case 3: elemExpr = qualifyJavaTypeName(std::get<3>(elemType.value), namespaceName) + ".fromJson(_arr.getJSONObject(_i))"; break;
        default: elemExpr = "_arr.get(_i)"; break;
        }
        return jt + " " + name + " = new java.util.ArrayList<>(); "
               "{ org.json.JSONArray _arr = new org.json.JSONArray(" + jsonLiteral + "); "
               "for (int _i = 0; _i < _arr.length(); _i++) { " + name + ".add(" + elemExpr + "); } }";
    }
    default:
        return typeKindToJavaType(type, namespaceName) + " " + name + " = null;";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char *argv[])
{
    bool fragmentMode = false;
    std::string namespaceName;
    std::string runnerClassName;
    int argIndex = 1;
    if (argc >= 5 && std::string(argv[1]) == "--fragment")
    {
        fragmentMode = true;
        namespaceName = argv[2];
        runnerClassName = argv[3];
        argIndex = 4;
    }

    if (argc != argIndex + 1)
    {
        std::cerr << "Usage: make-java-test [--fragment <namespace> <runner>] <input_folder>" << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path testDir(argv[argIndex]);
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

    tpp::TppProject project;
    for (const auto &fe : fileEntries)
    {
        std::string content = readFile(fe.path);
        if (fe.isTypes) project.add_type_source(std::move(content), fe.path.string());
        else            project.add_template_source(std::move(content), fe.path.string());
    }

    for (const auto &pe : config.value("replacement-policies", nlohmann::json::array()))
    {
        const auto policyPath = testDir / pe.get<std::string>();
        project.add_policy_source(readFile(policyPath), policyPath.string());
    }

    std::vector<tpp::DiagnosticLSPMessage> diagnostics;
    tpp::LexedProject lexed;
    tpp::ParsedProject parsed;
    tpp::IR ir;
    if (!tpp::lex(project, lexed, diagnostics) ||
        !tpp::parse(lexed, parsed, diagnostics) ||
        !tpp::compile(parsed, ir, diagnostics))
    {
        for (const auto &msg : diagnostics)
            for (const auto &d : msg.toGCCDiagnostics())
                std::cerr << d << std::endl;
        return EXIT_FAILURE;
    }

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
        ? tpp::get_function(ir, templateName, mainFunc, error)
        : tpp::get_function(ir, templateName, signature, mainFunc, error);
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

    // Read expected_output.txt
    std::string expectedRaw = readFile(testDir / "expected_output.txt");
    if (!expectedRaw.empty() && expectedRaw.back() == '\n')
        expectedRaw.pop_back();

    // Build JavaTestDef
    JavaTestDef defs;
    defs.testName = testName;
    defs.namespaceName = namespaceName;
    defs.runnerClassName = runnerClassName;
    defs.expectedOutputLiteral = javaStringLiteral(expectedRaw);

    const auto &params = mainFunc->params;
    std::string callArgs;

    if (params.size() == 1)
    {
        nlohmann::json value;
        bool isList = (params[0].type->value.index() == 4); // List variant
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
        std::string jsonLit = javaStringLiteral(value.dump());
        JavaTestParam p;
        p.name = params[0].name;
        p.parseLine = buildParseLine(p.name, *params[0].type, jsonLit, namespaceName);
        defs.params.push_back(p);
        callArgs = p.name;
    }
    else
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            std::string jsonLit = javaStringLiteral(inputJson[i].dump());
            JavaTestParam p;
            p.name = params[i].name;
            p.parseLine = buildParseLine(p.name, *params[i].type, jsonLit, namespaceName);
            defs.params.push_back(p);
            if (i > 0) callArgs += ", ";
            callArgs += p.name;
        }
    }
    defs.callArgs = callArgs;

    if (fragmentMode)
        std::cout << render_fragment(defs);
    else
        std::cout << render_test(defs);
    return EXIT_SUCCESS;
}
