template ignored(foo: String) 
I shall be ignored.
END

template main(names: list<String>)
@for name in names@
Hello, @name@!
@end for@
END