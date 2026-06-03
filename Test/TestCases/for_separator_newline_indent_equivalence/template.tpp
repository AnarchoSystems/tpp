template render_enum_multiline(e: EnumData)
enum E@e.name@
{
   @for item in e.cases | sep=",\n"@
   @e.prefix@@item@
   @end for@
};
END

template render_enum_inline(e: EnumData)
enum E@e.name@
{
   @for item in e.cases | sep=",\n"@@e.prefix@@item@@end for@
};
END

template main(input: Input)
@render_enum_multiline(input.e)@

@render_enum_inline(input.e)@
END
