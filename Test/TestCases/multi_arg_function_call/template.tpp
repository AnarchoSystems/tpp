template render_field(name: string, type: string)
@name@: @type@;
END

template main(meta: list<Meta>)
@for m in meta | sep="\n"@
struct @m.name@
{
    @for field in m.fields | sep="\n"@
    @render_field(field.name, field.type)@
    @end for@
}
@end for@
END