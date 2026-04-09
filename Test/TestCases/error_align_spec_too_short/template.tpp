template main(t: Table)
@for row in t.rows | align="rl" sep="\n"@@row.a@ @&@@row.b@ @&@@row.c@@end for@
END
