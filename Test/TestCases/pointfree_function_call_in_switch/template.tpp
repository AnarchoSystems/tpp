template render_integer(i: int)
@i@ is an integer
END

template render_string(s: string)
"@s@" is a string
END

template render_bool(b: bool)
@b@ is a boolean
END

template main(items: list<Item>)
@for item in items | sep="\n"@
@switch item | checkExhaustive@
@render Integer via render_integer@
@render Str via render_string@
@render Bool via render_bool@
@end switch@
@end for@
END