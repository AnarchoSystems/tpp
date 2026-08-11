template main(i: Item)
@switch i@
@case A(payload)@A:@payload@@end case@
@case B@B@end case@
@end switch@
END
