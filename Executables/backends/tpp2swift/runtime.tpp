template emit_swift_policy_runtime()
struct TppPolicy {
    let tag: String
    var minLength: Int? = nil
    var maxLength: Int? = nil
    struct RejectRule { let pattern: Regex<AnyRegexOutput>; let message: String }
    var rejectIf: RejectRule? = nil
    struct RequireStep { let pattern: Regex<AnyRegexOutput>; let replace: String? }
    var require: [RequireStep] = []
    var replacements: [(String, String)] = []
    var outputFilter: [Regex<AnyRegexOutput>] = []
    static let pure = TppPolicy(tag: "")

    static func _literalReplace(_ s: String, _ find: String, _ repl: String) -> String {
        guard !find.isEmpty else { return s }
        var result = ""
        var current = s.startIndex
        while current < s.endIndex {
            if s[current...].hasPrefix(find) {
                result += repl
                current = s.index(current, offsetBy: find.count)
            } else {
                result.append(s[current])
                current = s.index(after: current)
            }
        }
        return result
    }

    func apply(_ value: String) throws -> String {
        var v = value
        if let minLength = minLength, v.count < minLength {
            throw TppPolicyError.violation("[policy \(tag)] value is below minimum length of \(minLength)")
        }
        if let maxLength = maxLength, v.count > maxLength {
            throw TppPolicyError.violation("[policy \(tag)] value exceeds maximum length of \(maxLength)")
        }
        if let rj = rejectIf {
            if v.firstMatch(of: rj.pattern) != nil {
                throw TppPolicyError.violation("[policy \(tag)] \(rj.message)")
            }
        }
        for step in require {
            if v.firstMatch(of: step.pattern) == nil {
                throw TppPolicyError.violation("[policy \(tag)] value does not match required pattern")
            }
            if let repl = step.replace {
                v = v.replacing(step.pattern) { match in
                    var result = repl
                    for i in stride(from: match.output.count - 1, through: 0, by: -1) {
                        if let sub = match.output[i].substring {
                            result = TppPolicy._literalReplace(result, "$\(i)", String(sub))
                        }
                    }
                    return result
                }
            }
        }
        for (find, repl) in replacements {
            v = TppPolicy._literalReplace(v, find, repl)
        }
        for p in outputFilter {
            if v.wholeMatch(of: p) == nil {
                throw TppPolicyError.violation("[policy \(tag)] output does not match required filter")
            }
        }
        return v
    }
}
END

template emit_swift_writer_runtime()
final class TppWriter {
    enum OutputPostProcessing {
        case none
        case stripSingleTrailingNewline
    }

    struct CaptureResult {
        let text: String
        let topLevelIndented: Bool
    }

    final class OutputFrame {
        var lines: [String] = []
        var currentLine = ""
        var firstLineBaseColumn = 0
        var trailingNewline = false
        var sawAnyOutput = false
        var topLevelIndentedOnly = false

        func append(_ text: String) {
            for ch in text {
                if ch == "\n" {
                    lines.append(currentLine)
                    currentLine = ""
                    trailingNewline = true
                } else {
                    currentLine.append(ch)
                    trailingNewline = false
                }
            }
        }

        var isEmpty: Bool {
            lines.isEmpty && currentLine.isEmpty && !trailingNewline
        }

        var currentColumn: Int {
            if trailingNewline { return 0 }
            let baseColumn = lines.isEmpty ? firstLineBaseColumn : 0
            return baseColumn + currentLine.count
        }

        func notePlainOutput(_ text: String) {
            guard !text.isEmpty else { return }
            sawAnyOutput = true
            topLevelIndentedOnly = false
        }

        func noteIndentedOutput(_ text: String) {
            guard !text.isEmpty else { return }
            if !sawAnyOutput {
                sawAnyOutput = true
                topLevelIndentedOnly = true
                return
            }
            sawAnyOutput = true
            topLevelIndentedOnly = false
        }
    }

    private var outputStack: [OutputFrame] = [OutputFrame()]
    private var capturedBlockColumnStack: [Int] = []
    private(set) var error = ""

    var hasError: Bool { !error.isEmpty }

    func emit(_ text: String) {
        output(text)
    }

    func emitValue(_ value: String) {
        emitWithMultilineIndent(value)
    }

    func emitValue(_ value: String, _ policy: TppPolicy) throws {
        emitWithMultilineIndent(try policy.apply(value))
    }

    func beginCapturedBlock() {
        beginCapturedBlock(0)
    }

    func beginCapturedBlock(_ blockIndentInParentBlock: Int) {
        let outputColumn = max(activeFrame().currentColumn, blockIndentInParentBlock)
        capturedBlockColumnStack.append(outputColumn)
        beginCapture()
    }

    func emitCapturedBlock() {
        guard !capturedBlockColumnStack.isEmpty else { fatalError("emitCapturedBlock: scope stack underflow") }
        let outputColumn = capturedBlockColumnStack.removeLast()
        _ = emitIndentedFrame(endCaptureFrame(), outputColumn)
    }

    func beginCapture() {
        let frame = OutputFrame()
        if !outputStack.isEmpty {
            frame.firstLineBaseColumn = activeFrame().currentColumn
        }
        outputStack.append(frame)
    }

    func endCapture() -> String {
        Self.serializeFrame(endCaptureFrame())
    }

    func endCaptureResult() -> CaptureResult {
        let captured = endCaptureFrame()
        return CaptureResult(text: Self.serializeFrame(captured), topLevelIndented: captured.topLevelIndentedOnly)
    }

