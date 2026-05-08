template main(t: Table)
@for row in t.rows | align sep="\n"@
left
cell@&@@row.value@@end for@
END