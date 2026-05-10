template emit_java_policy_runtime()
class TppPolicy {
    final String tag;
    Integer minLength, maxLength;
    static class RejectRule { java.util.regex.Pattern pattern; String message; }
    RejectRule rejectIf;
    static class RequireStep { java.util.regex.Pattern pattern; String replace; }
    java.util.List<RequireStep> require = new java.util.ArrayList<>();
    String[][] replacements;
    java.util.regex.Pattern[] outputFilter;
    TppPolicy(String tag) { this.tag = tag; }
    String apply(String value) throws Exception {
        String v = value;
        if (minLength != null && v.length() < minLength)
            throw new Exception("[policy " + tag + "] value is below minimum length of " + minLength);
        if (maxLength != null && v.length() > maxLength)
            throw new Exception("[policy " + tag + "] value exceeds maximum length of " + maxLength);
        if (rejectIf != null && rejectIf.pattern.matcher(v).find())
            throw new Exception("[policy " + tag + "] " + rejectIf.message);
        for (RequireStep step : require) {
            if (!step.pattern.matcher(v).find())
                throw new Exception("[policy " + tag + "] value does not match required pattern");
            if (step.replace != null)
                v = step.pattern.matcher(v).replaceAll(step.replace);
        }
        if (replacements != null) {
            for (String[] r : replacements)
                v = v.replace(r[0], r[1]);
        }
        if (outputFilter != null) {
            for (java.util.regex.Pattern p : outputFilter) {
                if (!p.matcher(v).matches())
                    throw new Exception("[policy " + tag + "] output does not match required filter");
            }
        }
        return v;
    }

    static final TppPolicy pure = new TppPolicy("");
}
END

template emit_java_writer_runtime()
static class TppWriter {
    enum OutputPostProcessing {
        None,
        StripSingleTrailingNewline
    }

    static final class CaptureResult {
        final String text;
        final boolean topLevelIndented;

        CaptureResult(String text, boolean topLevelIndented) {
            this.text = text;
            this.topLevelIndented = topLevelIndented;
        }
    }

    private static final class OutputFrame {
        final java.util.ArrayList<String> lines = new java.util.ArrayList<>();
        final StringBuilder currentLine = new StringBuilder();
        int firstLineBaseColumn = 0;
        boolean trailingNewline = false;
        boolean sawAnyOutput = false;
        boolean topLevelIndentedOnly = false;

        void append(String text) {
            for (int i = 0; i < text.length(); ++i) {
                char ch = text.charAt(i);
                if (ch == '\n') {
                    lines.add(currentLine.toString());
                    currentLine.setLength(0);
                    trailingNewline = true;
                } else {
                    currentLine.append(ch);
                    trailingNewline = false;
                }
            }
        }

        boolean isEmpty() {
            return lines.isEmpty() && currentLine.length() == 0 && !trailingNewline;
        }

        int currentColumn() {
            if (trailingNewline)
                return 0;
            int baseColumn = lines.isEmpty() ? firstLineBaseColumn : 0;
            return baseColumn + currentLine.length();
        }

        void notePlainOutput(String text) {
            if (text.isEmpty())
                return;
            sawAnyOutput = true;
            topLevelIndentedOnly = false;
        }

        void noteIndentedOutput(String text) {
            if (text.isEmpty())
                return;
            if (!sawAnyOutput) {
                sawAnyOutput = true;
                topLevelIndentedOnly = true;
                return;
            }
            sawAnyOutput = true;
            topLevelIndentedOnly = false;
        }
    }

    private final java.util.ArrayList<OutputFrame> outputStack = new java.util.ArrayList<>();
    private final java.util.ArrayList<Integer> capturedBlockColumnStack = new java.util.ArrayList<>();
    private String error = "";

    TppWriter() {
        outputStack.add(new OutputFrame());
    }

    boolean emit(String text) {
        output(text);
        return true;
    }

    boolean emitValue(String value) {
        emitWithMultilineIndent(value);
        return true;
    }

    boolean emitValue(String value, TppPolicy policy) {
        try {
            value = policy.apply(value);
        } catch (Exception e) {
            error = e.getMessage();
            return false;
        }
        emitWithMultilineIndent(value);
        return true;
    }

    boolean beginCapturedBlock() {
        return beginCapturedBlock(0);
    }

    boolean beginCapturedBlock(int blockIndentInParentBlock) {
        int outputColumn = Math.max(activeFrame().currentColumn(), blockIndentInParentBlock);
        capturedBlockColumnStack.add(outputColumn);
        beginCapture();
        return true;
    }

    boolean emitCapturedBlock() {
        if (capturedBlockColumnStack.isEmpty()) {
            error = "emitCapturedBlock: scope stack underflow";
            return false;
        }
        int outputColumn = capturedBlockColumnStack.remove(capturedBlockColumnStack.size() - 1);
        return emitIndentedFrame(endCaptureFrame(), outputColumn);
    }

    void beginCapture() {
        OutputFrame frame = new OutputFrame();
        if (!outputStack.isEmpty())
            frame.firstLineBaseColumn = activeFrame().currentColumn();
        outputStack.add(frame);
    }

    String endCapture() {
        return serializeFrame(endCaptureFrame());
    }

    CaptureResult endCaptureResult() {
        OutputFrame captured = endCaptureFrame();
        return new CaptureResult(serializeFrame(captured), captured.topLevelIndentedOnly);
    }

    String takeOutput() {
        return takeOutput(OutputPostProcessing.None);
    }

