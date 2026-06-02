#include <tpp/IRInterpreter.h>

#include <tpp/Writer.h>

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace tpp
{
    namespace
    {
        struct CapturedBlockInfo
        {
            bool wrapped = false;
            std::optional<int> blockIndentInParentBlock;
            size_t firstIndex = 0;
            size_t pastLastIndex = 0;
        };

        CapturedBlockInfo analyze_captured_block(const std::vector<Instruction> &body)
        {
            CapturedBlockInfo info;
            info.pastLastIndex = body.size();

            if (body.size() >= 2 &&
                std::holds_alternative<BeginCapturedBlockInstr>(body.front().value) &&
                std::holds_alternative<Instruction_EmitCapturedBlock>(body.back().value))
            {
                info.wrapped = true;
                info.blockIndentInParentBlock = std::get<BeginCapturedBlockInstr>(body.front().value).blockIndentInParentBlock;
                info.firstIndex = 1;
                info.pastLastIndex = body.size() - 1;
            }

            return info;
        }

        std::vector<std::vector<const Instruction *>> split_cells(const std::vector<Instruction> &body)
        {
            std::vector<std::vector<const Instruction *>> cells;
            cells.emplace_back();

            const auto capturedBlockInfo = analyze_captured_block(body);
            for (size_t index = capturedBlockInfo.firstIndex; index < capturedBlockInfo.pastLastIndex; ++index)
            {
                const auto &instr = body[index];
                if (std::holds_alternative<Instruction_AlignCell>(instr.value))
                    cells.emplace_back();
                else
                    cells.back().push_back(&instr);
            }

            return cells;
        }

        std::string json_scalar_to_string(const nlohmann::json &value)
        {
            if (value.is_null())
                return "";
            if (value.is_string())
                return value.get<std::string>();
            if (value.is_boolean())
                return value.get<bool>() ? "true" : "false";
            if (value.is_number_integer())
                return std::to_string(value.get<long long>());
            if (value.is_number_unsigned())
                return std::to_string(value.get<unsigned long long>());
            return value.dump();
        }

        bool extract_enum_case(const nlohmann::json &value,
                               std::string &tag,
                               const nlohmann::json *&payload)
        {
            payload = nullptr;

            if (value.is_string())
            {
                tag = value.get<std::string>();
                return true;
            }

            if (!value.is_object())
                return false;

            if (value.contains("tag") && value["tag"].is_string())
            {
                tag = value["tag"].get<std::string>();
                if (value.contains("value") && !value["value"].is_null())
                    payload = &value["value"];
                return true;
            }

            if (value.size() != 1)
                return false;

            const auto item = value.begin();
            tag = item.key();
            if (!item.value().is_null())
                payload = &item.value();
            return true;
        }

        std::map<std::string, TppPolicy> compile_runtime_policies(const IR &ir)
        {
            std::map<std::string, TppPolicy> result;
            for (const auto &def : ir.policies)
            {
                TppPolicy policy;
                policy.tag = def.tag;
                if (def.length.has_value())
                {
                    policy.minLength = def.length->min;
                    policy.maxLength = def.length->max;
                }
                if (def.rejectIf.has_value())
                {
                    policy.rejectIf = TppPolicy::RejectRule{
                        std::regex(def.rejectIf->regex),
                        def.rejectIf->message};
                }
                for (const auto &step : def.require)
                {
                    TppPolicy::RequireStep requireStep;
                    requireStep.pattern = std::regex(step.regex);
                    if (step.compiledReplace.has_value())
                        requireStep.replace = *step.compiledReplace;
                    policy.require.push_back(std::move(requireStep));
                }
                for (const auto &replacement : def.replacements)
                    policy.replacements.emplace_back(replacement.find, replacement.replace);
                for (const auto &filter : def.outputFilter)
                    policy.outputFilter.push_back(std::regex(filter.regex));

                result.emplace(def.tag, std::move(policy));
            }
            return result;
        }

        Range to_tracking_range(const SourceRange &sourceRange)
        {
            return {{sourceRange.start.line, sourceRange.start.character},
                    {sourceRange.end.line, sourceRange.end.character}};
        }

        void trim_mappings(std::vector<RenderMapping> &mappings, int newEnd)
        {
            std::vector<RenderMapping> adjusted;
            adjusted.reserve(mappings.size());
            for (const auto &mapping : mappings)
            {
                if (mapping.outStart >= newEnd)
                    continue;

                RenderMapping trimmed = mapping;
                trimmed.outEnd = std::min(trimmed.outEnd, newEnd);
                if (trimmed.outStart < trimmed.outEnd)
                    adjusted.push_back(std::move(trimmed));
            }
            mappings = std::move(adjusted);
        }

        void strip_trailing_newline(Writer::CaptureResult &result)
        {
            if (result.text.empty() || result.text.back() != '\n')
                return;

            result.text.pop_back();
            trim_mappings(result.mappings, static_cast<int>(result.text.size()));
        }
    }

    struct IRInterpreter::Impl
    {
        explicit Impl(const IR &ir)
            : ir_(ir), writer_(compile_runtime_policies(ir))
        {
        }

        bool render(const FunctionDef &function, std::map<std::string, nlohmann::json> bindings)
        {
            scopes_.push_back(std::move(bindings));
            sourceUriStack_.push_back(function.sourceUri.value_or(""));

            const bool hasPolicy = !function.policy.empty();
            if (hasPolicy && !writer_.pushPolicy(function.policy))
            {
                sourceUriStack_.pop_back();
                scopes_.pop_back();
                return false;
            }

            const bool ok = exec_body(*function.body);

            if (hasPolicy && !writer_.popPolicy())
            {
                sourceUriStack_.pop_back();
                scopes_.pop_back();
                return false;
            }

            sourceUriStack_.pop_back();
            scopes_.pop_back();
            return ok;
        }

        std::string take_output(std::vector<RenderMapping> *tracking)
        {
            std::string output = writer_.takeOutput(Writer::OutputPostProcessing::StripSingleTrailingNewline);
            if (tracking)
            {
                *tracking = writer_.mappings();
                trim_mappings(*tracking, static_cast<int>(output.size()));
            }
            return output;
        }

        const std::string &error() const
        {
            return writer_.error();
        }

    private:
        const std::string &current_source_uri() const
        {
            static const std::string empty;
            if (sourceUriStack_.empty())
                return empty;
            return sourceUriStack_.back();
        }

        template <typename Func>
        bool exec_with_structural_tracking(const std::optional<SourceRange> &sourceRange, Func &&func)
        {
            const int outStart = static_cast<int>(writer_.outputSize());
            const bool ok = std::forward<Func>(func)();
            if (!ok)
                return false;

            if (sourceRange.has_value())
            {
                const int outEnd = static_cast<int>(writer_.outputSize());
                writer_.addMapping(current_source_uri(), to_tracking_range(*sourceRange), outStart, outEnd);
            }

            return true;
        }

        const nlohmann::json *resolve_binding(const std::string &name) const
        {
            for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
            {
                const auto found = it->find(name);
                if (found != it->end())
                    return &found->second;
            }
            return nullptr;
        }

        const nlohmann::json *resolve_expr(const ExprInfo &expr) const
        {
            const nlohmann::json *value = resolve_binding(expr.bindingName);
            if (!value || value->is_null())
                return nullptr;

            for (const auto &segment : expr.segments)
            {
                if (!value->is_object())
                    return nullptr;
                const auto it = value->find(segment.field);
                if (it == value->end() || it->is_null())
                    return nullptr;
                value = &*it;
            }

            return value->is_null() ? nullptr : value;
        }

        bool expr_truthy(const ExprInfo &expr) const
        {
            const nlohmann::json *value = resolve_expr(expr);
            if (!value)
                return false;
            if (value->is_boolean())
                return value->get<bool>();
            return !value->is_null();
        }

        bool exec_body(const std::vector<Instruction> &body)
        {
            return exec_body_range(body, 0, body.size());
        }

        bool exec_body_range(const std::vector<Instruction> &body,
                             size_t firstIndex,
                             size_t pastLastIndex)
        {
            for (size_t index = firstIndex; index < pastLastIndex; ++index)
            {
                if (!exec_instruction(body[index]))
                    return false;
            }
            return true;
        }

        bool exec_instruction(const Instruction &instr)
        {
            return std::visit([&](auto &&arg) -> bool
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, EmitInstr>)
                {
                    if (arg.sourceRange.has_value())
                        return writer_.emitTracked(arg.text, current_source_uri(), to_tracking_range(*arg.sourceRange));
                    return writer_.emit(arg.text);
                }
                else if constexpr (std::is_same_v<T, EmitExprInstr>)
                {
                    const nlohmann::json *value = resolve_expr(arg.expr);
                    if (!value)
                        return true;

                    if (arg.sourceRange.has_value())
                    {
                        return writer_.emitValueTracked(json_scalar_to_string(*value), arg.policy, current_source_uri(),
                                                        to_tracking_range(*arg.sourceRange));
                    }
                    return writer_.emitValue(json_scalar_to_string(*value), arg.policy);
                }
                else if constexpr (std::is_same_v<T, Instruction_AlignCell>)
                {
                    return writer_.emit("");
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<ForInstr>>)
                {
                    return exec_for(*arg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<IfInstr>>)
                {
                    return exec_if(*arg);
                }
                else if constexpr (std::is_same_v<T, std::unique_ptr<SwitchInstr>>)
                {
                    return exec_switch(*arg);
                }
                else if constexpr (std::is_same_v<T, CallInstr>)
                {
                    return exec_call(arg);
                }
                else if constexpr (std::is_same_v<T, BeginCapturedBlockInstr>)
                {
                    if (arg.blockIndentInParentBlock.has_value())
                        return writer_.beginCapturedBlock(*arg.blockIndentInParentBlock);
                    return writer_.beginCapturedBlock();
                }
                else if constexpr (std::is_same_v<T, Instruction_EmitCapturedBlock>)
                {
                    return writer_.emitCapturedBlock();
                }
            }, instr.value);
        }

        bool exec_for(const ForInstr &instr)
        {
            return exec_with_structural_tracking(instr.sourceRange, [&]() {
                const bool hasPolicy = !instr.policy.empty();
                if (hasPolicy && !writer_.pushPolicy(instr.policy))
                    return false;

                const nlohmann::json *collection = resolve_expr(instr.collection);
                if (!collection || !collection->is_array())
                {
                    if (hasPolicy)
                        return writer_.popPolicy();
                    return true;
                }

                const bool ok = instr.alignSpec.has_value()
                    ? exec_for_aligned(instr, *collection)
                    : exec_for_normal(instr, *collection);

                if (hasPolicy && !writer_.popPolicy())
                    return false;
                return ok;
            });
        }

        bool exec_for_normal(const ForInstr &instr, const nlohmann::json &collection)
        {
            const auto &items = collection;
            const auto capturedBlockInfo = analyze_captured_block(*instr.body);

            if (capturedBlockInfo.wrapped)
            {
                Writer::NativeLoopOptions options;
                options.precededBy = instr.precededBy;
                options.sep = instr.sep;
                options.followedBy = instr.followedBy;

                if (capturedBlockInfo.blockIndentInParentBlock.has_value())
                {
                    return writer_.emitCapturedBlockForEach(items, *capturedBlockInfo.blockIndentInParentBlock, options, [&](const auto &item, int enumerator) {
                        std::map<std::string, nlohmann::json> bindings;
                        bindings.emplace(instr.varName, item);
                        if (instr.enumeratorName.has_value())
                            bindings.emplace(*instr.enumeratorName, enumerator);

                        scopes_.push_back(std::move(bindings));
                        exec_body_range(*instr.body, capturedBlockInfo.firstIndex, capturedBlockInfo.pastLastIndex);
                        scopes_.pop_back();
                    });
                }

                return writer_.emitCapturedBlockForEach(items, options, [&](const auto &item, int enumerator) {
                    std::map<std::string, nlohmann::json> bindings;
                    bindings.emplace(instr.varName, item);
                    if (instr.enumeratorName.has_value())
                        bindings.emplace(*instr.enumeratorName, enumerator);

                    scopes_.push_back(std::move(bindings));
                    exec_body_range(*instr.body, capturedBlockInfo.firstIndex, capturedBlockInfo.pastLastIndex);
                    scopes_.pop_back();
                });
            }

            for (size_t index = 0; index < items.size(); ++index)
            {
                std::map<std::string, nlohmann::json> bindings;
                bindings.emplace(instr.varName, items[index]);
                if (instr.enumeratorName.has_value())
                    bindings.emplace(*instr.enumeratorName, static_cast<int>(index));

                scopes_.push_back(std::move(bindings));
                writer_.beginCapture();
                const bool ok = exec_body(*instr.body);
                Writer::CaptureResult iterResult = writer_.endCaptureResult();
                scopes_.pop_back();
                if (!ok)
                    return false;

                if (instr.precededBy.has_value())
                    writer_.emit(*instr.precededBy);

                writer_.emitCaptureResult(iterResult);

                if (index + 1 < items.size())
                {
                    if (instr.sep.has_value())
                        writer_.emit(*instr.sep);
                }
                else
                {
                    if (instr.followedBy.has_value() && !items.empty())
                        writer_.emit(*instr.followedBy);
                }
            }

            return true;
        }

        bool exec_for_aligned(const ForInstr &instr, const nlohmann::json &collection)
        {
            const auto cells = split_cells(*instr.body);
            std::vector<std::vector<std::string>> rows;
            rows.reserve(collection.size());

            for (size_t index = 0; index < collection.size(); ++index)
            {
                std::map<std::string, nlohmann::json> bindings;
                bindings.emplace(instr.varName, collection[index]);
                if (instr.enumeratorName.has_value())
                    bindings.emplace(*instr.enumeratorName, static_cast<int>(index));

                scopes_.push_back(std::move(bindings));

                std::vector<std::string> row;
                row.reserve(cells.size());
                for (const auto &cell : cells)
                {
                    writer_.beginCapture();
                    bool ok = true;
                    for (const auto *cellInstr : cell)
                    {
                        if (!exec_instruction(*cellInstr))
                        {
                            ok = false;
                            break;
                        }
                    }
                    std::string cellText = writer_.endCapture();
                    if (!ok)
                    {
                        scopes_.pop_back();
                        return false;
                    }
                    if (Writer::containsLineBreak(cellText))
                    {
                        writer_.error() = "aligned cells must be single-line";
                        scopes_.pop_back();
                        return false;
                    }
                    row.push_back(std::move(cellText));
                }

                scopes_.pop_back();
                rows.push_back(std::move(row));
            }

            size_t numCols = 0;
            for (const auto &row : rows)
                numCols = std::max(numCols, row.size());

            std::vector<char> colSpec(numCols, 'l');
            if (instr.alignSpec.has_value() && !instr.alignSpec->empty())
            {
                const std::string &spec = *instr.alignSpec;
                if (spec.size() == 1)
                {
                    std::fill(colSpec.begin(), colSpec.end(), spec[0]);
                }
                else
                {
                    for (size_t column = 0; column < colSpec.size() && column < spec.size(); ++column)
                        colSpec[column] = spec[column];
                }
            }

            std::vector<int> colWidth(numCols, 0);
            for (const auto &row : rows)
                for (size_t column = 0; column < row.size(); ++column)
                    colWidth[column] = std::max(colWidth[column], static_cast<int>(row[column].size()));

            for (size_t index = 0; index < rows.size(); ++index)
            {
                const std::string iterResult = Writer::renderAlignedRow(rows[index], colWidth, colSpec, numCols);
                if (instr.precededBy.has_value())
                    writer_.emit(*instr.precededBy);
                writer_.emit(iterResult);
                if (index + 1 < rows.size())
                {
                    if (instr.sep.has_value())
                        writer_.emit(*instr.sep);
                }
                else if (instr.followedBy.has_value())
                {
                    writer_.emit(*instr.followedBy);
                }
            }

            return true;
        }

        bool exec_if(const IfInstr &instr)
        {
            return exec_with_structural_tracking(instr.sourceRange, [&]() {
                bool cond = expr_truthy(instr.condExpr);
                if (instr.negated)
                    cond = !cond;

                const auto &branch = cond ? *instr.thenBody : *instr.elseBody;
                return exec_body(branch);
            });
        }

        bool exec_switch(const SwitchInstr &instr)
        {
            return exec_with_structural_tracking(instr.sourceRange, [&]() {
                const bool hasPolicy = !instr.policy.empty();
                if (hasPolicy && !writer_.pushPolicy(instr.policy))
                    return false;

                const nlohmann::json *value = resolve_expr(instr.expr);
                if (!value)
                {
                    if (hasPolicy)
                        return writer_.popPolicy();
                    return true;
                }

                std::string tag;
                const nlohmann::json *payload = nullptr;
                if (!extract_enum_case(*value, tag, payload))
                {
                    if (hasPolicy)
                        return writer_.popPolicy();
                    return true;
                }

                for (const auto &caseInstr : *instr.cases)
                {
                    if (caseInstr.tag != tag)
                        continue;

                    std::map<std::string, nlohmann::json> bindings;
                    if (caseInstr.bindingName.has_value() && payload)
                        bindings.emplace(caseInstr.bindingName->name, *payload);

                    const bool ok = exec_with_structural_tracking(caseInstr.sourceRange, [&]() {
                        scopes_.push_back(std::move(bindings));
                        const bool branchOk = exec_body(*caseInstr.body);
                        scopes_.pop_back();
                        return branchOk;
                    });

                    if (hasPolicy && !writer_.popPolicy())
                        return false;
                    return ok;
                }

                if (hasPolicy && !writer_.popPolicy())
                    return false;
                return true;
            });
        }

        bool exec_call(const CallInstr &instr)
        {
            if (instr.functionIndex < 0 ||
                instr.functionIndex >= static_cast<int>(ir_.functions.size()))
            {
                writer_.error() = "render call: invalid function index for '" + instr.functionName + "'";
                return false;
            }

            const FunctionDef &callee = ir_.functions[static_cast<size_t>(instr.functionIndex)];
            const std::string callSiteUri = current_source_uri();
            std::map<std::string, nlohmann::json> bindings;
            for (size_t index = 0; index < instr.arguments.size() && index < callee.params.size(); ++index)
            {
                const nlohmann::json *value = resolve_expr(instr.arguments[index]);
                bindings.emplace(callee.params[index].name, value ? *value : nlohmann::json());
            }

            scopes_.push_back(std::move(bindings));
            writer_.beginCapture();
            sourceUriStack_.push_back(callee.sourceUri.value_or(""));

            const bool hasPolicy = !callee.policy.empty();
            if (hasPolicy && !writer_.pushPolicy(callee.policy))
            {
                writer_.endCapture();
                sourceUriStack_.pop_back();
                scopes_.pop_back();
                return false;
            }

            const bool ok = exec_body(*callee.body);
            Writer::CaptureResult callResult = writer_.endCaptureResult();

            if (hasPolicy && !writer_.popPolicy())
            {
                sourceUriStack_.pop_back();
                scopes_.pop_back();
                return false;
            }

            sourceUriStack_.pop_back();
            scopes_.pop_back();
            if (!ok)
                return false;

            strip_trailing_newline(callResult);

            const int outStart = static_cast<int>(writer_.outputSize());
            if (!writer_.emitCaptureResult(callResult))
                return false;

            if (instr.sourceRange.has_value())
            {
                writer_.addMapping(callSiteUri, to_tracking_range(*instr.sourceRange),
                                   outStart, static_cast<int>(writer_.outputSize()));
            }

            return true;
        }

        const IR &ir_;
        Writer writer_;
        std::vector<std::map<std::string, nlohmann::json>> scopes_;
        std::vector<std::string> sourceUriStack_;
    };

    IRInterpreter::IRInterpreter(const IR &ir)
        : impl_(std::make_unique<Impl>(ir))
    {
    }

    IRInterpreter::~IRInterpreter() = default;

    bool IRInterpreter::render(const FunctionDef &function,
                               std::map<std::string, nlohmann::json> bindings)
    {
        return impl_->render(function, std::move(bindings));
    }

    std::string IRInterpreter::take_output(std::vector<RenderMapping> *tracking)
    {
        return impl_->take_output(tracking);
    }

    const std::string &IRInterpreter::error() const
    {
        return impl_->error();
    }
} // namespace tpp