// Source-view commands produce text patches, committed by Editor through the
// same authoritative document/history adapter as ordinary source typing.
export module folia.core.source_commands;
import std;
import folia.core.command;
import folia.core.source_editor;
import folia.core.document_inline_format;
import folia.core.document_edit_primitives;
import folia.markdown.dialect;
import folia.markdown.ids;
import folia.markdown.text_edit;
import folia.markdown.selection;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.serializer;
import folia.markdown.block_line_recognizer;
import folia.markdown.callout;
import folia.markdown.utf;

export namespace folia {
namespace source_command_detail {

inline bool horizontal_space(char32_t c) { return c == U' ' || c == U'\t'; }

inline std::size_t line_at(const SourceEditor& editor, std::size_t offset) {
    const auto& lines = editor.lines();
    auto found = std::upper_bound(lines.begin(), lines.end(), offset,
        [](std::size_t value, const SourceLine& line) { return value < line.source_start; });
    return found == lines.begin() ? 0 : static_cast<std::size_t>(found - lines.begin() - 1);
}

inline std::pair<std::size_t, std::size_t> selected_lines(const SourceEditor& editor) {
    const auto range = editor.selection().ordered_range();
    const auto first = line_at(editor, range.start);
    auto last = line_at(editor, range.end);
    if (!range.empty() && last > first && range.end == editor.lines()[last].source_start) --last;
    return {first, last};
}

inline bool apply_prefix_edits(SourceEditor& editor, std::vector<SourceReplacement> edits) {
    if (edits.empty()) return false;
    std::ranges::sort(edits, {}, [](const auto& edit) { return edit.range.start; });
    auto after = editor.selection();
    for (auto edit = edits.rbegin(); edit != edits.rend(); ++edit) {
        TextEdit translation{{}, edit->range, edit->replacement};
        after.anchor = translate_offset(after.anchor, TextAffinity::Downstream, translation);
        after.active = translate_offset(after.active, TextAffinity::Downstream, translation);
    }
    return editor.replace_all_with_selection(edits, after);
}

inline bool toggle_inline(SourceEditor& editor, InlineFormat format, const MarkdownDialect& dialect) {
    auto [first, last] = selected_lines(editor);
    // A selected physical line can be inside a multiline inline construct.
    // Include its paragraph context so toggling from a continuation line or
    // from a selection ending at the next line's start sees both delimiters.
    const auto boundary = [](const SourceLine& line) {
        return line.state_before.in_fence() || line.state_after.in_fence()
            || std::ranges::all_of(line.text, horizontal_space);
    };
    while (first != 0 && !boundary(editor.lines()[first]) && !boundary(editor.lines()[first - 1])) --first;
    while (last + 1 < editor.lines().size() && !boundary(editor.lines()[last])
        && !boundary(editor.lines()[last + 1])) ++last;
    const auto begin = editor.lines()[first].source_start;
    const auto end = (std::max)(editor.lines()[last].source_end(), editor.selection().ordered_range().end);
    InlineDocument inline_document;
    inline_document.source = editor.source().substr(begin, end - begin);
    InlineParseContext context;
    context.dialect = dialect;
    reparse_inline_document(inline_document, context);
    const auto selected = editor.selection();
    TextSelection local{
        {NodeId{1}, selected.anchor - begin, selected.anchor_affinity},
        {NodeId{1}, selected.active - begin, selected.active_affinity}};
    const auto range = SourceRange{(std::min)(local.anchor.source_offset, local.active.source_offset),
        (std::max)(local.anchor.source_offset, local.active.source_offset)};
    using namespace document_inline_format_detail;
    const auto existing = find_innermost_format(inline_document.tree.nodes, cst_kind(format),
        range.start, range.end, local.is_caret());
    const auto markers = default_markers(format, inline_document.syntax_mode);
    if (!existing && !markers) return false;
    auto change = existing
        ? remove_existing_format(NodeId{1}, inline_document, local, range, *existing)
        : add_format(NodeId{1}, inline_document, local, range, *markers);
    SourceSelection after{begin + change.selection_after.anchor.source_offset,
        begin + change.selection_after.active.source_offset,
        change.selection_after.anchor.affinity, change.selection_after.active.affinity};
    return editor.replace_with_selection({begin + change.range.start, begin + change.range.end},
        std::move(change.replacement), after);
}

inline std::size_t indentation_end(std::u32string_view line) {
    std::size_t cursor = 0;
    while (cursor < line.size() && horizontal_space(line[cursor])) ++cursor;
    return cursor;
}

inline std::size_t quote_prefix_end(std::u32string_view line) {
    auto cursor = indentation_end(line);
    while (cursor < line.size() && line[cursor] == U'>') {
        ++cursor;
        if (cursor < line.size() && horizontal_space(line[cursor])) ++cursor;
    }
    return cursor;
}

struct ListPrefix {
    std::size_t start = 0;
    std::size_t end = 0;
    bool present = false;
    bool ordered = false;
    bool task = false;
    bool checked = false;
    std::size_t checkbox = 0;
};

inline ListPrefix list_prefix(std::u32string_view line) {
    ListPrefix result;
    auto cursor = result.start = result.end = quote_prefix_end(line);
    if (cursor >= line.size()) return result;
    if (line[cursor] == U'-' || line[cursor] == U'+' || line[cursor] == U'*') ++cursor;
    else {
        const auto begin = cursor;
        while (cursor < line.size() && line[cursor] >= U'0' && line[cursor] <= U'9'
            && cursor - begin < 9) ++cursor;
        if (cursor == begin || cursor >= line.size() || (line[cursor] != U'.' && line[cursor] != U')')) return result;
        result.ordered = true;
        ++cursor;
    }
    if (cursor >= line.size() || !horizontal_space(line[cursor])) return result;
    while (cursor < line.size() && horizontal_space(line[cursor])) ++cursor;
    result.present = true;
    if (cursor + 2 < line.size() && line[cursor] == U'[' && line[cursor + 2] == U']'
        && (line[cursor + 1] == U' ' || line[cursor + 1] == U'x' || line[cursor + 1] == U'X')
        && (cursor + 3 == line.size() || horizontal_space(line[cursor + 3]))) {
        result.task = true;
        result.checkbox = cursor + 1;
        result.checked = line[result.checkbox] != U' ';
        cursor += 3;
        while (cursor < line.size() && horizontal_space(line[cursor])) ++cursor;
    }
    result.end = cursor;
    return result;
}

inline bool line_format(SourceEditor& editor, const Command& command) {
    auto [first, last] = selected_lines(editor);
    const auto& lines = editor.lines();
    std::vector<SourceReplacement> edits;
    auto add = [&](const SourceLine& line, std::size_t begin, std::size_t end, std::u32string replacement) {
        if (line.text.substr(begin, end - begin) != replacement)
            edits.push_back({{line.source_start + begin, line.source_start + end}, std::move(replacement)});
    };
    if (command.kind == CommandKind::SetHeading || command.kind == CommandKind::ClearHeading) {
        const auto level = command.kind == CommandKind::ClearHeading ? 0 : (std::min)(unsigned(command.level), 6u);
        auto underline = [&](std::size_t index) {
            if (index >= lines.size() || lines[index].code_content || lines[index].state_before.in_fence()) return false;
            const auto& text = lines[index].text;
            const auto begin = quote_prefix_end(text);
            return detail::BlockLineRecognizer(std::u32string_view(text).substr(begin)).setext_underline_at(0).has_value();
        };
        auto title = [&](std::size_t index) {
            const auto& line = lines[index];
            if (line.code_content || line.state_before.in_fence() || line.state_after.in_fence()) return false;
            const auto prefix = list_prefix(line.text);
            const auto begin = prefix.present ? prefix.end : quote_prefix_end(line.text);
            if (begin == line.text.size()) return false;
            if (!prefix.present && begin >= 4 && line.text.substr(0, begin).find(U'>') == std::u32string::npos) return false;
            return !detail::BlockLineRecognizer(std::u32string_view(line.text).substr(begin)).line_starts_interrupting_block(0);
        };
        if (first != 0 && underline(first) && title(first - 1)) --first;
        std::unordered_set<std::size_t> removed_underlines;
        for (auto index = first; index <= last; ++index) {
            if (removed_underlines.contains(index)) continue;
            const auto& line = lines[index];
            if (index + 1 < lines.size() && title(index) && underline(index + 1)) {
                edits.push_back({{line.source_end(), lines[index + 1].source_end()}, {}});
                removed_underlines.insert(index + 1);
            }
            const auto prefix = list_prefix(line.text);
            const auto start = prefix.present ? prefix.end : quote_prefix_end(line.text);
            auto end = start;
            while (end < line.text.size() && line.text[end] == U'#') ++end;
            if (end - start > 6 || (end < line.text.size() && !horizontal_space(line.text[end]))) end = start;
            if (end > start) while (end < line.text.size() && horizontal_space(line.text[end])) ++end;
            if (level == 0 && end > start) {
                auto tail = line.text.size();
                while (tail > end && horizontal_space(line.text[tail - 1])) --tail;
                auto closing = tail;
                while (closing > end && line.text[closing - 1] == U'#') --closing;
                if (closing < tail && (closing == end || horizontal_space(line.text[closing - 1]))) {
                    while (closing > end && horizontal_space(line.text[closing - 1])) --closing;
                    add(line, closing, line.text.size(), {});
                }
            }
            add(line, start, end, level ? std::u32string(level, U'#') + U" " : std::u32string{});
        }
    } else if (command.kind == CommandKind::ToggleBlockQuote) {
        bool all_quoted = true;
        for (auto index = first; index <= last; ++index) {
            const auto& text = lines[index].text;
            auto start = indentation_end(text);
            all_quoted = all_quoted && start < text.size() && text[start] == U'>';
        }
        for (auto index = first; index <= last; ++index) {
            const auto& line = lines[index];
            auto start = indentation_end(line.text);
            auto end = start;
            if (all_quoted) {
                ++end;
                if (end < line.text.size() && horizontal_space(line.text[end])) ++end;
            }
            add(line, start, end, all_quoted ? U"" : U"> ");
        }
    } else {
        const auto ordered = command.kind == CommandKind::ToggleOrderedList;
        const auto task = command.kind == CommandKind::ToggleTaskList;
        const auto checkbox = command.kind == CommandKind::ToggleTaskCheckbox;
        bool all_match = true, all_checked = true;
        for (auto index = first; index <= last; ++index) {
            auto prefix = list_prefix(lines[index].text);
            all_match = all_match && prefix.present && prefix.task == task && (task || prefix.ordered == ordered);
            all_checked = all_checked && prefix.task && prefix.checked;
        }
        for (auto index = first; index <= last; ++index) {
            const auto& line = lines[index];
            auto prefix = list_prefix(line.text);
            if (checkbox) {
                if (prefix.task) add(line, prefix.checkbox, prefix.checkbox + 1, all_checked ? U" " : U"x");
                continue;
            }
            std::u32string marker;
            if (!all_match) marker = task ? U"- [ ] "
                : ordered ? utf8_to_cps(std::to_string(index - first + 1)) + U". " : U"- ";
            add(line, prefix.start, prefix.end, std::move(marker));
        }
    }
    return apply_prefix_edits(editor, std::move(edits));
}

inline std::size_t longest_run(std::u32string_view source, char32_t marker) {
    std::size_t longest = 0, current = 0;
    for (auto value : source) {
        current = value == marker ? current + 1 : 0;
        longest = (std::max)(longest, current);
    }
    return longest;
}

inline bool insert_block(SourceEditor& editor, const Command& command) {
    const auto range = editor.selection().ordered_range();
    const auto ending = editor.preferred_line_ending();
    const auto selected = editor.selected_text();
    std::u32string block;
    std::size_t caret = 0;
    if (command.kind == CommandKind::InsertCodeBlock || command.kind == CommandKind::InsertMathBlock) {
        auto language = command.lang.value_or(std::u32string{});
        if (language.find_first_of(U"\r\n") != std::u32string::npos) return false;
        const auto code = command.kind == CommandKind::InsertCodeBlock;
        const auto marker = language.find(U'`') == std::u32string::npos ? U'`' : U'~';
        const auto fence = code ? std::u32string((std::max)(std::size_t{3}, longest_run(selected, marker) + 1), marker)
            : std::u32string{U"$$"};
        block = fence;
        if (code && !language.empty() && language.front() == marker) block += U' ';
        if (code) block += language;
        block += ending;
        caret = block.size();
        block += selected;
        if (selected.empty() || (selected.back() != U'\n' && selected.back() != U'\r')) block += ending;
        block += fence;
    } else if (command.kind == CommandKind::InsertToc) {
        block = U"[TOC]";
        caret = block.size();
    } else if (command.kind == CommandKind::InsertTable) {
        const auto columns = (std::max)(command.cols, std::size_t{1});
        auto row = [&](std::u32string_view cell) {
            block += U'|';
            for (std::size_t column = 0; column < columns; ++column) block += U" " + std::u32string(cell) + U" |";
        };
        row(U"Header");
        caret = 2;
        block += ending;
        row(U"---");
        for (std::size_t index = 0; index < command.rows; ++index) { block += ending; row(U"Cell"); }
    } else return false;

    std::u32string before, after;
    if (range.start > 0) {
        const auto preceding = std::u32string_view(editor.source()).substr(0, range.start);
        if (!preceding.ends_with(ending + ending)) before = preceding.ends_with(ending) ? ending : ending + ending;
    }
    if (range.end < editor.source().size()) {
        const auto following = std::u32string_view(editor.source()).substr(range.end);
        if (!following.starts_with(ending + ending)) after = following.starts_with(ending) ? ending : ending + ending;
    }
    caret += range.start + before.size();
    return editor.replace_with_selection(range, before + block + after, SourceSelection::caret(caret));
}

inline bool callout(SourceEditor& editor, const Command& command) {
    auto [first, last] = selected_lines(editor);
    const auto& lines = editor.lines();
    const auto normalized = normalize_callout_kind(command.callout_kind.empty()
        ? std::string{"NOTE"} : cps_to_utf8(command.callout_kind));
    if (!normalized) return false;
    auto kind = utf8_to_cps(*normalized);
    auto quote = [&](std::size_t index) -> std::optional<SourceRange> {
        const auto& text = lines[index].text;
        auto begin = indentation_end(text);
        if (begin >= text.size() || text[begin] != U'>') return std::nullopt;
        auto end = begin + 1;
        if (end < text.size() && horizontal_space(text[end])) ++end;
        return SourceRange{begin, end};
    };
    auto run_first = first;
    if (quote(first)) {
        while (run_first != 0 && quote(run_first - 1)) --run_first;
    }
    const auto& header = lines[run_first];
    const auto header_quote = quote(run_first);
    const auto marker = header_quote ? header_quote->end : 0;
    const auto close = header_quote && std::u32string_view(header.text).substr(marker).starts_with(U"[!")
        ? header.text.find(U']', marker + 2) : std::u32string::npos;
    std::vector<SourceReplacement> edits;
    const auto existing_kind = close != std::u32string::npos
        ? normalize_callout_kind(cps_to_utf8(std::u32string_view(header.text).substr(marker + 2, close - marker - 2)))
        : std::nullopt;
    if (existing_kind) {
        if (*existing_kind != *normalized)
            return apply_prefix_edits(editor, {{{header.source_start + marker + 2,
                header.source_start + close}, std::move(kind)}});
        first = run_first;
        last = first;
        while (last + 1 < lines.size() && quote(last + 1)) ++last;
        auto title = close + 1;
        while (title < header.text.size() && horizontal_space(header.text[title])) ++title;
        if (title == header.text.size()) {
            // Remove only the header line; blank body lines remain literal
            // lines and the live source selection follows the prefix edits.
            edits.push_back({{header.source_start, header.next_source_start()}, {}});
        } else {
            edits.push_back({{header.source_start + header_quote->start, header.source_start + title}, {}});
        }
        for (auto index = first + 1; index <= last; ++index) {
            const auto prefix = *quote(index);
            edits.push_back({{lines[index].source_start + prefix.start,
                lines[index].source_start + prefix.end}, {}});
        }
        return apply_prefix_edits(editor, std::move(edits));
    }

    const auto ending = editor.preferred_line_ending();
    auto all_quoted = true;
    for (auto index = first; index <= last; ++index) all_quoted = all_quoted && quote(index).has_value();
    if (all_quoted) {
        // A normal quote becomes a callout at its existing nesting level.
        // Keep the quote body's exact prefixes instead of introducing a
        // second quote around each line.
        const auto begin = lines[run_first].source_start;
        const auto prefix = *quote(run_first);
        auto heading = lines[run_first].text.substr(0, prefix.start) + U"> [!" + kind + U"]" + ending;
        return apply_prefix_edits(editor, {{{begin, begin}, std::move(heading)}});
    }
    const auto heading = U"> [!" + kind + U"]" + ending;
    for (auto index = first; index <= last; ++index) {
        const auto begin = lines[index].source_start;
        edits.push_back({{begin, begin}, index == first ? heading + U"> " : std::u32string{U"> "}});
    }
    return apply_prefix_edits(editor, std::move(edits));
}

inline bool footnote(SourceEditor& editor, const Command& command, std::string_view fresh_label) {
    const auto label = utf8_to_cps(command.kind == CommandKind::CreateFootnoteDefinition
        ? std::string_view(command.footnote_label) : fresh_label);
    if (label.empty() || label.find_first_of(U"[]\r\n") != std::u32string::npos) return false;
    const auto reference = U"[^" + label + U"]";
    const auto ending = editor.preferred_line_ending();
    auto definition = (editor.source().empty() && command.kind == CommandKind::CreateFootnoteDefinition
        ? U"" : ending + ending) + reference + U": ";
    const auto end = editor.source().size();
    const auto offset = editor.selection().ordered_range().end;
    if (command.kind == CommandKind::CreateFootnoteDefinition)
        return editor.replace({end, end}, std::move(definition));
    if (offset == end) return editor.replace({end, end}, reference + definition);
    const std::array edits{SourceReplacement{{offset, offset}, reference},
        SourceReplacement{{end, end}, definition}};
    return editor.replace_all_with_selection(edits,
        SourceSelection::caret(end + reference.size() + definition.size()));
}

inline bool code_language(SourceEditor& editor, const Command& command) {
    const auto& lines = editor.lines();
    auto index = line_at(editor, editor.selection().active);
    while (index > 0 && lines[index].state_before.in_fence()) --index;
    const auto fence = source_editor_detail::fence_at(lines[index].text);
    if (!fence) return false;
    auto language = command.lang.value_or(std::u32string{});
    if (language.find_first_of(U"\r\n") != std::u32string::npos
        || (fence->first == U'`' && language.find(U'`') != std::u32string::npos)) return false;
    if (!language.empty() && language.front() == fence->first) language.insert(language.begin(), U' ');
    auto begin = source_editor_detail::leading_spaces(lines[index].text) + fence->second;
    return apply_prefix_edits(editor, {{{lines[index].source_start + begin, lines[index].source_end()}, std::move(language)}});
}

} // namespace source_command_detail

inline bool execute_source_markup_command(SourceEditor& editor, const Command& command,
    const MarkdownDialect& dialect, std::string_view fresh_footnote_label = {}) {
    using namespace source_command_detail;
    switch (command.kind) {
        case CommandKind::ToggleStrong: return toggle_inline(editor, InlineFormat::Strong, dialect);
        case CommandKind::ToggleEmphasis: return toggle_inline(editor, InlineFormat::Emphasis, dialect);
        case CommandKind::ToggleStrikethrough: return toggle_inline(editor, InlineFormat::Strikethrough, dialect);
        case CommandKind::ToggleInlineCode: return toggle_inline(editor, InlineFormat::Code, dialect);
        case CommandKind::InsertMathInline: return toggle_inline(editor, InlineFormat::Math, dialect);
        case CommandKind::SetHeading:
        case CommandKind::ClearHeading:
        case CommandKind::ToggleBlockQuote:
        case CommandKind::ToggleUnorderedList:
        case CommandKind::ToggleOrderedList:
        case CommandKind::ToggleTaskList:
        case CommandKind::ToggleTaskCheckbox: return line_format(editor, command);
        case CommandKind::InsertCodeBlock:
        case CommandKind::InsertMathBlock:
        case CommandKind::InsertTable:
        case CommandKind::InsertToc: return insert_block(editor, command);
        case CommandKind::ToggleCallout: return callout(editor, command);
        case CommandKind::InsertFootnote:
        case CommandKind::CreateFootnoteDefinition: return footnote(editor, command, fresh_footnote_label);
        case CommandKind::SetCodeBlockLanguage: return code_language(editor, command);
        case CommandKind::InsertImage: {
            const auto alt = command.alt.empty() ? editor.selected_text() : command.alt;
            return editor.replace_selection(serialize_image_source(command.path.empty() ? U"image.png" : command.path, alt));
        }
        case CommandKind::InsertLink: {
            const auto range = editor.selection().ordered_range();
            const auto label = serialize_link_label(editor.selected_text());
            auto replacement = serialize_link_source(command.href, editor.selected_text(), command.title);
            return editor.replace_with_selection(range, std::move(replacement),
                SourceSelection::caret(range.start + 1 + label.size()));
        }
        default: return false;
    }
}

} // namespace folia
