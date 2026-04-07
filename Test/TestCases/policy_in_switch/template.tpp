template main(item: Tag)
@switch item | policy="escape-lt"@
@case A(val)@
A: @val@
@end case@
@case B(val)@
B: @val@
@end case@
@end switch@
END
