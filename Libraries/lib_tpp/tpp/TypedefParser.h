#pragma once

#include <tpp/Types.h>
#include <tpp/Tokenizer.h>
#include <tpp/Diagnostic.h>
#include <string>
#include <vector>

namespace tpp::compiler
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
        std::string pendingDoc; // accumulated doc comment text waiting to be attached

        // Returns the current token, transparently skipping Comment/DocComment tokens.
        // DocComment text is accumulated into pendingDoc; Comment resets it.
        const Token &cur();
        const Token &eat(TokKind k);
        bool at(TokKind k);

        // Returns and clears pendingDoc (call after the first at()/eat() that
        // advances past doc comments preceding a declaration).
        std::string takeDoc() { auto d = std::move(pendingDoc); pendingDoc.clear(); return d; }

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
