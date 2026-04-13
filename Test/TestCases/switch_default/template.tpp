template main(item: Item)
@switch item@
@case A(value)@
A:@value@
@end case@
@default@
other
@end case@
@end switch@
END