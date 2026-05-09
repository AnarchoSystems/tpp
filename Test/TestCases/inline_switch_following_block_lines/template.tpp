template main(input: Input)
@input.prefix@ @switch input.item@
@case String(str)@@input.middle@
@str@
@end case@
@case Bool(value)@
@value@
@end case@
@end switch@
END