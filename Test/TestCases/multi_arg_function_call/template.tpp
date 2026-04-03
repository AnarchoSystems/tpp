template render_field(name: string, type: string)
@name@: @type@
END

template main(meta: Meta)
struct @meta.name@
{
    @for field in meta.fields@
    @render_field(field.name, field.type)@
    @endfor@
}
END