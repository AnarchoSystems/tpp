template main(data: Data)
@for item in data.items | enumerator=idx sep=", "@[@idx@]=@item@@end for@
END