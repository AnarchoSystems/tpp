#pragma once

#include <tpp/Types.h>
#include <tpp/Tokenizer.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>

namespace tpp
{
    // Tokenises a typedef source string into tokens.
    std::vector<Token> tokenize_typedefs(const std::string &src);

    // ── Typedef parser ──
    // Parses a typedef source string (structs and enums) into a TypeRegistry.

    struct TypedefParser
    {
        const std::vector<Token> &tokens;
        size_t pos = 0;
        TypeRegistry &reg;
        std::vector<Diagnostic> &diags;
        bool ok = true;

        const Token &cur() const;
        const Token &eat(TokKind k);
        bool at(TokKind k) const;

        TypeRef parseTypeRef();
        void parseStruct();
        void parseEnum();
        bool validateTypes();
        // Returns false and emits diagnostics for any type with no finite minimal JSON value.
        bool computeFiniteTypes();
        // Sets recursive=true on FieldDef/VariantDef entries whose TypeRef references a cyclic NamedType.
        void annotateRecursiveFields();
        void parse();
    };

    // Parses a type string like "string", "list<Item>", "optional<bool>" into a TypeRef.
    TypeRef parseParamType(const std::string &s);
}
