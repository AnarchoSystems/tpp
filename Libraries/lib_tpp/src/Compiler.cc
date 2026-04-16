#include <tpp/Compiler.h>

#include <tpp/IRAssembler.h>
#include <tpp/Lowering.h>

#include "tpp/PublicIRConverter.h"
#include "tpp/TemplateParser.h"
#include "tpp/TypedefParser.h"

#include <algorithm>

namespace tpp
{

namespace
{
    constexpr const char *kProjectDiagnosticUri = "tpp://project";

    bool has_any_diagnostics(const std::vector<DiagnosticLSPMessage> &messages) noexcept
    {
        for (const auto &message : messages)
        {
            if (!message.diagnostics.empty())
                return true;
        }
        return false;
    }

    std::vector<Diagnostic> &diagnostics_for(std::vector<DiagnosticLSPMessage> &messages,
                                             const std::string &uri)
    {
        auto it = std::find_if(messages.begin(), messages.end(), [&](const DiagnosticLSPMessage &message)
        {
            return message.uri == uri;
        });
        if (it == messages.end())
        {
            messages.emplace_back(uri);
            return messages.back().diagnostics;
        }
        return it->diagnostics;
    }

    void add_project_diagnostic(std::vector<DiagnosticLSPMessage> &messages,
                                std::string message)
    {
        diagnostics_for(messages, kProjectDiagnosticUri).push_back(makeErrorDiagnostic(std::move(message)));
    }

    TokKind to_internal_kind(TypeSourceTokenKind kind)
    {
        switch (kind)
        {
        case TypeSourceTokenKind::Struct:
            return TokKind::Struct;
        case TypeSourceTokenKind::Enum:
            return TokKind::Enum;
        case TypeSourceTokenKind::LBrace:
            return TokKind::LBrace;
        case TypeSourceTokenKind::RBrace:
            return TokKind::RBrace;
        case TypeSourceTokenKind::Semi:
            return TokKind::Semi;
        case TypeSourceTokenKind::Colon:
            return TokKind::Colon;
        case TypeSourceTokenKind::LParen:
            return TokKind::LParen;
        case TypeSourceTokenKind::RParen:
            return TokKind::RParen;
        case TypeSourceTokenKind::Comma:
            return TokKind::Comma;
        case TypeSourceTokenKind::LAngle:
            return TokKind::LAngle;
        case TypeSourceTokenKind::RAngle:
            return TokKind::RAngle;
        case TypeSourceTokenKind::KwOptional:
            return TokKind::KwOptional;
        case TypeSourceTokenKind::KwList:
            return TokKind::KwList;
        case TypeSourceTokenKind::KwString:
            return TokKind::KwString;
        case TypeSourceTokenKind::KwInt:
            return TokKind::KwInt;
        case TypeSourceTokenKind::KwBool:
            return TokKind::KwBool;
        case TypeSourceTokenKind::Ident:
            return TokKind::Ident;
        case TypeSourceTokenKind::Comment:
            return TokKind::Comment;
        case TypeSourceTokenKind::DocComment:
            return TokKind::DocComment;
        case TypeSourceTokenKind::Eof:
            return TokKind::Eof;
        }
        return TokKind::Eof;
    }

    std::vector<Token> to_internal_tokens(const std::vector<TypeSourceToken> &tokens)
    {
        std::vector<Token> result;
        result.reserve(tokens.size());
        for (const auto &token : tokens)
        {
            result.push_back({
                to_internal_kind(token.kind),
                token.text,
                token.range.start.line,
                token.range.start.character
            });
        }
        return result;
    }

    void clear_range(Range &range) noexcept
    {
        range = {};
    }

    void strip_type_ranges(std::vector<compiler::StructDef> &structs,
                           std::vector<compiler::EnumDef> &enums)
    {
        for (auto &structDef : structs)
        {
            clear_range(structDef.sourceRange);
            for (auto &field : structDef.fields)
                clear_range(field.sourceRange);
        }

        for (auto &enumDef : enums)
        {
            clear_range(enumDef.sourceRange);
            for (auto &variant : enumDef.variants)
                clear_range(variant.sourceRange);
        }
    }

    void strip_case_ranges(compiler::CaseNode &caseNode);

