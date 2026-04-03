template ignored(foo: String) 
I shall be ignored.
END

template main(names: list<String>)
@for name in names | sep="\n"@
Hello, @name@!
@end for@
END