template render_elem(i: int)
@i@ is an integer
END

template render_elem(s: string)
"@s@" is a string
END

template render_elem(b: bool)
@b@ is a boolean
END

template main(items: list<Item>)
@render items via render_elem | sep="\n"@
END