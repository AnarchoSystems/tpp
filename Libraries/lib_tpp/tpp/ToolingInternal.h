#pragma once

#include "tpp/AST.h"
#include <tpp/Tooling.h>
#include <string>
#include <vector>

namespace tpp
{
    struct ParsedTemplateSource
    {
        std::string name;
        std::vector<compiler::ParamDef> params;
        std::vector<compiler::ASTNode> body;
        std::string policy;
        std::string doc;
        Range sourceRange{};
        size_t bodyStartLine = 0;
        std::string bodyText;
        std::string headerText;
        Range headerRange{};
    };

    bool parseTemplateSource(const std::string &src,
                             std::vector<ParsedTemplateSource> &templates);

    bool parseTemplateSource(const std::string &src,
                             std::vector<ParsedTemplateSource> &templates,
                             std::vector<Diagnostic> &diagnostics);
}