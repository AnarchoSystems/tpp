#include <tpp/VM.h>
#include <tpp/Policy.h>

#include <algorithm>
#include <sstream>

namespace tpp
{

    // ═══════════════════════════════════════════════════════════════════
    // Construction
    // ═══════════════════════════════════════════════════════════════════

    VM::VM(const LayoutTable &layouts, std::map<std::string, VMCompiledPolicy> policies)
        : layouts_(layouts),
          compiledPolicies_(std::move(policies))
    {
        (void)layouts_; // will be used once the VM executes layout-dependent instructions
        outputStack_.emplace_back(); // root output buffer
    }

    // ═══════════════════════════════════════════════════════════════════
    // Output helpers
    // ═══════════════════════════════════════════════════════════════════

    void VM::output(const std::string &text)
    {
        outputStack_.back() += text;
    }

    std::string &VM::activeOutput()
    {
        return outputStack_.back();
    }

    void VM::pushCapture()
    {
        outputStack_.emplace_back();
    }

    std::string VM::popCapture()
    {
        std::string captured = std::move(outputStack_.back());
        outputStack_.pop_back();
        return captured;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Literal output
    // ═══════════════════════════════════════════════════════════════════

    bool VM::emit(const std::string &text)
    {
        output(text);
        return true;
    }

    bool VM::emitSlot(int offset)
    {
        const Slot &s = slot(offset);
        std::string str = s.to_string();
        emitWithMultilineIndent(str);
        return true;
    }

    bool VM::emitSlotWithPolicy(int offset, const std::string &exprPolicy)
    {
        const Slot &s = slot(offset);
        std::string str = s.to_string();

        const std::string &effPol = effectivePolicy(exprPolicy);
        if (!effPol.empty() && effPol != "none")
        {
            const VMCompiledPolicy *pol = exprPolicy.empty()
                ? activePolicyPtr_
                : findPolicy(effPol);
            if (pol && !applyPolicy(effPol, *pol, str))
                return false;
        }

        emitWithMultilineIndent(str);
        return true;
    }

    void VM::emitWithMultilineIndent(const std::string &text)
    {
        auto &out = activeOutput();
        auto lastNl = out.rfind('\n');
        size_t col = (lastNl == std::string::npos) ? out.size() : out.size() - lastNl - 1;

        if (col > 0 && text.find('\n') != std::string::npos)
        {
            std::string pad(col, ' ');
            std::string result;
            size_t start = 0;
            while (true)
            {
                auto end = text.find('\n', start);
                if (start > 0)
                    result += pad;
                result += text.substr(start, end == std::string::npos ? end : end - start);
                if (end == std::string::npos)
                    break;
                result += '\n';
                start = end + 1;
            }
            output(result);
        }
        else
        {
            output(text);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Frame management
    // ═══════════════════════════════════════════════════════════════════

    void VM::pushFrame(const std::vector<Slot> &slots)
    {
        fpStack_.push_back(fp_);
        fp_ = static_cast<int>(stack_.size());
        stack_.insert(stack_.end(), slots.begin(), slots.end());
    }

    void VM::popFrame()
    {
        stack_.resize(static_cast<size_t>(fp_));
        fp_ = fpStack_.back();
        fpStack_.pop_back();
    }

    const Slot &VM::slot(int offset) const
    {
        return stack_[static_cast<size_t>(fp_ + offset)];
    }

    Slot &VM::slotMut(int offset)
    {
        return stack_[static_cast<size_t>(fp_ + offset)];
    }

    // ═══════════════════════════════════════════════════════════════════
    // Policy scope
    // ═══════════════════════════════════════════════════════════════════

    bool VM::pushPolicy(const std::string &tag)
    {
        policyScopeStack_.push_back(activePolicy_);
        activePolicy_ = tag;
        activePolicyPtr_ = findPolicy(tag);
        return true;
    }

    bool VM::popPolicy()
    {
        if (policyScopeStack_.empty())
        {
            error_ = "popPolicy: scope stack underflow";
            return false;
        }
        activePolicy_ = policyScopeStack_.back();
        policyScopeStack_.pop_back();
        activePolicyPtr_ = activePolicy_.empty() ? nullptr : findPolicy(activePolicy_);
        return true;
    }

    const std::string &VM::effectivePolicy(const std::string &exprPolicy) const
    {
        return exprPolicy.empty() ? activePolicy_ : exprPolicy;
    }

    const VMCompiledPolicy *VM::findPolicy(const std::string &tag)
    {
        auto it = compiledPolicies_.find(tag);
        if (it != compiledPolicies_.end())
            return &it->second;
        return nullptr;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Control flow — execBody / execBlockBody
    // ═══════════════════════════════════════════════════════════════════

    bool VM::execBody(const std::vector<Instruction> &instrs)
    {
        for (const auto &instr : instrs)
        {
            if (hasError())
                return false;

            bool ok = std::visit([&](auto &&arg) -> bool
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, EmitInstr>)
                {
                    return emit(arg.text);
                }
                else if constexpr (std::is_same_v<T, EmitExprInstr>)
                {
                    // The caller is responsible for mapping the expr to a slot offset.
                    // In this low-level VM, EmitExprInstr is emitted as emitSlotWithPolicy
                    // by a higher-level driver. For now, set an error.
                    error_ = "VM::execBody: EmitExprInstr requires slot-based translation";
                    return false;
                }
                else if constexpr (std::is_same_v<T, Instruction_AlignCell>)
                {
                    // Handled by execInstructionsCells; no-op in normal body
                    return true;
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<ForInstr>>)
                {
                    return execFor(*arg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<IfInstr>>)
                {
                    return execIf(*arg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<SwitchInstr>>)
                {
                    return execSwitch(*arg);
                }
                else if constexpr (std::is_same_v<T, CallInstr>)
                {
                    error_ = "VM::execBody: CallInstr is not supported in the interpreted path";
                    return false;
                }
                else
                {
                    return true;
                }
            }, instr.value);

            if (!ok)
                return false;
        }
        return true;
    }

    bool VM::execBlockBody(const std::vector<Instruction> &instrs, int insertCol)
    {
        pushCapture();
        if (!execBody(instrs))
        {
            popCapture(); // discard on error
            return false;
        }
        std::string raw = popCapture();
        if (!raw.empty())
            output(blockIndent(raw, insertCol));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Control flow — execFor
    // ═══════════════════════════════════════════════════════════════════

    bool VM::execFor(const ForInstr &instr)
    {
        // The collection expression must already be resolved to a slot.
        // In the low-level VM, the for instruction operates on the slot
        // that the collection.path resolves to.  For now this is a
        // framework that a higher-level driver will call after setting
        // up the slot references.

        if (instr.alignSpec.has_value())
            return execForAligned(instr);
        else
            return execForNormal(instr);
    }

    bool VM::execForNormal(const ForInstr &instr)
    {
        // Save policy scope
        std::string savedPolicy = activePolicy_;
        const VMCompiledPolicy *savedPtr = activePolicyPtr_;
        if (!instr.policy.empty())
        {
            activePolicy_ = instr.policy;
            activePolicyPtr_ = findPolicy(instr.policy);
        }

        // The for body receives its iteration element via a frame push.
        // The caller must have set up the list slot; this method provides
        // the iteration scaffolding (sep, precededBy, followedBy, block indent).

        // For now, this is a placeholder that the driver will call with
        // a list slot reference + element layout.  We provide the
        // iteration + output logic.

        activePolicy_ = savedPolicy;
        activePolicyPtr_ = savedPtr;
        return true;
    }

    bool VM::execForAligned(const ForInstr &instr)
    {
        std::string savedPolicy = activePolicy_;
        const VMCompiledPolicy *savedPtr = activePolicyPtr_;
        if (!instr.policy.empty())
        {
            activePolicy_ = instr.policy;
            activePolicyPtr_ = findPolicy(instr.policy);
        }

        // Alignment rendering placeholder — driver will supply iteration.

        activePolicy_ = savedPolicy;
        activePolicyPtr_ = savedPtr;
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Control flow — execIf
    // ═══════════════════════════════════════════════════════════════════

    bool VM::execIf(const IfInstr &instr)
    {
        // The condition expression must already be resolved to a slot.
        // This is a framework method — the higher-level driver resolves
        // the condition before calling.  For now, set an error.
        error_ = "VM::execIf: requires slot-based condition resolution";
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Control flow — execSwitch
    // ═══════════════════════════════════════════════════════════════════

    bool VM::execSwitch(const SwitchInstr &instr)
    {
        // The switch expression must already be resolved to a slot.
        error_ = "VM::execSwitch: requires slot-based expression resolution";
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Alignment helpers
    // ═══════════════════════════════════════════════════════════════════

    std::vector<std::string> VM::execInstructionsCells(const std::vector<Instruction> &instrs)
    {
        std::vector<std::string> cells;
        std::string current;

        for (const auto &instr : instrs)
        {
            if (std::holds_alternative<Instruction_AlignCell>(instr.value))
            {
                cells.push_back(std::move(current));
                current.clear();
            }
            else
            {
                std::visit([&](auto &&arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, EmitInstr>)
                        current += arg.text;
                    // EmitExprInstr and other types would need slot-based
                    // resolution from a higher-level driver.
                }, instr.value);
            }
        }
        cells.push_back(std::move(current));
        return cells;
    }

    std::string VM::padCell(const std::string &s, int width, char spec)
    {
        int len = static_cast<int>(s.size());
        if (len >= width)
            return s;
        int pad = width - len;
        if (spec == 'r')
            return std::string(static_cast<size_t>(pad), ' ') + s;
        if (spec == 'c')
        {
            int left = pad / 2;
            int right = pad - left;
            return std::string(static_cast<size_t>(left), ' ') + s +
                   std::string(static_cast<size_t>(right), ' ');
        }
        return s + std::string(static_cast<size_t>(pad), ' ');
    }

    // ═══════════════════════════════════════════════════════════════════
    // Block indentation
    // ═══════════════════════════════════════════════════════════════════

    static std::string trim(const std::string &s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    std::string VM::blockIndent(const std::string &raw, int insertCol)
    {
        if (raw.empty())
            return "";

        std::vector<std::string> lines;
        std::istringstream iss(raw);
        std::string line;
        while (std::getline(iss, line))
            lines.push_back(line);
        bool trailingNl = !raw.empty() && raw.back() == '\n';

        // Find zero-marker
        std::string zeroMarker;
        for (auto &l : lines)
        {
            if (!trim(l).empty())
            {
                size_t ws = 0;
                while (ws < l.size() && (l[ws] == ' ' || l[ws] == '\t'))
                    ws++;
                zeroMarker = l.substr(0, ws);
                break;
            }
        }

        std::string indent(static_cast<size_t>(insertCol), ' ');
        std::string result;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            std::string l = lines[i];
            if (!l.empty() && !zeroMarker.empty() &&
                l.size() >= zeroMarker.size() &&
                l.compare(0, zeroMarker.size(), zeroMarker) == 0)
            {
                l = l.substr(zeroMarker.size());
            }
            if (!l.empty())
                result += indent + l;
            if (i + 1 < lines.size() || trailingNl)
                result += "\n";
        }

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Policy pipeline
    // ═══════════════════════════════════════════════════════════════════

    bool VM::applyPolicy(const std::string &tag, const VMCompiledPolicy &pol,
                         std::string &value)
    {
        // 1. Length check
        if (pol.length.has_value())
        {
            int len = static_cast<int>(value.size());
            if (pol.length->min.has_value() && len < *pol.length->min)
            {
                error_ = "[policy " + tag + "] value is below minimum length of " +
                         std::to_string(*pol.length->min);
                return false;
            }
            if (pol.length->max.has_value() && len > *pol.length->max)
            {
                error_ = "[policy " + tag + "] value exceeds maximum length of " +
                         std::to_string(*pol.length->max);
                return false;
            }
        }

        // 2. reject-if
        if (pol.rejectIf.has_value())
        {
            if (std::regex_search(value, pol.rejectIf->rx))
            {
                error_ = "[policy " + tag + "] " + pol.rejectIf->message;
                return false;
            }
        }

        // 3. require[]
        for (auto &step : pol.require)
        {
            std::smatch m;
            if (!std::regex_search(value, m, step.rx))
            {
                error_ = "[policy " + tag + "] value does not match required pattern";
                return false;
            }
            if (!step.replace.empty())
            {
                value = std::regex_replace(value, step.rx, step.compiled_replace);
            }
        }

        // 4. replacements[]
        for (auto &rep : pol.replacements)
        {
            if (rep.find.empty())
                continue;
            std::string out;
            size_t pos = 0;
            while (true)
            {
                size_t found = value.find(rep.find, pos);
                if (found == std::string::npos)
                {
                    out += value.substr(pos);
                    break;
                }
                out += value.substr(pos, found - pos);
                out += rep.replace;
                pos = found + rep.find.size();
            }
            value = std::move(out);
        }

        // 5. output-filter[]
        for (auto &rx : pol.outputFilter)
        {
            if (!std::regex_match(value, rx))
            {
                error_ = "[policy " + tag + "] output does not match required filter";
                return false;
            }
        }

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Policy compilation
    // ═══════════════════════════════════════════════════════════════════

    VMCompiledPolicy VM::compilePolicy(const PolicyDef &def)
    {
        VMCompiledPolicy cp;
        cp.tag = def.tag;
        cp.length = def.length;
        if (def.rejectIf.has_value())
        {
            cp.rejectIf = VMCompiledPolicy::RejectIf{
                std::regex(def.rejectIf->regex),
                def.rejectIf->message};
        }
        for (const auto &r : def.require)
        {
            VMCompiledRegexStep step;
            step.rx = std::regex(r.regex);
            step.replace = r.replace;
            if (!r.replace.empty())
                step.compiled_replace = tpp::precompile_replace(r.replace);
            cp.require.push_back(std::move(step));
        }
        cp.replacements = def.replacements;
        for (const auto &f : def.outputFilter)
            cp.outputFilter.push_back(std::regex(f.regex));
        return cp;
    }

    std::map<std::string, VMCompiledPolicy> VM::compilePolicies(const IR &ir)
    {
        std::map<std::string, VMCompiledPolicy> result;
        for (const auto &pd : ir.policies)
            result.emplace(pd.tag, compilePolicy(pd));
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Result
    // ═══════════════════════════════════════════════════════════════════

    std::string VM::takeOutput()
    {
        return std::move(outputStack_.front());
    }

    std::string &VM::error()
    {
        return error_;
    }

} // namespace tpp
