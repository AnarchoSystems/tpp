template main(items: list<Item>)
@for item in items | sep="\n"@- @item.name@ (@item.count@)@if item.note@ — @item.note@@end if@@end for@
END