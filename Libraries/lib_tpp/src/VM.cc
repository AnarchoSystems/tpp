#include <tpp/VM.h>
#include <tpp/Policy.h>

#include <nlohmann/json.hpp>

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
        (void)layouts_; // used by the Rendering.cc integration layer
        outputStack_.emplace_back(); // root output buffer
    }

    void VM::setFunctions(const std::vector<FunctionDef> *functions)
    {
        functions_ = functions;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Output helpers
    // ═══════════════════════════════════════════════════════════════════

    void VM::output(std::string_view text)
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

    void VM::beginCapture()
    {
        pushCapture();
    }

    std::string VM::endCapture()
    {
        return popCapture();
    }

    std::string VM::endBlockCapture(int insertCol)
    {
        return blockIndent(popCapture(), insertCol);
    }

    // ═══════════════════════════════════════════════════════════════════
    // Literal output
    // ═══════════════════════════════════════════════════════════════════

    bool VM::emit(std::string_view text)
    {
        output(text);
        return true;
    }

    bool VM::emitSlot(int offset)
    {
        const Slot *s = derefSlot(offset);
        std::string str = s->to_string();
        emitWithMultilineIndent(str);
        return true;
    }

    bool VM::emitSlotWithPolicy(int offset, const std::string &exprPolicy)
    {
        const Slot *s = derefSlot(offset);
        std::string str = s->to_string();

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

    bool VM::emitValue(const std::string &value)
    {
        emitWithMultilineIndent(value);
        return true;
    }

    bool VM::emitValue(std::string value, const TppPolicy &policy)
    {
        try
        {
            value = policy.apply(value);
        }
        catch (const std::exception &e)
        {
            error_ = e.what();
            return false;
        }
        catch (...)
        {
            error_ = "emitValue: policy application failed";
            return false;
        }

        emitWithMultilineIndent(value);
        return true;
    }

    bool VM::emitOptionalText(const std::optional<std::string_view> &text)
    {
        return !text.has_value() || emit(*text);
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

    const Slot *VM::derefSlot(int offset) const
    {
        const Slot &s = slot(offset);
        if (s.kind == SlotKind::BoxRef && s.payload.box && !s.payload.box->empty())
            return s.payload.box->data();
        return &s;
    }

    const Slot *VM::resolveSlot(int offset, bool isOptional, bool isRecursive) const
    {
        if (isRecursive)
        {
            const Slot &s = slot(offset);
            if (s.kind == SlotKind::BoxRef && s.payload.box && !s.payload.box->empty())
            {
                const Slot *base = s.payload.box->data();
                if (isOptional) return base + 1; // skip OptionalFlag inside box
                return base;
            }
            return &s; // fallback for empty/missing box
        }
        if (isOptional) offset += 1;
        return derefSlot(offset);
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
                    const Slot *s = resolveSlot(arg.expr.slotOffset, arg.expr.isOptional, arg.expr.isRecursive);

                    // resolveSlot already unwraps Optional (skips flag) and
                    // Recursive (dereferences box), so peel the type wrapper
                    // to match the slot pointer.
                    const TypeKind *emitType = arg.expr.type.get();
                    if (emitType && emitType->value.index() == 5) { // Optional
                        const auto &inner = std::get<5>(emitType->value);
                        if (inner) emitType = inner.get();
                    }

                    // For complex types (struct/enum/list), reconstruct JSON;
                    // for primitives, use to_string().
                    std::string str;
                    if (emitType && emitType->value.index() >= 3)
                        str = slotToJsonString(s, *emitType);
                    else
                        str = s->to_string();

                    const std::string &effPol = effectivePolicy(arg.policy);
                    if (!effPol.empty() && effPol != "none")
                    {
                        const VMCompiledPolicy *pol = arg.policy.empty()
                            ? activePolicyPtr_
                            : findPolicy(effPol);
                        if (pol && !applyPolicy(effPol, *pol, str))
                            return false;
                    }

                    emitWithMultilineIndent(str);
                    return true;
                }
                else if constexpr (std::is_same_v<T, Instruction_AlignCell>)
                {
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
                    return execCall(arg);
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
        // Push policy scope if specified
        bool hasPolicy = !instr.policy.empty();
        if (hasPolicy && !pushPolicy(instr.policy))
            return false;

        const Slot *listPtr = resolveSlot(instr.collection.slotOffset, instr.collection.isOptional, instr.collection.isRecursive);
        if (listPtr->kind != SlotKind::List || !listPtr->payload.list)
        {
            if (hasPolicy) popPolicy();
            return true; // empty or non-list — silently skip
        }

        const auto &listVec = *listPtr->payload.list;
        int elemSize = instr.elementSlotCount;
        if (elemSize <= 0) elemSize = 1;
        int count = static_cast<int>(listVec.size()) / elemSize;

        for (int i = 0; i < count; ++i)
        {
            // Copy element slots to varSlotOffset
            for (int s = 0; s < elemSize; ++s)
                slotMut(instr.varSlotOffset + s) = listVec[static_cast<size_t>(i * elemSize + s)];

            // Set enumerator if present
            if (instr.enumeratorSlotOffset >= 0)
                slotMut(instr.enumeratorSlotOffset) = Slot::make_int(static_cast<int64_t>(i));

            // Render body
            std::string iterResult;
            if (instr.isBlock)
            {
                pushCapture();
                if (!execBlockBody(*instr.body, instr.insertCol))
                {
                    popCapture();
                    if (hasPolicy) popPolicy();
                    return false;
                }
                iterResult = popCapture();
            }
            else
            {
                pushCapture();
                if (!execBody(*instr.body))
                {
                    popCapture();
                    if (hasPolicy) popPolicy();
                    return false;
                }
                iterResult = popCapture();
            }

            // Handle trailing newline stripping for block sep
            bool strippedNl = false;
            if (instr.isBlock && instr.sep.has_value() && !instr.sep->empty() &&
                !iterResult.empty() && iterResult.back() == '\n')
            {
                auto nlCount = std::count(iterResult.begin(), iterResult.end(), '\n');
                if (nlCount == 1)
                {
                    iterResult.pop_back();
                    strippedNl = true;
                }
            }

            if (instr.precededBy.has_value())
                output(*instr.precededBy);

            output(iterResult);

            if (i + 1 < count)
            {
                if (instr.sep.has_value())
                    output(*instr.sep);
            }
            else
            {
                if (instr.followedBy.has_value() && count > 0)
                    output(*instr.followedBy);
                if (strippedNl)
                    output("\n");
            }
        }

        if (hasPolicy) popPolicy();
        return true;
    }

    bool VM::execForAligned(const ForInstr &instr)
    {
        bool hasPolicy = !instr.policy.empty();
        if (hasPolicy && !pushPolicy(instr.policy))
            return false;

        const Slot *listPtr2 = resolveSlot(instr.collection.slotOffset, instr.collection.isOptional, instr.collection.isRecursive);
        if (listPtr2->kind != SlotKind::List || !listPtr2->payload.list)
        {
            if (hasPolicy) popPolicy();
            return true;
        }

        const auto &listVec = *listPtr2->payload.list;
        int elemSize = instr.elementSlotCount;
        if (elemSize <= 0) elemSize = 1;
        int count = static_cast<int>(listVec.size()) / elemSize;

        // Collect cells for each iteration
        std::vector<std::vector<std::string>> rows;
        rows.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            for (int s = 0; s < elemSize; ++s)
                slotMut(instr.varSlotOffset + s) = listVec[static_cast<size_t>(i * elemSize + s)];
            if (instr.enumeratorSlotOffset >= 0)
                slotMut(instr.enumeratorSlotOffset) = Slot::make_int(static_cast<int64_t>(i));

            rows.push_back(execInstructionsCells(*instr.body));
            if (hasError())
            {
                if (hasPolicy) popPolicy();
                return false;
            }
        }

        // Compute column count and widths
        size_t numCols = 0;
        for (const auto &row : rows)
            numCols = std::max(numCols, row.size());

        std::vector<char> colSpec(numCols, 'l');
        if (instr.alignSpec.has_value() && !instr.alignSpec->empty())
        {
            const std::string &s = *instr.alignSpec;
            if (s.size() == 1)
                std::fill(colSpec.begin(), colSpec.end(), s[0]);
            else
                for (size_t ci = 0; ci < colSpec.size() && ci < s.size(); ++ci)
                    colSpec[ci] = s[ci];
        }

        std::vector<int> colWidth(numCols, 0);
        for (const auto &row : rows)
            for (size_t ci = 0; ci < row.size(); ++ci)
                colWidth[ci] = std::max(colWidth[ci], static_cast<int>(row[ci].size()));

        // Emit padded rows
        for (int i = 0; i < count; ++i)
        {
            const auto &row = rows[static_cast<size_t>(i)];
            std::string iterResult = renderAlignedRow(row, colWidth, colSpec, numCols);

            if (instr.precededBy.has_value())
                output(*instr.precededBy);
            output(iterResult);
            if (i + 1 < count)
            {
                if (instr.sep.has_value())
                    output(*instr.sep);
            }
            else if (instr.followedBy.has_value())
                output(*instr.followedBy);
        }

        if (hasPolicy) popPolicy();
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Control flow — execIf
    // ═══════════════════════════════════════════════════════════════════

    bool VM::execIf(const IfInstr &instr)
    {
        int condOff = instr.condExpr.slotOffset;
        // NOTE: do NOT skip +1 for isOptional here.  The if-condition on an
        // Optional checks the OptionalFlag (present/absent) at slotOffset,
        // not the inner value.
        const Slot *condPtr = derefSlot(condOff);
        bool cond = false;

        if (instr.negated)
        {
            // Negated: true if bool-false or optional-absent or empty
            if (condPtr->kind == SlotKind::Bool)
                cond = !condPtr->as_bool();
            else if (condPtr->kind == SlotKind::OptionalFlag)
                cond = !condPtr->as_optional_flag();
            else
                cond = (condPtr->kind == SlotKind::Empty);
        }
        else
        {
            if (condPtr->kind == SlotKind::Bool)
                cond = condPtr->as_bool();
            else if (condPtr->kind == SlotKind::OptionalFlag)
                cond = condPtr->as_optional_flag();
            else
                cond = (condPtr->kind != SlotKind::Empty);
        }

        const auto &branch = cond ? *instr.thenBody : *instr.elseBody;
        if (instr.isBlock)
            return execBlockBody(branch, instr.insertCol);
        else
            return execBody(branch);
    }

    // ═══════════════════════════════════════════════════════════════════
    // Control flow — execSwitch
    // ═══════════════════════════════════════════════════════════════════

    bool VM::execSwitch(const SwitchInstr &instr)
    {
        bool hasPolicy = !instr.policy.empty();
        if (hasPolicy && !pushPolicy(instr.policy))
            return false;

        // The expr slot is a VariantTag (or BoxRef to one)
        const Slot *tagPtr = resolveSlot(instr.expr.slotOffset, instr.expr.isOptional, instr.expr.isRecursive);
        if (tagPtr->kind != SlotKind::VariantTag)
        {
            if (hasPolicy) popPolicy();
            return true; // not a variant — silently skip
        }

        int32_t tag = tagPtr->as_variant_tag();

        for (const auto &c : *instr.cases)
        {
            if (c.variantIndex == tag)
            {
                // Copy payload slots to bindingSlotOffset
                if (c.bindingSlotOffset >= 0 && c.payloadSlotCount > 0)
                {
                    // Payload slots start after the tag (in the box or inline)
                    const Slot *payloadStart = tagPtr + 1;
                    if (c.payloadIsRecursive && payloadStart->kind == SlotKind::BoxRef
                        && payloadStart->payload.box && !payloadStart->payload.box->empty())
                    {
                        // Recursive payload: deref the BoxRef to get unboxed data
                        const Slot *inner = payloadStart->payload.box->data();
                        for (int s = 0; s < c.payloadSlotCount; ++s)
                            slotMut(c.bindingSlotOffset + s) = inner[s];
                    }
                    else
                    {
                        for (int s = 0; s < c.payloadSlotCount; ++s)
                            slotMut(c.bindingSlotOffset + s) = payloadStart[s];
                    }
                }

                if (instr.isBlock)
                {
                    if (!execBlockBody(*c.body, instr.insertCol))
                    {
                        if (hasPolicy) popPolicy();
                        return false;
                    }
                }
                else
                {
                    if (!execBody(*c.body))
                    {
                        if (hasPolicy) popPolicy();
                        return false;
                    }
                }
                break;
            }
        }

        if (hasPolicy) popPolicy();
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Control flow — execCall
    // ═══════════════════════════════════════════════════════════════════

    bool VM::execCall(const CallInstr &instr)
    {
        if (!functions_)
        {
            error_ = "VM::execCall: no function table set";
            return false;
        }
        if (instr.functionIndex < 0 ||
            instr.functionIndex >= static_cast<int>(functions_->size()))
        {
            error_ = "VM::execCall: invalid function index for '" + instr.functionName + "'";
            return false;
        }

        const FunctionDef &callee = (*functions_)[static_cast<size_t>(instr.functionIndex)];

        // Build callee frame: allocate frameSlotCount slots
        std::vector<Slot> calleeFrame(static_cast<size_t>(callee.frameSlotCount));

        // Copy argument slots into the callee frame at each param's slotOffset
        for (size_t i = 0; i < instr.arguments.size() && i < callee.params.size(); ++i)
        {
            const auto &argExpr = instr.arguments[i];
            const auto &param = callee.params[i];

            // Resolve slot, handling recursive (BoxRef) and optional correctly
            const Slot *src = resolveSlot(argExpr.slotOffset, argExpr.isOptional, argExpr.isRecursive);
            for (int s = 0; s < param.slotCount; ++s)
                calleeFrame[static_cast<size_t>(param.slotOffset + s)] = src[s];
        }

        // Push callee frame
        pushFrame(calleeFrame);

        // Push callee policy scope if set
        bool hasPolicy = !callee.policy.empty();
        if (hasPolicy && !pushPolicy(callee.policy))
        {
            popFrame();
            return false;
        }

        // Execute callee body into a capture buffer
        pushCapture();
        bool ok = execBody(*callee.body);
        std::string callResult = popCapture();

        if (hasPolicy) popPolicy();
        popFrame();

        if (!ok) return false;

        // Strip trailing newline from call result (match Rendering.cc behavior)
        if (!callResult.empty() && callResult.back() == '\n')
            callResult.pop_back();

        output(callResult);
        return true;
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
                    else if constexpr (std::is_same_v<T, EmitExprInstr>)
                    {
                        current += resolveSlot(arg.expr.slotOffset, arg.expr.isOptional, arg.expr.isRecursive)->to_string();
                    }
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

    void VM::beginForEach(size_t size)
    {
        ForEachState state;
        state.size = size;
        forEachStack_.push_back(std::move(state));
    }

    bool VM::nextForEach()
    {
        if (forEachStack_.empty())
        {
            error_ = "nextForEach: no active for-each session";
            return false;
        }

        auto &state = forEachStack_.back();
        if (!state.started)
        {
            state.started = true;
            state.index = 0;
            return state.size > 0;
        }

        if (state.index + 1 >= state.size)
            return false;

        ++state.index;
        return true;
    }

    size_t VM::forEachIndex() const
    {
        return forEachStack_.back().index;
    }

    bool VM::forEachHasNext() const
    {
        const auto &state = forEachStack_.back();
        return state.started && (state.index + 1 < state.size);
    }

    int VM::forEachEnumerator() const
    {
        return static_cast<int>(forEachStack_.back().index);
    }

    void VM::endForEach()
    {
        if (forEachStack_.empty())
        {
            error_ = "endForEach: no active for-each session";
            return;
        }
        forEachStack_.pop_back();
    }

    void VM::beginFor(size_t numCols,
                      const std::vector<char> &alignSpecChars,
                      bool singleAlignChar,
                      size_t rowReserve)
    {
        ForState state;
        state.numCols = numCols;
        state.colWidth.assign(numCols, 0);
        state.colSpec.assign(numCols, 'l');
        state.currentRow.assign(numCols, std::string{});
        if (rowReserve > 0)
            state.rows.reserve(rowReserve);

        if (!alignSpecChars.empty())
        {
            if (singleAlignChar)
            {
                std::fill(state.colSpec.begin(), state.colSpec.end(), alignSpecChars.front());
            }
            else
            {
                for (size_t i = 0; i < numCols && i < alignSpecChars.size(); ++i)
                    state.colSpec[i] = alignSpecChars[i];
            }
        }

        forStack_.push_back(std::move(state));
    }

    VM::ForState &VM::activeForState()
    {
        return forStack_.back();
    }

    const VM::ForState &VM::activeForState() const
    {
        return forStack_.back();
    }

    bool VM::startAlignedRow()
    {
        if (forStack_.empty())
        {
            error_ = "startAlignedRow: no active aligned-for session";
            return false;
        }

        auto &state = activeForState();
        state.currentRow.assign(state.numCols, std::string{});
        state.rowOpen = true;
        return true;
    }

    bool VM::finishAlignedRow()
    {
        if (forStack_.empty())
        {
            error_ = "finishAlignedRow: no active aligned-for session";
            return false;
        }

        auto &state = activeForState();
        if (!state.rowOpen)
        {
            error_ = "finishAlignedRow: no active row";
            return false;
        }

        exec(state.currentRow);
        state.currentRow.assign(state.numCols, std::string{});
        state.rowOpen = false;
        return true;
    }

    void VM::abortAlignedRow()
    {
        if (forStack_.empty())
            return;

        auto &state = activeForState();
        state.currentRow.assign(state.numCols, std::string{});
        state.rowOpen = false;
    }

    void VM::exec(const std::vector<std::string> &row)
    {
        auto &state = forStack_.back();
        for (size_t ci = 0; ci < row.size() && ci < state.colWidth.size(); ++ci)
            state.colWidth[ci] = std::max(state.colWidth[ci], static_cast<int>(row[ci].size()));
        state.rows.push_back(row);
    }

    std::string VM::endFor(size_t rowIndex) const
    {
        const auto &state = forStack_.back();
        if (rowIndex >= state.rows.size())
            return "";
        return renderAlignedRow(state.rows[rowIndex], state.colWidth, state.colSpec, state.numCols);
    }

    size_t VM::forSize() const
    {
        return forStack_.back().rows.size();
    }

    void VM::finishFor()
    {
        if (forStack_.empty())
        {
            error_ = "finishFor: no active aligned-for session";
            return;
        }
        abortAlignedRow();
        forStack_.pop_back();
    }

    std::string VM::renderAlignedRow(const std::vector<std::string> &row,
                                     const std::vector<int> &colWidth,
                                     const std::vector<char> &colSpec,
                                     size_t numCols)
    {
        std::string line;
        for (size_t ci = 0; ci < row.size(); ++ci)
        {
            if (ci + 1 < numCols)
            {
                line += padCell(row[ci], colWidth[ci], colSpec[ci]);
            }
            else
            {
                char spec = colSpec[ci];
                int pad = colWidth[ci] - static_cast<int>(row[ci].size());
                if (pad > 0 && spec != 'l')
                {
                    int left = (spec == 'c') ? pad / 2 : pad;
                    line += std::string(static_cast<size_t>(left), ' ');
                }
                line += row[ci];
            }
        }
        return line;
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

    bool VM::stripSingleTrailingNewline(std::string &text)
    {
        if (text.empty() || text.back() != '\n')
            return false;

        if (std::count(text.begin(), text.end(), '\n') != 1)
            return false;

        text.pop_back();
        return true;
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
    // Slot → JSON serialization (for emitting complex types)
    // ═══════════════════════════════════════════════════════════════════

    std::string VM::slotToJsonString(const Slot *base, const TypeKind &type) const
    {
        switch (type.value.index())
        {
        case 0: // Str
            return nlohmann::json(base->as_str()).dump();
        case 1: // Int
            return std::to_string(base->as_int());
        case 2: // Bool
            return base->as_bool() ? "true" : "false";
        case 3: // Named — struct or enum
        {
            const auto &name = std::get<3>(type.value);
            const Layout *layout = layouts_.find(name);
            if (!layout) return "null";

            if (layout->isEnum)
            {
                int32_t tagIdx = base->as_variant_tag();
                if (tagIdx < 0 || tagIdx >= static_cast<int32_t>(layout->caseRanges.size()))
                    return "null";
                const CaseRange &cr = layout->caseRanges[static_cast<size_t>(tagIdx)];
                if (!cr.hasPayload)
                    return nlohmann::json(cr.tag).dump(); // bare tag → "TagName"

                // Payload slots start at base + cr.payloadOffset
                const Slot *payloadBase = base + cr.payloadOffset;
                if (cr.isRecursive)
                {
                    // Recursive payload is boxed
                    if (payloadBase->kind == SlotKind::BoxRef && payloadBase->payload.box && !payloadBase->payload.box->empty())
                        payloadBase = payloadBase->payload.box->data();
                    else
                        return "null";
                }
                std::string payloadJson = cr.payloadType
                    ? slotToJsonString(payloadBase, *cr.payloadType)
                    : "null";
                return "{" + nlohmann::json(cr.tag).dump() + ":" + payloadJson + "}";
            }
            else
            {
                // Struct
                std::string result = "{";
                bool first = true;
                for (const auto &fr : layout->fieldRanges)
                {
                    if (!first) result += ",";
                    first = false;
                    result += nlohmann::json(fr.name).dump() + ":";

                    const Slot *fieldBase = base + fr.offset;
                    if (fr.isOptional)
                    {
                        if (!fieldBase->as_optional_flag())
                        {
                            result += "null";
                            continue;
                        }
                        fieldBase += 1; // skip OptionalFlag
                    }
                    if (fr.isBox)
                    {
                        if (fieldBase->kind == SlotKind::BoxRef && fieldBase->payload.box && !fieldBase->payload.box->empty())
                            fieldBase = fieldBase->payload.box->data();
                        else
                        {
                            result += "null";
                            continue;
                        }
                    }
                    if (fr.type)
                        result += slotToJsonString(fieldBase, *fr.type);
                    else
                        result += fieldBase->to_string();
                }
                result += "}";
                return result;
            }
        }
        case 4: // List
        {
            const auto &inner = std::get<4>(type.value);
            if (!inner || base->kind != SlotKind::List || !base->payload.list)
                return "[]";

            const auto &vec = *base->payload.list;
            int elemSize = layouts_.slot_size(*inner);
            if (elemSize <= 0) return "[]";
            size_t count = vec.size() / static_cast<size_t>(elemSize);

            std::string result = "[";
            for (size_t i = 0; i < count; ++i)
            {
                if (i > 0) result += ",";
                result += slotToJsonString(&vec[i * static_cast<size_t>(elemSize)], *inner);
            }
            result += "]";
            return result;
        }
        case 5: // Optional
        {
            const auto &inner = std::get<5>(type.value);
            if (!inner) return "null";
            if (base->kind == SlotKind::OptionalFlag)
            {
                if (!base->as_optional_flag()) return "null";
                return slotToJsonString(base + 1, *inner);
            }
            return slotToJsonString(base, *inner);
        }
        default:
            return base->to_string();
        }
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
