template main(data: Data)
@for item in data.items sep=", "@@item@@end for@
END