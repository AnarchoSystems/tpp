template render_elem(elem: Item)
foo @elem.value@ bar
END

template main(list: list<Item>)
@for elem in list | sep="\n"@
    @render_elem(elem)@
@end for@
END