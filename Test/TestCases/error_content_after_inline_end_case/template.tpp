template main(e: Expr)
@switch e@
@case Num(n)@num: @n@
end num@end case@@case Add(op)@add@end case@
@end switch@
END
