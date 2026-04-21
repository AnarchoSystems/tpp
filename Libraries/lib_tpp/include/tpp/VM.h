#pragma once

#include <tpp/IR.h>
#include <tpp/Layout.h>
#include <tpp/Slot.h>

#include <map>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace tpp
{

    struct TppPolicy;

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

        // ── Function table ──────────────────────────────────────────────

        /// Set the function table for CallInstr dispatch.
        void setFunctions(const std::vector<FunctionDef> *functions);

        // ── Literal output ──────────────────────────────────────────────

        /// Append literal text to the active output buffer.
        bool emit(std::string_view text);

        /// Read the slot at fp+offset, convert to string, and emit.
        bool emitSlot(int offset);

        /// emitSlot + apply the effective policy (expr-level or scope).
        bool emitSlotWithPolicy(int offset, const std::string &exprPolicy);

        struct NativeLoopOptions
        {
            std::optional<std::string_view> precededBy;
            std::optional<std::string_view> sep;
            std::optional<std::string_view> followedBy;
            bool stripSingleTrailingNewline = false;
        };

        /// Emit an already-materialized native value with multiline indentation.
        bool emitValue(const std::string &value);

        /// Apply a generated native policy, then emit with multiline indentation.
        bool emitValue(std::string value, const TppPolicy &policy);

        /// Capture a block body, indent it, and append it to the active output.
        template <typename Body>
        bool emitBlock(int insertCol, Body &&body);

        /// Iterate a native collection and let the VM handle separators and trailers.
        template <typename Collection, typename Body>
        bool emitForEach(const Collection &collection,
                         const NativeLoopOptions &options,
                         Body &&body);

        /// Iterate a native collection rendering block bodies with block indentation.
        template <typename Collection, typename Body>
        bool emitBlockForEach(const Collection &collection,
                              int insertCol,
                              const NativeLoopOptions &options,
                              Body &&body);

        /// Capture one aligned cell into the active aligned-row session.
        template <typename Body>
        bool captureAlignedCell(size_t cellIndex, Body &&body);

        /// Iterate a native collection rendering aligned rows with VM-owned buffers.
        template <typename Collection, typename Body>
        bool emitAlignedForEach(const Collection &collection,
                                size_t numCols,
                                const std::vector<char> &alignSpecChars,
                                bool singleAlignChar,
                                const NativeLoopOptions &options,
                                Body &&body);

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

        /// Begin capturing into a nested output buffer.
        void beginCapture();

        /// End the current capture and return its raw contents.
        std::string endCapture();

        /// End the current capture, then block-indent its contents.
        std::string endBlockCapture(int insertCol);

        /// Begin a for-each session over a collection of the given size.
        void beginForEach(size_t size);

        /// Advance the active for-each session. Returns true while an item is active.
        bool nextForEach();

        /// Index of the active for-each item.
        size_t forEachIndex() const;

        /// Whether the active for-each item has a successor.
        bool forEachHasNext() const;

        /// Enumerator value for the active for-each item.
        int forEachEnumerator() const;

        /// End the active for-each session.
        void endForEach();

        /// Begin an aligned-for rendering session.
        void beginFor(size_t numCols,
                      const std::vector<char> &alignSpecChars,
                      bool singleAlignChar,
                      size_t rowReserve = 0);

        /// Feed one logical row into the active aligned-for session.
        void exec(const std::vector<std::string> &row);

        /// Render one row from the active aligned-for session.
        std::string endFor(size_t rowIndex) const;

        /// Number of rows collected in the active aligned-for session.
        size_t forSize() const;

        /// End the active aligned-for session.
        void finishFor();

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
        const std::vector<FunctionDef> *functions_ = nullptr;

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

        struct ForState
        {
            std::vector<std::vector<std::string>> rows;
            std::vector<int> colWidth;
            std::vector<char> colSpec;
            std::vector<std::string> currentRow;
            size_t numCols = 0;
            bool rowOpen = false;
        };

        struct ForEachState
        {
            size_t size = 0;
            size_t index = 0;
            bool started = false;
        };

        std::vector<ForState> forStack_;
        std::vector<ForEachState> forEachStack_;

        // ── Internal helpers ────────────────────────────────────────────

        /// Write to the active output buffer.
        void output(std::string_view text);

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

        /// Render one aligned row using precomputed widths/spec.
        static std::string renderAlignedRow(const std::vector<std::string> &row,
                            const std::vector<int> &colWidth,
                            const std::vector<char> &colSpec,
                            size_t numCols);

        /// Emit an optional loop delimiter if present.
        bool emitOptionalText(const std::optional<std::string_view> &text);

        /// Remove a single trailing newline from a block iteration when required.
        static bool stripSingleTrailingNewline(std::string &text);

        /// Active aligned-for session.
        ForState &activeForState();

        /// Active aligned-for session.
        const ForState &activeForState() const;

        /// Begin collecting one aligned row into the active for session.
        bool startAlignedRow();

        /// Commit the active aligned row to the current for session.
        bool finishAlignedRow();

        /// Abort the active aligned row after an exception/error.
        void abortAlignedRow();

        /// Render instructions collecting cells at AlignCell markers.
        std::vector<std::string> execInstructionsCells(const std::vector<Instruction> &instrs);

        /// Execute aligned for-loop.
        bool execForAligned(const ForInstr &instr);

        /// Execute normal (non-aligned) for-loop.
        bool execForNormal(const ForInstr &instr);

        /// Execute a CallInstr: push callee frame, execute callee body, pop.
        bool execCall(const CallInstr &instr);

        /// Apply multi-line indentation to a string being emitted.
        void emitWithMultilineIndent(const std::string &text);

        /// Dereference a slot: if it's a BoxRef, return a pointer to the
        /// first slot in the referenced sub-frame. Otherwise return
        /// a pointer to the slot itself. The returned pointer stays valid
        /// as long as the frame/box is alive.
        const Slot *derefSlot(int offset) const;

        /// Resolve a slot, handling recursive (BoxRef) and optional fields
        /// correctly. When isRecursive is true, the BoxRef is dereferenced
        /// first and the optional skip happens inside the box. When false,
        /// the optional skip happens in the current frame before deref.
        const Slot *resolveSlot(int offset, bool isOptional, bool isRecursive) const;

        /// Serialize slots back to JSON using type information.
        /// Used when emitting complex types (struct, enum, list).
        std::string slotToJsonString(const Slot *base, const TypeKind &type) const;

        /// Compile a single PolicyDef.
        static VMCompiledPolicy compilePolicy(const PolicyDef &def);
    };

    template <typename Body>
    bool VM::emitBlock(int insertCol, Body &&body)
    {
        pushCapture();
        try
        {
            body();
        }
        catch (...)
        {
            popCapture();
            throw;
        }

        if (hasError())
        {
            popCapture();
            return false;
        }

        std::string raw = popCapture();
        if (!raw.empty())
            output(blockIndent(raw, insertCol));
        return true;
    }

    template <typename Collection, typename Body>
    bool VM::emitForEach(const Collection &collection,
                         const NativeLoopOptions &options,
                         Body &&body)
    {
        beginForEach(collection.size());
        struct Guard
        {
            VM &vm;
            bool active = true;
            ~Guard()
            {
                if (active)
                    vm.endForEach();
            }
        } guard{*this};

        while (nextForEach())
        {
            const auto index = forEachIndex();
            if (!emitOptionalText(options.precededBy))
                return false;

            body(collection[index], forEachEnumerator());
            if (hasError())
                return false;

            if (forEachHasNext())
            {
                if (!emitOptionalText(options.sep))
                    return false;
            }
            else if (!options.followedBy.has_value() || collection.empty())
            {
                continue;
            }
            else if (!emitOptionalText(options.followedBy))
            {
                return false;
            }
        }

        return !hasError();
    }

    template <typename Collection, typename Body>
    bool VM::emitBlockForEach(const Collection &collection,
                              int insertCol,
                              const NativeLoopOptions &options,
                              Body &&body)
    {
        beginForEach(collection.size());
        struct Guard
        {
            VM &vm;
            bool active = true;
            ~Guard()
            {
                if (active)
                    vm.endForEach();
            }
        } guard{*this};

        while (nextForEach())
        {
            const auto index = forEachIndex();
            pushCapture();
            try
            {
                body(collection[index], forEachEnumerator());
            }
            catch (...)
            {
                popCapture();
                throw;
            }

            if (hasError())
            {
                popCapture();
                return false;
            }

            std::string iterResult = blockIndent(popCapture(), insertCol);
            bool strippedNewline = false;
            if (options.stripSingleTrailingNewline)
                strippedNewline = stripSingleTrailingNewline(iterResult);

            if (!emitOptionalText(options.precededBy))
                return false;
            if (!emit(iterResult))
                return false;

            if (forEachHasNext())
            {
                if (!emitOptionalText(options.sep))
                    return false;
            }
            else
            {
                if (options.followedBy.has_value() && !collection.empty() &&
                    !emitOptionalText(options.followedBy))
                {
                    return false;
                }
                if (strippedNewline && !emit("\n"))
                    return false;
            }
        }

        return !hasError();
    }

    template <typename Body>
    bool VM::captureAlignedCell(size_t cellIndex, Body &&body)
    {
        auto &state = activeForState();
        if (!state.rowOpen)
        {
            error_ = "captureAlignedCell: no active row";
            return false;
        }
        if (cellIndex >= state.currentRow.size())
        {
            error_ = "captureAlignedCell: cell index out of range";
            return false;
        }

        pushCapture();
        try
        {
            body();
        }
        catch (...)
        {
            popCapture();
            throw;
        }

        if (hasError())
        {
            popCapture();
            return false;
        }

        state.currentRow[cellIndex] = popCapture();
        return true;
    }

    template <typename Collection, typename Body>
    bool VM::emitAlignedForEach(const Collection &collection,
                                size_t numCols,
                                const std::vector<char> &alignSpecChars,
                                bool singleAlignChar,
                                const NativeLoopOptions &options,
                                Body &&body)
    {
        beginFor(numCols, alignSpecChars, singleAlignChar, collection.size());
        struct Guard
        {
            VM &vm;
            bool active = true;
            ~Guard()
            {
                if (active)
                    vm.finishFor();
            }
        } guard{*this};

        for (size_t index = 0; index < collection.size(); ++index)
        {
            if (!startAlignedRow())
                return false;

            try
            {
                body(collection[index], static_cast<int>(index));
            }
            catch (...)
            {
                abortAlignedRow();
                throw;
            }

            if (hasError())
            {
                abortAlignedRow();
                return false;
            }

            if (!finishAlignedRow())
                return false;
        }

        const size_t rowCount = forSize();
        for (size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
        {
            if (!emitOptionalText(options.precededBy))
                return false;
            if (!emit(endFor(rowIndex)))
                return false;

            if (rowIndex + 1 < rowCount)
            {
                if (!emitOptionalText(options.sep))
                    return false;
            }
            else if (options.followedBy.has_value() && rowCount > 0 &&
                     !emitOptionalText(options.followedBy))
            {
                return false;
            }
        }

        return !hasError();
    }

} // namespace tpp
