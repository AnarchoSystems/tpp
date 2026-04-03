template main(data: Data)
@for item in data.items | precededBy=">" sep=", " followedBy="!"@@item@@end for@
END