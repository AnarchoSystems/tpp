#include "tpp/ToolingCompile.h"

#include "tpp/CompilerPipeline.h"

namespace tpp
{

namespace
{
    void rebuild_tooling_symbol_index(const compiler::SemanticModel &semanticModel,
                                      ToolingSymbolIndex &symbolIndex)
    {
        symbolIndex = {};

        for (const auto &structDef : semanticModel.structs_view())
        {
            symbolIndex.namedTypeLocations[structDef.name] = {structDef.sourceUri, structDef.sourceRange};

            auto &fieldLocations = symbolIndex.structFieldLocations[structDef.name];
            for (const auto &field : structDef.fields)
                fieldLocations[field.name] = {field.sourceUri, field.sourceRange};
        }

        for (const auto &enumDef : semanticModel.enums_view())
            symbolIndex.namedTypeLocations[enumDef.name] = {enumDef.sourceUri, enumDef.sourceRange};
    }
}

bool compile_for_tooling(const TppProject &project,
                         IR &output,
                         std::vector<DiagnosticLSPMessage> &diagnostics,
                         ToolingSymbolIndex &symbolIndex,
                         CompileOptions options) noexcept
{
    compiler::SemanticModel semanticModel;
    const bool success = compile(project, output, diagnostics, options, &semanticModel);
    rebuild_tooling_symbol_index(semanticModel, symbolIndex);
    return success;
}

} // namespace tpp