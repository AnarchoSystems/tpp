template main(l: List)
@for item in l.items | align="rl" sep="\n"@@item.label@ @&@@item.count@@end for@
END
