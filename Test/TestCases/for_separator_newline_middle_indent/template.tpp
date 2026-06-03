template main(data: Data)
{
    @for item in data.items | sep=",\nthen "@@item@@end for@
}
END
