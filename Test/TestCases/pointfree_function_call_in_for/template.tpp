template render_elem(elem: Item)
foo @elem.value@ bar
END

template main(list: list<Item>)
@render list via render_elem | sep="\n"@
END