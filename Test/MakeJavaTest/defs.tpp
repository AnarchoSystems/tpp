template render_test(defs: JavaTestDef)
public class Test {
    public static void main(String[] argv) {
        try {
            String expected = @defs.expectedOutputLiteral@;
            @for p in defs.params@
            @p.parseLine@
            @end for@
            String actual = Functions.render_main(@defs.callArgs@);
            if (!expected.equals(actual)) {
                System.err.println("EXPECTED:");
                System.err.println(expected);
                System.err.println("ACTUAL:");
                System.err.println(actual);
                System.exit(1);
            }
        } catch (Exception e) {
            e.printStackTrace();
            System.exit(2);
        }
    }
}
END

template render_fragment(defs: JavaTestDef)
class @defs.runnerClassName@ {
    static void run() throws Exception {
        String expected = @defs.expectedOutputLiteral@;
        @for p in defs.params@
        @p.parseLine@
        @end for@
        String actual = @defs.namespaceName@.main(@defs.callArgs@);
        if (!expected.equals(actual)) {
            System.err.println("EXPECTED:");
            System.err.println(expected);
            System.err.println("ACTUAL:");
            System.err.println(actual);
            throw new RuntimeException("mismatch in @defs.testName@");
        }
    }
}
END
