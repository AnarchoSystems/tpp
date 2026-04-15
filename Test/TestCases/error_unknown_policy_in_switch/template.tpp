template main(item: Foo)
@switch item | policy="escape-html"@
@case A(val)@
  A: @val@
@end case@
@case B(val)@
  B: @val@
@end case@
@end switch@
END