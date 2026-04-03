struct Field
{
    name : string;
    type : string;
    initialValue : optional<string>;
}

struct TypeDef
{
    name  string;
    fields : list<Field>;
}