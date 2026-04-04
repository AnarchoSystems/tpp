template main(items: list<string>)
@for item in items | sep="\n"@@item@@end for@
END