template main(args: Args)
(@for v in args.values | sep=", "@@v@@end for@)
END