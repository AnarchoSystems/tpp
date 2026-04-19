#pragma once

#include <tpp/IR.h>
#include <string>
#include <vector>

namespace tpp
{

    // ════════════════════════════════════════════════════════════════════════════
    // VirtualMachine — output formatter
    //
    // Handles only scalar value emission, literal text printing, alignment
    // formatting, and policy enforcement.  Has no knowledge of control flow,
    // expression resolution, or variable slots.
    //
    // Shared by both execution modes:
    //   • Static  (tpp2cpp):  generated C++ calls these methods directly
    //   • Dynamic (Runner):   the Runner does control flow and calls these
    // ════════════════════════════════════════════════════════════════════════════

    class VirtualMachine
    {
    public:
        explicit VirtualMachine(const std::vector<PolicyDef> &policies,
                                std::string &error);

        // ── output instructions ─────────────────────────────────────────

        void emit(const std::string &text);

        [[nodiscard]] bool emitValue(const std::string &value,
                                     const std::string &exprPolicy);

        void emitAlignCell();

        // ── policy scope ────────────────────────────────────────────────

        void pushPolicy(const std::string &policy);
        void popPolicy();
        void setPolicy(const std::string &policy);
        const std::string &activePolicy() const;

        // ── capture / block body ──────────────────────────────────────

        std::string captureStart();
        std::string captureEnd(std::string saved);

        /// captureEnd + blockIndent in one call.
        std::string captureBlockEnd(std::string saved, int insertCol);

        /// De-indent captured text by its zero marker, re-indent by insertCol.
        static std::string blockIndent(const std::string &raw, int insertCol);

        /// Strip trailing '\n' if the string contains exactly one newline.
        /// Returns true if stripped.
        static bool stripSingleNewline(std::string &s);

        // ── alignment ───────────────────────────────────────────────────

        void emitAligned(const std::vector<std::string> &rows,
                         const std::string &spec,
                         const std::string &sep);

        // ── result / diagnostics ────────────────────────────────────────

        std::string takeOutput();
        std::string &error();

    private:
        const std::vector<PolicyDef> &policies_;
        std::string &error_;
        std::string output_;
        std::string activePolicy_;
        std::vector<std::string> policyScopeStack_;

        std::string effectivePolicy(const std::string &exprPolicy) const;
        const PolicyDef *findPolicy(const std::string &tag) const;
        bool applyPolicy(const std::string &policyTag, std::string &value);
        static std::string alignColumns(const std::vector<std::string> &rows,
                                        const std::string &spec,
                                        const std::string &sep);
    };

} // namespace tpp