    void strip_ast_ranges(std::vector<compiler::ASTNode> &nodes)
    {
        for (auto &node : nodes)
        {
            std::visit([&](auto &value)
            {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, compiler::AlignmentCellNode>)
                {
                    clear_range(value.sourceRange);
                }
                else if constexpr (std::is_same_v<T, compiler::CommentNode>)
                {
                    clear_range(value.startRange);
                    clear_range(value.endRange);
                }
                else if constexpr (std::is_same_v<T, compiler::InterpolationNode>)
                {
                    clear_range(value.sourceRange);
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::ForNode>>)
                {
                    if (!value)
                        return;
                    clear_range(value->sourceRange);
                    clear_range(value->endRange);
                    strip_ast_ranges(value->body);
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::IfNode>>)
                {
                    if (!value)
                        return;
                    clear_range(value->sourceRange);
                    clear_range(value->elseRange);
                    clear_range(value->endRange);
                    strip_ast_ranges(value->thenBody);
                    strip_ast_ranges(value->elseBody);
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::SwitchNode>>)
                {
                    if (!value)
                        return;
                    clear_range(value->sourceRange);
                    clear_range(value->endRange);
                    for (auto &caseNode : value->cases)
                        strip_case_ranges(caseNode);
                    if (value->defaultCase.has_value())
                        strip_case_ranges(*value->defaultCase);
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::FunctionCallNode>>)
                {
                    if (value)
                        clear_range(value->sourceRange);
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<compiler::RenderViaNode>>)
                {
                    if (value)
                        clear_range(value->sourceRange);
                }
            }, node);
        }
    }

    void strip_case_ranges(compiler::CaseNode &caseNode)
    {
        clear_range(caseNode.sourceRange);
        clear_range(caseNode.endRange);
        strip_ast_ranges(caseNode.body);
    }

    void strip_template_ranges(ParsedTemplateSource &templateSource)
    {
        clear_range(templateSource.sourceRange);
        strip_ast_ranges(templateSource.body);
    }

    void strip_semantic_model_ranges(compiler::SemanticModel &semanticModel)
    {
        strip_type_ranges(semanticModel.structs, semanticModel.enums);
        for (auto &function : semanticModel.mutable_functions())
        {
            clear_range(function.sourceRange);
            strip_ast_ranges(function.body);
        }
        semanticModel.mutable_type_source_files().clear();
    }

    std::optional<Range> find_type_name_range(const std::vector<TypeSourceToken> &tokens,
                                              const std::string &name)
    {
        for (std::size_t index = 0; index + 1 < tokens.size(); ++index)
        {
            const auto &current = tokens[index];
            const auto &next = tokens[index + 1];
            if ((current.kind == TypeSourceTokenKind::Struct || current.kind == TypeSourceTokenKind::Enum) &&
                next.kind == TypeSourceTokenKind::Ident &&
                next.text == name)
            {
                return next.range;
            }
        }
        return std::nullopt;
    }

    void add_duplicate_type_diagnostic(std::vector<Diagnostic> &diagnostics,
                                       const std::string &name,
                                       const std::optional<Range> &range)
    {
        Diagnostic diagnostic;
        if (range.has_value())
            diagnostic.range = *range;
        diagnostic.message = "type '" + name + "' is already defined";
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostics.push_back(std::move(diagnostic));
    }

    std::string extract_type_definition(const std::string &raw,
                                        const std::string &keyword,
                                        const std::string &name)
    {
        const std::string marker = keyword + " " + name;
        const auto pos = raw.find(marker);
        if (pos == std::string::npos)
            return "";

        auto start = pos;
        while (start > 0)
        {
            const auto prevEnd = start - 1;
            if (raw[prevEnd] != '\n')
                break;

            auto prevStart = raw.rfind('\n', prevEnd > 0 ? prevEnd - 1 : std::string::npos);
            prevStart = (prevStart == std::string::npos) ? 0 : prevStart + 1;
            const auto line = raw.substr(prevStart, prevEnd - prevStart);
            const auto firstNonWhitespace = line.find_first_not_of(" \t");
            if (firstNonWhitespace == std::string::npos)
            {
                start = prevStart;
                continue;
            }

            if (line.substr(firstNonWhitespace, 2) == "//")
            {
                start = prevStart;
                continue;
            }

            break;
        }

        while (start < pos && (raw[start] == '\n' || raw[start] == '\r'))
            ++start;

        const auto brace = raw.find('{', pos);
        if (brace == std::string::npos)
            return "";

        int depth = 0;
        auto end = brace;
        for (; end < raw.size(); ++end)
        {
            if (raw[end] == '{')
                ++depth;
            else if (raw[end] == '}')
            {
                --depth;
                if (depth == 0)
                {
                    ++end;
                    break;
                }
            }
        }

        return raw.substr(start, end - start);
    }

