template render_elem(item: Item)
- @item.name@
END

template main(data: list<Data>)
@render data.items via render_elem | sep="\n"@
END