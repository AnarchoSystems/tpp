template render_test(defs: SwiftTestDef)
import Foundation

// Test: @defs.testName@

let expected = @defs.expectedOutputLiteral@
@for p in defs.params@
@p.parseLine@
@end for@
let actual = render_main(@defs.callArgs@)

if expected != actual {
    fputs("EXPECTED:\n", stderr)
    fputs(expected + "\n", stderr)
    fputs("ACTUAL:\n", stderr)
    fputs(actual + "\n", stderr)
    exit(1)
}
END