    void populate_type_definitions(ParsedTypeArtifact &artifact)
    {
        for (auto &structDef : artifact.structs)
            structDef.rawTypedefs = extract_type_definition(artifact.source.content, "struct", structDef.name);
        for (auto &enumDef : artifact.enums)
            enumDef.rawTypedefs = extract_type_definition(artifact.source.content, "enum", enumDef.name);
    }

    void merge_type_artifact(const ParsedTypeArtifact &artifact,
                             compiler::SemanticModel &semanticModel,
                             std::vector<Diagnostic> &diagnostics)
    {
        for (const auto &structDef : artifact.structs)
        {
            if (semanticModel.nameIndex.count(structDef.name) != 0)
            {
                add_duplicate_type_diagnostic(diagnostics,
                                              structDef.name,
                                              find_type_name_range(artifact.tokens, structDef.name));
                continue;
            }

            const std::size_t index = semanticModel.structs.size();
            semanticModel.structs.push_back(structDef);
            semanticModel.nameIndex[structDef.name] = {compiler::TypeKind::Struct, index};
        }

        for (const auto &enumDef : artifact.enums)
        {
            if (semanticModel.nameIndex.count(enumDef.name) != 0)
            {
                add_duplicate_type_diagnostic(diagnostics,
                                              enumDef.name,
                                              find_type_name_range(artifact.tokens, enumDef.name));
                continue;
            }

            const std::size_t index = semanticModel.enums.size();
            semanticModel.enums.push_back(enumDef);
            semanticModel.nameIndex[enumDef.name] = {compiler::TypeKind::Enum, index};
        }
    }

    bool is_same_signature(const compiler::TemplateFunction &lhs,
                           const compiler::TemplateFunction &rhs)
    {
        if (lhs.params.size() != rhs.params.size())
            return false;

        for (std::size_t index = 0; index < lhs.params.size(); ++index)
        {
            if (!(lhs.params[index].type == rhs.params[index].type))
                return false;
        }

        return true;
    }

    compiler::TemplateFunction to_template_function(const ParsedTemplateSource &templateSource)
    {
        compiler::TemplateFunction function;
        function.name = templateSource.name;
        function.params = templateSource.params;
        function.body = templateSource.body;
        function.policy = templateSource.policy;
        function.doc = templateSource.doc;
        function.sourceRange = templateSource.sourceRange;
        return function;
    }
}

void TppProject::clear() noexcept
{
    clear_type_sources();
    clear_template_sources();
    clear_policy_sources();
}

void TppProject::clear_type_sources() noexcept
{
    typeSources_.clear();
    nextTypeSourceIndex_ = 0;
}

void TppProject::clear_template_sources() noexcept
{
    templateSources_.clear();
    nextTemplateSourceIndex_ = 0;
}

void TppProject::clear_policy_sources() noexcept
{
    policySources_.clear();
    nextPolicySourceIndex_ = 0;
}

std::string TppProject::make_default_uri(const std::string &prefix, std::size_t index)
{
    return "memory://tpp/" + prefix + "/" + std::to_string(index);
}

void TppProject::add_type_source(std::string content, std::string uri) noexcept
{
    if (uri.empty())
        uri = make_default_uri("types", nextTypeSourceIndex_);
    ++nextTypeSourceIndex_;
    typeSources_.push_back({std::move(uri), std::move(content)});
}

void TppProject::add_template_source(std::string content, std::string uri) noexcept
{
    if (uri.empty())
        uri = make_default_uri("templates", nextTemplateSourceIndex_);
    ++nextTemplateSourceIndex_;
    templateSources_.push_back({std::move(uri), std::move(content)});
}

void TppProject::add_policy_source(std::string content, std::string uri) noexcept
{
    if (uri.empty())
        uri = make_default_uri("policies", nextPolicySourceIndex_);
    ++nextPolicySourceIndex_;
    policySources_.push_back({std::move(uri), std::move(content)});
}

bool lex(const TppProject &project,
         LexedProject &output,
         std::vector<DiagnosticLSPMessage> &diagnostics,
         CompileOptions) noexcept
{
    output = {};

    try
    {
        for (const auto &source : project.type_sources())
        {
            output.typeSources.push_back({source, tokenizeTypeSource(source.content)});
            diagnostics_for(diagnostics, source.uri);
        }

        for (const auto &source : project.template_sources())
        {
            output.templateSources.push_back({source, tokenizeTemplateSource(source.content)});
            diagnostics_for(diagnostics, source.uri);
        }

        for (const auto &source : project.policy_sources())
        {
            output.policySources.push_back(source);
            diagnostics_for(diagnostics, source.uri);
        }
    }
    catch (const std::exception &error)
    {
        add_project_diagnostic(diagnostics,
                               std::string("unexpected error while lexing project: ") + error.what());
    }
    catch (...)
    {
        add_project_diagnostic(diagnostics, "unexpected error while lexing project");
    }

    return !has_any_diagnostics(diagnostics);
}

