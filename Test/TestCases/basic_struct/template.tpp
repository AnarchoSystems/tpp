template main(def: TypeDef)
struct @def.name@
{
    @for field in def.fields@
    @field.type@ @field.name@@if field.initialValue@ = @field.initialValue@@end if@;
    @end for@
};
END