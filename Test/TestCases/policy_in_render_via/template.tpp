template render_str(s: string)
[@s@]
END

template main(data: Data)
@render data.values via render_str | sep=", " policy="escape-lt"@
END