bool parse(const LexedProject &project,
           ParsedProject &output,
           std::vector<DiagnosticLSPMessage> &diagnostics,
           CompileOptions options) noexcept
{
    output = {};

    if (has_any_diagnostics(diagnostics))
        return false;

    try
    {
        for (const auto &source : project.typeSources)
        {
            ParsedTypeArtifact artifact;
            artifact.source = source.source;
            artifact.tokens = source.tokens;

            auto tokens = to_internal_tokens(source.tokens);
            compiler::SemanticModel semanticModel;
            auto &sourceDiagnostics = diagnostics_for(diagnostics, source.source.uri);
            compiler::TypedefParser parser{tokens, 0, semanticModel, sourceDiagnostics};
            parser.parse();

            artifact.structs = std::move(semanticModel.structs);
            artifact.enums = std::move(semanticModel.enums);
            populate_type_definitions(artifact);
            if (!options.includeSourceRanges)
                strip_type_ranges(artifact.structs, artifact.enums);

            output.typeSources.push_back(std::move(artifact));
        }

        for (const auto &source : project.templateSources)
        {
            std::vector<ParsedTemplateSource> templates;
            auto &sourceDiagnostics = diagnostics_for(diagnostics, source.source.uri);
            parseTemplateSource(source.source.content, templates, sourceDiagnostics);

            for (auto &templateSource : templates)
            {
                if (!options.includeSourceRanges)
                    strip_template_ranges(templateSource);

                ParsedTemplateArtifact artifact;
                artifact.source = source.source;
                artifact.tokens = source.tokens;
                artifact.value = std::move(templateSource);
                output.templateSources.push_back(std::move(artifact));
            }
        }

        for (const auto &source : project.policySources)
        {
            ParsedPolicyArtifact artifact;
            artifact.source = source;

            auto &sourceDiagnostics = diagnostics_for(diagnostics, source.uri);
            if (source.content.empty())
            {
                sourceDiagnostics.push_back(makeErrorDiagnostic("policy file not found or empty"));
            }
            else
            {
                try
                {
                    artifact.value = nlohmann::json::parse(source.content);
                }
                catch (const std::exception &error)
                {
                    sourceDiagnostics.push_back(makeErrorDiagnostic(std::string("invalid JSON: ") + error.what()));
                }
            }

            output.policySources.push_back(std::move(artifact));
        }
    }
    catch (const std::exception &error)
    {
        add_project_diagnostic(diagnostics,
                               std::string("unexpected error while parsing project: ") + error.what());
    }
    catch (...)
    {
        add_project_diagnostic(diagnostics, "unexpected error while parsing project");
    }

    return !has_any_diagnostics(diagnostics);
}

