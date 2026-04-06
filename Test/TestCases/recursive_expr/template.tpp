template render_expr(e: Expr)
@switch e@
@case Num(n)@
@n@
@end case@
@case Add(op)@
(@render_expr(op.left)@ + @render_expr(op.right)@)
@end case@
@case Mul(op)@
(@render_expr(op.left)@ * @render_expr(op.right)@)
@end case@
@end switch@
END

template main(root: Expr)
@render_expr(root)@
END