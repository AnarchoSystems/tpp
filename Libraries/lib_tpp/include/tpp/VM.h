#pragma once

#include <tpp/IR.h>
#include <tpp/Layout.h>
#include <tpp/Slot.h>

#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // Compiled policy — pre-built from PolicyDef at VM construction time
    // ════════════════════════════════════════════════════════════════════════════

    struct VMCompiledRegexStep
    {
        std::regex rx;
        std::string replace;
        std::string compiled_replace; // @subexpr_N@ → $N
    };

    struct VMCompiledPolicy
    {
        std::string tag;
        std::optional<PolicyLength> length;

        struct RejectIf
        {
            std::regex rx;
            std::string message;
        };
        std::optional<RejectIf> rejectIf;

        std::vector<VMCompiledRegexStep> require;
        std::vector<PolicyReplacement> replacements;
        std::vector<std::regex> outputFilter;
    };

    // ════════════════════════════════════════════════════════════════════════════
    // VM — low-level slot-based template execution engine
    //
    // Operates on flat Slot frames produced by DataLoader.  Executes IR
    // instructions (emit, for, if, switch, call) against those frames,
    // handling indentation, alignment, and policy enforcement.
    //
    // All methods except takeOutput()/error() return bool (false = error).
    // Capture is internal via an output stack.
    // ════════════════════════════════════════════════════════════════════════════

    class VM
    {
    public:
        VM(const LayoutTable &layouts, std::map<std::string, VMCompiledPolicy> policies = {});

        // ── Literal output ──────────────────────────────────────────────

        /// Append literal text to the active output buffer.
        bool emit(const std::string &text);

        /// Read the slot at fp+offset, convert to string, and emit.
        bool emitSlot(int offset);

        /// emitSlot + apply the effective policy (expr-level or scope).
        bool emitSlotWithPolicy(int offset, const std::string &exprPolicy);

        // ── Control flow ────────────────────────────────────────────────

        /// Execute a sequence of instructions.
        bool execBody(const std::vector<Instruction> &instrs);

        /// Capture body output, then apply block indentation.
        bool execBlockBody(const std::vector<Instruction> &instrs, int insertCol);

        /// For loop: iterate a list slot, executing body per element.
        bool execFor(const ForInstr &instr);

        /// Conditional: evaluate condition slot, branch.
        bool execIf(const IfInstr &instr);

        /// Switch on a variant tag slot, dispatch to matching case.
        bool execSwitch(const SwitchInstr &instr);

        // ── Policy scope ────────────────────────────────────────────────

        bool pushPolicy(const std::string &tag);
        bool popPolicy();

        // ── Frame management ────────────────────────────────────────────

        /// Push slots onto the stack and set fp to the start of the new frame.
        void pushFrame(const std::vector<Slot> &slots);

        /// Pop the topmost frame (restores fp from fpStack).
        void popFrame();

        /// Direct access to a slot at fp+offset.
        const Slot &slot(int offset) const;

        /// Mutable access to a slot at fp+offset.
        Slot &slotMut(int offset);

        /// Current frame pointer.
        int framePointer() const { return fp_; }

        /// Current stack size.
        int stackSize() const { return static_cast<int>(stack_.size()); }

        /// Compile all policies from the IR into a policy map for the constructor.
        static std::map<std::string, VMCompiledPolicy> compilePolicies(const IR &ir);

        // ── Result / diagnostics ────────────────────────────────────────

        /// Move the root output buffer out.
        std::string takeOutput();

        /// Reference to the error string.
        std::string &error();

        /// Check whether an error has occurred.
        bool hasError() const { return !error_.empty(); }

    private:
        // ── Immutable (set at construction) ─────────────────────────────
        const LayoutTable &layouts_;
        std::map<std::string, VMCompiledPolicy> compiledPolicies_;

        // ── Frame stack ─────────────────────────────────────────────────
        std::vector<Slot> stack_;
        int fp_ = 0;
        std::vector<int> fpStack_;

        // ── Output ──────────────────────────────────────────────────────
        std::vector<std::string> outputStack_; // back() is active buffer
        std::string error_;

        // ── Policy scope ────────────────────────────────────────────────
        std::string activePolicy_;
        const VMCompiledPolicy *activePolicyPtr_ = nullptr;
        std::vector<std::string> policyScopeStack_;

        // ── Internal helpers ────────────────────────────────────────────

        /// Write to the active output buffer.
        void output(const std::string &text);

        /// Active output buffer reference.
        std::string &activeOutput();

        /// Push a new capture buffer.
        void pushCapture();

        /// Pop the top capture buffer and return its content.
        std::string popCapture();

        /// Resolve the effective policy tag for an emission.
        const std::string &effectivePolicy(const std::string &exprPolicy) const;

        /// Look up a compiled policy by tag (nullptr if not found or empty).
        const VMCompiledPolicy *findPolicy(const std::string &tag);

        /// Apply the 5-step policy pipeline to a value.
        bool applyPolicy(const std::string &tag, const VMCompiledPolicy &pol,
                         std::string &value);

        /// Block-indent algorithm: de-indent by zero marker, re-indent by insertCol.
        static std::string blockIndent(const std::string &raw, int insertCol);

        /// Pad a cell for alignment.
        static std::string padCell(const std::string &s, int width, char spec);

        /// Render instructions collecting cells at AlignCell markers.
        std::vector<std::string> execInstructionsCells(const std::vector<Instruction> &instrs);

        /// Execute aligned for-loop.
        bool execForAligned(const ForInstr &instr);

        /// Execute normal (non-aligned) for-loop.
        bool execForNormal(const ForInstr &instr);

        /// Apply multi-line indentation to a string being emitted.
        void emitWithMultilineIndent(const std::string &text);

        /// Compile a single PolicyDef.
        static VMCompiledPolicy compilePolicy(const PolicyDef &def);
    };

} // namespace tpp
