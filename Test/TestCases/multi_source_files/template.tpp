template main(items: list<string>)
@for item in items | sep="\n"@@render_item(item)@@end for@
END