    func takeOutput(_ postProcessing: OutputPostProcessing = .none) -> String {
        var output = outputStack.first.map(Self.serializeFrame) ?? ""
        if postProcessing == .stripSingleTrailingNewline && output.hasSuffix("\n") {
            output.removeLast()
        }
        return output
    }

    static func containsLineBreak(_ text: String) -> Bool {
        text.contains("\n") || text.contains("\r")
    }

    static func padCell(_ s: String, _ width: Int, _ spec: Character) -> String {
        if s.count >= width { return s }
        let pad = width - s.count
        if spec == "r" { return String(repeating: " ", count: pad) + s }
        if spec == "c" {
            let left = pad / 2
            return String(repeating: " ", count: left) + s + String(repeating: " ", count: pad - left)
        }
        return s + String(repeating: " ", count: pad)
    }

    private static func serializeFrame(_ frame: OutputFrame) -> String {
        guard !frame.isEmpty else { return "" }
        var result = ""
        for line in frame.lines {
            result += line
            result += "\n"
        }
        result += frame.currentLine
        return result
    }

    private static func isTrimmedEmpty(_ text: String) -> Bool {
        !text.contains { ch in ch != " " && ch != "\t" && ch != "\r" && ch != "\n" }
    }

    private static func sharedIndentWidth(_ line: String) -> Int {
        var width = 0
        while width < line.count {
            let index = line.index(line.startIndex, offsetBy: width)
            let ch = line[index]
            if ch != " " && ch != "\t" { break }
            width += 1
        }
        return width
    }

    private static func stripSharedIndent(_ line: String, _ indentWidth: Int) -> String {
        var width = 0
        while width < line.count && width < indentWidth {
            let index = line.index(line.startIndex, offsetBy: width)
            let ch = line[index]
            if ch != " " && ch != "\t" { break }
            width += 1
        }
        return String(line.dropFirst(width))
    }

    private static func renderIndentedFrame(_ frame: OutputFrame, _ indentColumns: Int) -> String {
        guard !frame.isEmpty else { return "" }

        var logicalLines = frame.lines.map { ($0, true) }
        if !frame.trailingNewline && (!frame.lines.isEmpty || !frame.currentLine.isEmpty) {
            logicalLines.append((frame.currentLine, false))
        }

        var first = 0
        while first < logicalLines.count && isTrimmedEmpty(logicalLines[first].0) {
            first += 1
        }

        var pastLast = logicalLines.count
        while pastLast > first && isTrimmedEmpty(logicalLines[pastLast - 1].0) {
            pastLast -= 1
        }

        if first == pastLast { return "" }

        var minSharedIndent = 0
        var sawNonEmpty = false
        for index in first..<pastLast {
            let line = logicalLines[index].0
            if isTrimmedEmpty(line) { continue }
            let width = sharedIndentWidth(line)
            if !sawNonEmpty || width < minSharedIndent {
                minSharedIndent = width
            }
            sawNonEmpty = true
        }

        let indent = indentColumns > 0 ? String(repeating: " ", count: indentColumns) : ""
        let firstLineIndent = max(indentColumns - frame.firstLineBaseColumn, 0)
        var result = ""

        for index in first..<pastLast {
            let (line, terminated) = logicalLines[index]
            let currentLine = stripSharedIndent(line, minSharedIndent)
            if !currentLine.isEmpty {
                if index == first && firstLineIndent > 0 {
                    result += String(repeating: " ", count: firstLineIndent)
                } else if index != first {
                    result += indent
                }
                result += currentLine
            }
            if terminated {
                result += "\n"
            }
        }
        return result
    }

    private func endCaptureFrame() -> OutputFrame {
        outputStack.removeLast()
    }

    private func emitIndentedFrame(_ frame: OutputFrame, _ indentColumns: Int) -> Bool {
        guard !frame.isEmpty else { return true }
        let rendered = Self.renderIndentedFrame(frame, indentColumns)
        let out = activeFrame()
        out.noteIndentedOutput(rendered)
        out.append(rendered)
        return true
    }

    private func activeFrame() -> OutputFrame {
        outputStack[outputStack.count - 1]
    }

    private func output(_ text: String) {
        let out = activeFrame()
        out.notePlainOutput(text)
        out.append(text)
    }

    private func emitWithMultilineIndent(_ text: String) {
        let column = activeFrame().currentColumn
        guard column > 0, text.contains("\n") else {
            output(text)
            return
        }

        let pad = String(repeating: " ", count: column)
        var indented = ""
        var start = text.startIndex
        var first = true
        while start < text.endIndex {
            if let nlIdx = text[start...].firstIndex(of: "\n") {
                if !first { indented += pad }
                indented += String(text[start..<nlIdx])
                indented += "\n"
                start = text.index(after: nlIdx)
                first = false
            } else {
                if !first { indented += pad }
                indented += String(text[start...])
                break
            }
        }
        output(indented)
    }
}
END

template render_swift_shared_runtime()
// Generated by tpp2swift — shared runtime helpers.

import Foundation

final class Box<T: Codable>: Codable {
    let value: T
    init(_ value: T) { self.value = value }
    required init(from decoder: Decoder) throws {
        value = try T(from: decoder)
    }
    func encode(to encoder: Encoder) throws {
        try value.encode(to: encoder)
    }
}

enum TppPolicyError: Error, CustomStringConvertible {
    case violation(String)
    var description: String {
        switch self { case .violation(let msg): return msg }
    }
}

@emit_swift_policy_runtime()@

@emit_swift_writer_runtime()@
END

template render_swift_runtime()
@render_swift_shared_runtime()@
END