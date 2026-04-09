// some comment

/**
 * documentation
 */
template foo()

END

/// more documentation
template main()
before
@comment@
This entire block is a comment.
It produces no output.
@end comment@
after
END

// trailing comment