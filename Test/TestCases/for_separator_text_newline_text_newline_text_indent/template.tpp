template main(data: Data)
{
    @for item in data.items | sep=" +\nthen\nfinally "@@item@@end for@
}
END
