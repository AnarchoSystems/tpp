template main(t: Table)
@for row in t.rows | align sep="\n"@@row.name@ @&@@row.type@ @&@@row.value@@end for@
END
