#pragma once

#include <tpp/Policy.h>

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpp
{

    class Writer
    {
    public:
        struct NativeLoopOptions
        {
            std::optional<std::string_view> precededBy;
            std::optional<std::string_view> sep;
            std::optional<std::string_view> followedBy;
            bool stripSingleTrailingNewline = false;
        };

        explicit Writer(std::map<std::string, TppPolicy> namedPolicies = {})
            : namedPolicies_(std::move(namedPolicies))
        {
            outputStack_.emplace_back();
        }

        bool emit(std::string_view text)
        {
            output(text);
            return true;
        }

        bool emitValue(const std::string &value)
        {
            emitWithMultilineIndent(value);
            return true;
        }

        bool emitValue(std::string value, const TppPolicy &policy)
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

            emitWithMultilineIndent(value);
            return true;
        }

        bool emitValue(std::string value, const std::string &exprPolicy)
        {
            const std::string effectivePolicy = exprPolicy.empty() ? activePolicy_ : exprPolicy;
            if (effectivePolicy.empty() || effectivePolicy == "none")
                return emitValue(value);

            const auto it = namedPolicies_.find(effectivePolicy);
            if (it == namedPolicies_.end())
            {
                error_ = "unknown policy '" + effectivePolicy + "'";
                return false;
            }

            return emitValue(std::move(value), it->second);
        }

        bool pushPolicy(const std::string &tag)
        {
            policyScopeStack_.push_back(activePolicy_);
            activePolicy_ = tag;
            return true;
        }

        bool popPolicy()
        {
            if (policyScopeStack_.empty())
            {
                error_ = "popPolicy: scope stack underflow";
                return false;
            }

            activePolicy_ = policyScopeStack_.back();
            policyScopeStack_.pop_back();
            return true;
        }

        bool pushIndent(int indentColumns)
        {
            indentStack_.push_back(indentColumns);
            beginCapture();
            return true;
        }

        bool popIndent()
        {
            if (indentStack_.empty())
            {
                error_ = "popIndent: scope stack underflow";
                return false;
            }

            const int indentColumns = indentStack_.back();
            indentStack_.pop_back();
            return emit(blockIndent(endCapture(), indentColumns));
        }

        template <typename Body>
        bool emitBlock(int indentColumns, Body &&body)
        {
            beginCapture();
            try
            {
                body();
            }
            catch (...)
            {
                endCapture();
                throw;
            }

            if (hasError())
            {
                endCapture();
                return false;
            }

            const std::string raw = endCapture();
            if (!raw.empty())
                return emit(blockIndent(raw, indentColumns));
            return true;
        }

        template <typename Collection, typename Body>
        bool emitForEach(const Collection &collection,
                         const NativeLoopOptions &options,
                         Body &&body)
        {
            beginForEach(collection.size());
            struct Guard
            {
                Writer &writer;
                bool active = true;
                ~Guard()
                {
                    if (active)
                        writer.endForEach();
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
                else if (options.followedBy.has_value() && !collection.empty())
                {
                    if (!emitOptionalText(options.followedBy))
                        return false;
                }
            }

            return !hasError();
        }

        template <typename Collection, typename Body>
        bool emitBlockForEach(const Collection &collection,
                              int indentColumns,
                              const NativeLoopOptions &options,
                              Body &&body)
        {
            beginForEach(collection.size());
            struct Guard
            {
                Writer &writer;
                bool active = true;
                ~Guard()
                {
                    if (active)
                        writer.endForEach();
                }
            } guard{*this};

            while (nextForEach())
            {
                const auto index = forEachIndex();
                beginCapture();
                try
                {
                    body(collection[index], forEachEnumerator());
                }
                catch (...)
                {
                    endCapture();
                    throw;
                }

                if (hasError())
                {
                    endCapture();
                    return false;
                }

                std::string iterResult = blockIndent(endCapture(), indentColumns);
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
        bool captureAlignedCell(size_t cellIndex, Body &&body)
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

            beginCapture();
            try
            {
                body();
            }
            catch (...)
            {
                endCapture();
                throw;
            }

            if (hasError())
            {
                endCapture();
                return false;
            }

            std::string captured = endCapture();
            if (containsLineBreak(captured))
            {
                error_ = "aligned cells must be single-line";
                return false;
            }

            state.currentRow[cellIndex] = std::move(captured);
            return true;
        }

        template <typename Collection, typename Body>
        bool emitAlignedForEach(const Collection &collection,
                                size_t numCols,
                                const std::vector<char> &alignSpecChars,
                                bool singleAlignChar,
                                const NativeLoopOptions &options,
                                Body &&body)
        {
            beginFor(numCols, alignSpecChars, singleAlignChar, collection.size());
            struct Guard
            {
                Writer &writer;
                bool active = true;
                ~Guard()
                {
                    if (active)
                        writer.finishFor();
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

        void beginCapture()
        {
            outputStack_.emplace_back();
        }

        std::string endCapture()
        {
            std::string captured = std::move(outputStack_.back());
            outputStack_.pop_back();
            return captured;
        }

        std::string endBlockCapture(int indentColumns)
        {
            return blockIndent(endCapture(), indentColumns);
        }

        std::string takeOutput()
        {
            return outputStack_.empty() ? std::string{} : std::move(outputStack_.front());
        }

        std::string &error()
        {
            return error_;
        }

        const std::string &error() const
        {
            return error_;
        }

        bool hasError() const
        {
            return !error_.empty();
        }

        static bool stripSingleTrailingNewline(std::string &text)
        {
            if (text.empty() || text.back() != '\n')
                return false;
            if (std::count(text.begin(), text.end(), '\n') != 1)
                return false;
            text.pop_back();
            return true;
        }

        static bool containsLineBreak(const std::string &text)
        {
            return text.find_first_of("\r\n") != std::string::npos;
        }

        static std::string renderAlignedRow(const std::vector<std::string> &row,
                                            const std::vector<int> &colWidth,
                                            const std::vector<char> &colSpec,
                                            size_t numCols)
        {
            std::string line;
            for (size_t column = 0; column < row.size(); ++column)
            {
                if (column + 1 < numCols)
                {
                    line += padCell(row[column], colWidth[column], colSpec[column]);
                }
                else
                {
                    const char spec = colSpec[column];
                    const int pad = colWidth[column] - static_cast<int>(row[column].size());
                    if (pad > 0 && spec != 'l')
                    {
                        const int left = (spec == 'c') ? pad / 2 : pad;
                        line += std::string(static_cast<size_t>(left), ' ');
                    }
                    line += row[column];
                }
            }
            return line;
        }

    private:
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

        static std::string trim(const std::string &text)
        {
            const size_t start = text.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                return "";
            const size_t end = text.find_last_not_of(" \t\r\n");
            return text.substr(start, end - start + 1);
        }

        static std::string blockIndent(const std::string &raw, int indentColumns)
        {
            if (raw.empty())
                return "";

            std::vector<std::string> lines;
            std::istringstream input(raw);
            std::string line;
            while (std::getline(input, line))
                lines.push_back(line);
            const bool trailingNewline = !raw.empty() && raw.back() == '\n';

            std::string zeroMarker;
            for (const auto &currentLine : lines)
            {
                if (trim(currentLine).empty())
                    continue;

                size_t width = 0;
                while (width < currentLine.size() &&
                       (currentLine[width] == ' ' || currentLine[width] == '\t'))
                {
                    ++width;
                }

                zeroMarker = currentLine.substr(0, width);
                break;
            }

            const std::string indent(static_cast<size_t>(indentColumns), ' ');
            std::string result;
            for (size_t index = 0; index < lines.size(); ++index)
            {
                std::string currentLine = lines[index];
                if (!currentLine.empty() && !zeroMarker.empty() &&
                    currentLine.size() >= zeroMarker.size() &&
                    currentLine.compare(0, zeroMarker.size(), zeroMarker) == 0)
                {
                    currentLine = currentLine.substr(zeroMarker.size());
                }

                if (!currentLine.empty())
                    result += indent + currentLine;
                if (index + 1 < lines.size() || trailingNewline)
                    result += '\n';
            }

            return result;
        }

        static std::string padCell(const std::string &text, int width, char spec)
        {
            const int len = static_cast<int>(text.size());
            if (len >= width)
                return text;

            const int pad = width - len;
            if (spec == 'r')
                return std::string(static_cast<size_t>(pad), ' ') + text;
            if (spec == 'c')
            {
                const int left = pad / 2;
                const int right = pad - left;
                return std::string(static_cast<size_t>(left), ' ') + text +
                       std::string(static_cast<size_t>(right), ' ');
            }
            return text + std::string(static_cast<size_t>(pad), ' ');
        }

        void output(std::string_view text)
        {
            activeOutput().append(text.data(), text.size());
        }

        std::string &activeOutput()
        {
            return outputStack_.back();
        }

        void emitWithMultilineIndent(const std::string &text)
        {
            auto &out = activeOutput();
            const auto lastNewline = out.rfind('\n');
            const size_t column = (lastNewline == std::string::npos) ? out.size() : out.size() - lastNewline - 1;

            if (column > 0 && text.find('\n') != std::string::npos)
            {
                const std::string pad(column, ' ');
                std::string indented;
                size_t start = 0;
                while (true)
                {
                    const auto end = text.find('\n', start);
                    if (start > 0)
                        indented += pad;
                    indented += text.substr(start, end == std::string::npos ? end : end - start);
                    if (end == std::string::npos)
                        break;
                    indented += '\n';
                    start = end + 1;
                }
                output(indented);
            }
            else
            {
                output(text);
            }
        }

        bool emitOptionalText(const std::optional<std::string_view> &text)
        {
            return !text.has_value() || emit(*text);
        }

        void beginForEach(size_t size)
        {
            forEachStack_.push_back(ForEachState{size, 0, false});
        }

        bool nextForEach()
        {
            auto &state = forEachStack_.back();
            if (!state.started)
            {
                state.started = true;
            }
            else
            {
                ++state.index;
            }
            return state.index < state.size;
        }

        size_t forEachIndex() const
        {
            return forEachStack_.back().index;
        }

        bool forEachHasNext() const
        {
            const auto &state = forEachStack_.back();
            return state.index + 1 < state.size;
        }

        int forEachEnumerator() const
        {
            return static_cast<int>(forEachIndex());
        }

        void endForEach()
        {
            if (!forEachStack_.empty())
                forEachStack_.pop_back();
        }

        void beginFor(size_t numCols,
                      const std::vector<char> &alignSpecChars,
                      bool singleAlignChar,
                      size_t rowReserve)
        {
            ForState state;
            state.rows.reserve(rowReserve);
            state.colWidth.assign(numCols, 0);
            state.colSpec.assign(numCols, 'l');
            state.currentRow.assign(numCols, "");
            state.numCols = numCols;

            if (!alignSpecChars.empty())
            {
                if (singleAlignChar)
                {
                    std::fill(state.colSpec.begin(), state.colSpec.end(), alignSpecChars.front());
                }
                else
                {
                    for (size_t index = 0; index < state.colSpec.size() && index < alignSpecChars.size(); ++index)
                        state.colSpec[index] = alignSpecChars[index];
                }
            }

            forStack_.push_back(std::move(state));
        }

        std::string endFor(size_t rowIndex) const
        {
            const auto &state = activeForState();
            if (rowIndex >= state.rows.size())
                return "";
            return renderAlignedRow(state.rows[rowIndex], state.colWidth, state.colSpec, state.numCols);
        }

        size_t forSize() const
        {
            return activeForState().rows.size();
        }

        void finishFor()
        {
            if (forStack_.empty())
            {
                error_ = "finishFor: no active aligned-for session";
                return;
            }

            abortAlignedRow();
            forStack_.pop_back();
        }

        ForState &activeForState()
        {
            return forStack_.back();
        }

        const ForState &activeForState() const
        {
            return forStack_.back();
        }

        bool startAlignedRow()
        {
            auto &state = activeForState();
            if (state.rowOpen)
            {
                error_ = "startAlignedRow: row already open";
                return false;
            }

            state.currentRow.assign(state.numCols, "");
            state.rowOpen = true;
            return true;
        }

        bool finishAlignedRow()
        {
            auto &state = activeForState();
            if (!state.rowOpen)
            {
                error_ = "finishAlignedRow: no active row";
                return false;
            }

            for (size_t index = 0; index < state.currentRow.size(); ++index)
                state.colWidth[index] = std::max(state.colWidth[index], static_cast<int>(state.currentRow[index].size()));

            state.rows.push_back(state.currentRow);
            state.currentRow.clear();
            state.rowOpen = false;
            return true;
        }

        void abortAlignedRow()
        {
            if (forStack_.empty())
                return;

            auto &state = forStack_.back();
            state.currentRow.clear();
            state.rowOpen = false;
        }

        std::map<std::string, TppPolicy> namedPolicies_;
        std::vector<std::string> outputStack_;
        std::vector<int> indentStack_;
        std::string activePolicy_;
        std::vector<std::string> policyScopeStack_;
        std::vector<ForState> forStack_;
        std::vector<ForEachState> forEachStack_;
        std::string error_;
    };

} // namespace tpp