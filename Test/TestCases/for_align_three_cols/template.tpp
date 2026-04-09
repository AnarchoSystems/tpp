template main(t: Table)
@for row in t.rows | align="rll" sep="\n"@@row.name@ @&@@row.type@ @&@@row.value@@end for@
END
