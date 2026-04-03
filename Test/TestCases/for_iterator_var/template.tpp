template main(data: Data)
@for item in data.items | iteratorVar=idx sep=", "@[@idx@]=@item@@end for@
END