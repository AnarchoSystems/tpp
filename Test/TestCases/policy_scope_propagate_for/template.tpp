template main(data: Data)
@for v in data.values | sep=", " policy="escape-lt"@@id(v)@@end for@
END

template id(v: string)
@v@
END