bool analyze(const ParsedProject &project,
             AnalyzedProject &output,
             std::vector<DiagnosticLSPMessage> &diagnostics,
             CompileOptions options) noexcept
{
    output = {};

    if (has_any_diagnostics(diagnostics))
        return false;

    try
    {
        auto &semanticModel = output.semanticModel;
        std::vector<compiler::TemplateFunction> pendingFunctions;

        for (const auto &artifact : project.policySources)
        {
            if (!artifact.value.has_value())
                continue;
            auto &sourceDiagnostics = diagnostics_for(diagnostics, artifact.source.uri);
            compiler::load_policy_json(semanticModel.mutable_policies(),
                                       *artifact.value,
                                       sourceDiagnostics);
        }

        if (options.includeSourceRanges)
        {
            for (const auto &artifact : project.typeSources)
                semanticModel.mutable_type_source_files().push_back(classifyTypeSourceTokens(artifact.tokens));
        }

        for (const auto &artifact : project.typeSources)
        {
            auto &sourceDiagnostics = diagnostics_for(diagnostics, artifact.source.uri);
            merge_type_artifact(artifact, semanticModel, sourceDiagnostics);
        }

        bool hasSemanticErrors = false;
        if (!hasSemanticErrors)
        {
            for (const auto &artifact : project.typeSources)
            {
                auto tokens = to_internal_tokens(artifact.tokens);
                auto &sourceDiagnostics = diagnostics_for(diagnostics, artifact.source.uri);
                compiler::TypedefParser validator{tokens, 0, semanticModel, sourceDiagnostics};
                if (!validator.validateTypes())
                    hasSemanticErrors = true;
            }
        }

        if (!hasSemanticErrors)
        {
            for (const auto &artifact : project.typeSources)
            {
                auto tokens = to_internal_tokens(artifact.tokens);
                auto &sourceDiagnostics = diagnostics_for(diagnostics, artifact.source.uri);
                compiler::TypedefParser validator{tokens, 0, semanticModel, sourceDiagnostics};
                if (!validator.computeFiniteTypes())
                    hasSemanticErrors = true;
            }
        }

        if (!hasSemanticErrors && !project.typeSources.empty())
            compiler::annotateRecursiveFields(semanticModel);

        if (!hasSemanticErrors)
        {
            struct PendingTemplateValidation
            {
                std::size_t functionIndex;
                std::string uri;
                std::size_t bodyStartLine;
                std::vector<compiler::TemplateLine> templateLines;
                std::string headerText;
            };

            std::vector<PendingTemplateValidation> pendingValidations;
            for (const auto &artifact : project.templateSources)
            {
                auto function = to_template_function(artifact.value);
                auto duplicate = std::find_if(pendingFunctions.begin(), pendingFunctions.end(),
                                              [&](const compiler::TemplateFunction &existing)
                {
                    return existing.name == function.name && is_same_signature(existing, function);
                });

                if (duplicate != pendingFunctions.end())
                {
                    Diagnostic diagnostic;
                    diagnostic.range = artifact.value.headerRange;
                    diagnostic.message = "function '" + function.name + "' is already defined";
                    diagnostic.severity = DiagnosticSeverity::Error;
                    diagnostics_for(diagnostics, artifact.source.uri).push_back(std::move(diagnostic));
                    hasSemanticErrors = true;
                    continue;
                }

                pendingFunctions.push_back(std::move(function));
                pendingValidations.push_back({
                    pendingFunctions.size() - 1,
                    artifact.source.uri,
                    artifact.value.bodyStartLine,
                    compiler::parseTemplateLines(artifact.value.bodyText),
                    artifact.value.headerText
                });
            }

            for (const auto &pendingValidation : pendingValidations)
            {
                auto &sourceDiagnostics = diagnostics_for(diagnostics, pendingValidation.uri);
                compiler::validateTemplateSemantics(
                    pendingFunctions[pendingValidation.functionIndex],
                    pendingValidation.templateLines,
                    pendingValidation.bodyStartLine,
                    semanticModel,
                    semanticModel.policies(),
                    sourceDiagnostics,
                    pendingFunctions,
                    static_cast<int>(pendingValidation.bodyStartLine) - 1,
                    pendingValidation.headerText);
            }
        }

        semanticModel.mutable_functions() = std::move(pendingFunctions);

        if (!options.includeSourceRanges)
            strip_semantic_model_ranges(semanticModel);
    }
    catch (const std::exception &error)
    {
        add_project_diagnostic(diagnostics,
                               std::string("unexpected error during semantic analysis: ") + error.what());
    }
    catch (...)
    {
        add_project_diagnostic(diagnostics, "unexpected error during semantic analysis");
    }

    return !has_any_diagnostics(diagnostics);
}

bool compile(const AnalyzedProject &project,
             IR &output,
             std::vector<DiagnosticLSPMessage> &diagnostics,
             CompileOptions options) noexcept
{
    output = {};

    if (has_any_diagnostics(diagnostics))
        return false;

    try
    {
        std::vector<FunctionDef> functions;
        if (!compiler::lowerToFunctions(project.semanticModel.functions(),
                                        project.semanticModel,
                                        functions,
                                        options.includeSourceRanges))
        {
            add_project_diagnostic(diagnostics, "failed to lower templates to IR");
            return false;
        }

        auto structs = to_public_structs(project.semanticModel.structs,
                        options.includeSourceRanges,
                        options.includeRawTypedefs);
        auto enums = to_public_enums(project.semanticModel.enums,
                         options.includeSourceRanges,
                         options.includeRawTypedefs);
        auto policies = to_public_policies(project.semanticModel.policies());

        output = assemble_ir(std::move(structs),
                             std::move(enums),
                             std::move(functions),
                             std::move(policies),
                             options.includeSourceRanges);
    }
    catch (const std::exception &error)
    {
        add_project_diagnostic(diagnostics,
                               std::string("unexpected error while compiling IR: ") + error.what());
    }
    catch (...)
    {
        add_project_diagnostic(diagnostics, "unexpected error while compiling IR");
    }

    return !has_any_diagnostics(diagnostics);
}

} // namespace tpp