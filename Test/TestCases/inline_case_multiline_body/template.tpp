template render_expr(e: Expr)
@switch e@
@case Num(n)@num: @n@
end num@end case@
@case Add(op)@add:
@render_expr(op.left)@
@render_expr(op.right)@
end add@end case@
@end switch@
END

template main(e: Expr, items: list<string>, flag: bool)
@render_expr(e)@
@for x in items@item: @x@
done @x@@end for@
after for
@if flag@yes
more yes@end if@
after if
END
