#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>

namespace tpp
{
    // based on lsp

    enum class DiagnosticSeverity
    {
        Error = 1,
        Warning = 2,
        Information = 3,
        Hint = 4
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(DiagnosticSeverity, {{DiagnosticSeverity::Error, "error"},
                                                      {DiagnosticSeverity::Warning, "warning"},
                                                      {DiagnosticSeverity::Information, "information"},
                                                      {DiagnosticSeverity::Hint, "hint"}});

    struct Position
    {
        int line;      // 0-based
        int character; // 0-based
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Position, line, character)
        bool operator==(const Position &other) const
        {
            return line == other.line && character == other.character;
        }
    };

    struct Range
    {
        Position start;
        Position end;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Range, start, end)
        bool operator==(const Range &other) const
        {
            return start == other.start && end == other.end;
        }
    };

    struct DiagnosticRelatedInformation
    {
        std::string uri;
        Range range;
        std::string message;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(DiagnosticRelatedInformation, uri, range, message)
        bool operator==(const DiagnosticRelatedInformation &other) const
        {
            return uri == other.uri && range == other.range && message == other.message;
        }
    };

    struct Diagnostic
    {
        Range range;
        std::string message;

        std::optional<DiagnosticSeverity> severity;
        std::optional<std::string> code;   // string is more flexible than int
        std::optional<std::string> source; // e.g. "clang", "mylinter"

        std::vector<DiagnosticRelatedInformation> relatedInformation;

        // Optional but useful extensions:
        std::optional<std::string> codeDescription; // URL in LSP
        std::vector<std::string> tags;              // e.g. "deprecated", "unnecessary"
        // need actual implementation of serialization bc of the optional fields
        friend void to_json(nlohmann::json &j, const Diagnostic &d)
        {
            j = nlohmann::json{
                {"range", d.range},
                {"message", d.message}};
            if (d.severity.has_value())
                j["severity"] = d.severity.value();
            if (d.code.has_value())
                j["code"] = d.code.value();
            if (d.source.has_value())
                j["source"] = d.source.value();
            if (!d.relatedInformation.empty())
                j["relatedInformation"] = d.relatedInformation;
            if (d.codeDescription.has_value())
                j["codeDescription"] = d.codeDescription.value();
            if (!d.tags.empty())
                j["tags"] = d.tags;
        }
        friend void from_json(const nlohmann::json &j, Diagnostic &d)
        {
            j.at("range").get_to(d.range);
            j.at("message").get_to(d.message);
            if (j.contains("severity"))
                d.severity = j.at("severity").get<DiagnosticSeverity>();
            if (j.contains("code"))
                d.code = j.at("code").get<std::string>();
            if (j.contains("source"))
                d.source = j.at("source").get<std::string>();
            if (j.contains("relatedInformation"))
                d.relatedInformation = j.at("relatedInformation").get<std::vector<DiagnosticRelatedInformation>>();
            if (j.contains("codeDescription"))
                d.codeDescription = j.at("codeDescription").get<std::string>();
            if (j.contains("tags"))
                d.tags = j.at("tags").get<std::vector<std::string>>();
        }
        bool operator==(const Diagnostic &other) const
        {
            return range == other.range &&
                   message == other.message &&
                   severity == other.severity &&
                   code == other.code &&
                   source == other.source &&
                   relatedInformation == other.relatedInformation &&
                   codeDescription == other.codeDescription &&
                   tags == other.tags;
        }
    };

}