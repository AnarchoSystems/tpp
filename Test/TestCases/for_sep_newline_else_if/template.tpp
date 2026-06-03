template main(data: Data)
    @for cond in data.conditions | sep="\nelse "@
    if (@cond.pred@) {@cond.statement@}
    @end for@
END
