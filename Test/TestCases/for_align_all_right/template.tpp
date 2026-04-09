template main(t: Table)
@for row in t.rows | align="r" sep="\n"@@row.name@ @&@@row.count@@end for@
END
