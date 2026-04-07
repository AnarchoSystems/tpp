template main(data: Data)
@for v in data.values | sep=", " policy="escape-lt"@@v | policy="none"@@end for@
END
