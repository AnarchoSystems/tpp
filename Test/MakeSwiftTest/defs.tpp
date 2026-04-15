template render_test(defs: SwiftTestDef)
import Foundation

// Test: @defs.testName@

let expected = @defs.expectedOutputLiteral@
@for p in defs.params@
@p.parseLine@
@end for@
@if defs.hasPolicies@
let actual = try render_main(@defs.callArgs@)
@else@
let actual = render_main(@defs.callArgs@)
@end if@

if expected != actual {
    fputs("EXPECTED:\n", stderr)
    fputs(expected + "\n", stderr)
    fputs("ACTUAL:\n", stderr)
    fputs(actual + "\n", stderr)
    exit(1)
}
END

template render_fragment(defs: SwiftTestDef)
enum @defs.runnerName@ {
    static func run() throws {
        let expected = @defs.expectedOutputLiteral@
        @for p in defs.params@
        @p.parseLine@
        @end for@
        @if defs.hasPolicies@
        let actual = try @defs.namespaceName@.main(@defs.callArgs@)
        @else@
        let actual = @defs.namespaceName@.main(@defs.callArgs@)
        @end if@

        if expected != actual {
            fputs("EXPECTED:\n", stderr)
            fputs(expected + "\n", stderr)
            fputs("ACTUAL:\n", stderr)
            fputs(actual + "\n", stderr)
            throw NSError(domain: "tpp", code: 1, userInfo: [NSLocalizedDescriptionKey: "mismatch in @defs.testName@"])
        }
    }
}
END
