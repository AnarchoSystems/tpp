template render_item(v: string | policy="escape-lt")
@v@
END

template main(data: Data)
@for item in data.values | sep="\n"@@render_item(item)@@end for@
END
