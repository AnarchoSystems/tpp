struct CppFieldDef
{
    name : string;
    type : TypeKind;
    recursive : bool;
    docComment : optional<string>;
}

struct CppStructDef
{
    name : string;
    fields : list<CppFieldDef>;
    docComment : optional<string>;
    rawTypedefs : string;
}

struct CppVariantDef
{
    tag : string;
    payload : optional<TypeKind>;
    recursive : bool;
    docComment : optional<string>;
}

struct CppEnumDef
{
    name : string;
    variants : list<CppVariantDef>;
    docComment : optional<string>;
    rawTypedefs : string;
}

struct CppFunctionDecl
{
    name : string;
    params : list<ParamDef>;
    docComment : optional<string>;
}