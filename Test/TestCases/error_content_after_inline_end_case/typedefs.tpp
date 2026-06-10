struct BinOp
{
    left : Expr;
    right : Expr;
}

enum Expr
{
    Num(int),
    Add(BinOp),
}
