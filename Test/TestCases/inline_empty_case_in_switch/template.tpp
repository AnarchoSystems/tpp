template render_elem(i: int)
@i@ is an integer
END

template render_elem(s: string)
"@s@" is a string
END

template render_elem(b: bool)
@b@ is a boolean
END

template main(item: Item)
@switch item | checkExhaustive@
@render Int via render_elem@
@case String(str)@@end case@
@render Bool via render_elem@
@end switch@
END