    String takeOutput(OutputPostProcessing postProcessing) {
        String output = outputStack.isEmpty() ? "" : serializeFrame(outputStack.get(0));
        if (postProcessing == OutputPostProcessing.StripSingleTrailingNewline &&
            !output.isEmpty() &&
            output.charAt(output.length() - 1) == '\n') {
            output = output.substring(0, output.length() - 1);
        }
        return output;
    }

    String error() {
        return error;
    }

    boolean hasError() {
        return !error.isEmpty();
    }

    static boolean containsLineBreak(String text) {
        return text.indexOf('\r') >= 0 || text.indexOf('\n') >= 0;
    }

    static String padCell(String text, int width, char spec) {
        if (text.length() >= width)
            return text;
        int pad = width - text.length();
        if (spec == 'r')
            return " ".repeat(pad) + text;
        if (spec == 'c') {
            int left = pad / 2;
            return " ".repeat(left) + text + " ".repeat(pad - left);
        }
        return text + " ".repeat(pad);
    }

    private static String serializeFrame(OutputFrame frame) {
        if (frame.isEmpty())
            return "";
        StringBuilder result = new StringBuilder();
        for (String line : frame.lines) {
            result.append(line).append('\n');
        }
        result.append(frame.currentLine);
        return result.toString();
    }

    private static boolean isTrimmedEmpty(String text) {
        for (int i = 0; i < text.length(); ++i) {
            char ch = text.charAt(i);
            if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
                return false;
        }
        return true;
    }

    private static int sharedIndentWidth(String line) {
        int width = 0;
        while (width < line.length() && (line.charAt(width) == ' ' || line.charAt(width) == '\t'))
            ++width;
        return width;
    }

    private static String stripSharedIndent(String line, int indentWidth) {
        int width = 0;
        while (width < line.length() && width < indentWidth &&
               (line.charAt(width) == ' ' || line.charAt(width) == '\t')) {
            ++width;
        }
        return line.substring(width);
    }

    private static String renderIndentedFrame(OutputFrame frame, int indentColumns) {
        if (frame.isEmpty())
            return "";

        java.util.ArrayList<String> lines = new java.util.ArrayList<>(frame.lines);
        java.util.ArrayList<Boolean> terminated = new java.util.ArrayList<>();
        for (int i = 0; i < frame.lines.size(); ++i)
            terminated.add(true);
        if (!frame.trailingNewline && (!frame.lines.isEmpty() || frame.currentLine.length() > 0)) {
            lines.add(frame.currentLine.toString());
            terminated.add(false);
        }

        int first = 0;
        while (first < lines.size() && isTrimmedEmpty(lines.get(first)))
            ++first;

        int pastLast = lines.size();
        while (pastLast > first && isTrimmedEmpty(lines.get(pastLast - 1)))
            --pastLast;

        if (first == pastLast)
            return "";

        int minSharedIndent = 0;
        boolean sawNonEmpty = false;
        for (int index = first; index < pastLast; ++index) {
            if (isTrimmedEmpty(lines.get(index)))
                continue;
            int width = sharedIndentWidth(lines.get(index));
            if (!sawNonEmpty || width < minSharedIndent)
                minSharedIndent = width;
            sawNonEmpty = true;
        }

        String indent = indentColumns > 0 ? " ".repeat(indentColumns) : "";
        int firstLineIndent = Math.max(indentColumns - frame.firstLineBaseColumn, 0);
        StringBuilder result = new StringBuilder();

        for (int index = first; index < pastLast; ++index) {
            String currentLine = stripSharedIndent(lines.get(index), minSharedIndent);
            if (!currentLine.isEmpty()) {
                if (index == first && firstLineIndent > 0)
                    result.append(" ".repeat(firstLineIndent));
                else if (index != first)
                    result.append(indent);
                result.append(currentLine);
            }
            if (terminated.get(index))
                result.append('\n');
        }
        return result.toString();
    }

    private OutputFrame endCaptureFrame() {
        return outputStack.remove(outputStack.size() - 1);
    }

    private boolean emitIndentedFrame(OutputFrame frame, int indentColumns) {
        if (frame.isEmpty())
            return true;
        String rendered = renderIndentedFrame(frame, indentColumns);
        OutputFrame out = activeFrame();
        out.noteIndentedOutput(rendered);
        out.append(rendered);
        return true;
    }

    private OutputFrame activeFrame() {
        return outputStack.get(outputStack.size() - 1);
    }

    private void output(String text) {
        OutputFrame out = activeFrame();
        out.notePlainOutput(text);
        out.append(text);
    }

    private void emitWithMultilineIndent(String text) {
        int column = activeFrame().currentColumn();
        if (column > 0 && text.indexOf('\n') >= 0) {
            String pad = " ".repeat(column);
            StringBuilder indented = new StringBuilder();
            int start = 0;
            while (true) {
                int end = text.indexOf('\n', start);
                if (start > 0)
                    indented.append(pad);
                if (end < 0) {
                    indented.append(text.substring(start));
                    break;
                }
                indented.append(text, start, end).append('\n');
                start = end + 1;
            }
            output(indented.toString());
        } else {
            output(text);
        }
    }
}
END

template render_java_shared_runtime()
// Generated by tpp2java — shared runtime helpers.

@emit_java_policy_runtime()@

class TppRuntime {
    @emit_java_writer_runtime()@
}
END

template render_java_runtime()
@render_java_shared_runtime()@
END