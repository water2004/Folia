#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

const std::vector<std::string> source_inputs{
    "", " ", "\n", "\n\n", " \n \n", "text", "text\n", "text\n\n", "a\n\nb\n\nc\n\nd",
    "# title", "Title\n---", "---", "***", "[TOC]", "![alt](image.png)",
    "*text*", "[label](target \"title\")", "[ref][r]\n\n[r]: /target",
    "- one\n- two", "1. first\n2. second", "- [ ] one\n- [x] two",
    "- first\n\n  second\n\n- third", "- first\n  - nested\n- last",
    "> quote\n> continued", "> first\n>\n> second", "> [!NOTE]\n> body",
    "> [!TIP] Title\n>\n> body\n>", "> > nested\n> > again", "[^n]: footnote\n\nreference[^n]",
    "```cpp\nvalue\n```", "```cpp\nvalue\n", "    code\n    next", "$$\nx+1\n$$",
    "> ```\n> code\n> ```", "- ```\n  code\n  ```",
    "| a | b |\n|---|---|\n|x|y|", "<div>\nbody\n</div>", "<p>body &amp; value</p>",
    "<!-- comment -->", "---\ntitle: sample\n---\n\nbody", "+++\ntitle = 'test'\n+++",
    "text\r\nnext\r\n", "\r\n\r\n", "> first\r\n>\r\n> second\r\n", "one\rtwo\r",
    cps_to_utf8(U"e\u0301 中文 👩\u200d💻 🇨🇳"),
    cps_to_utf8(U"- e\u0301\r\n- 中文 👩\u200d💻\r\n"),
    cps_to_utf8(U"> [!NOTE]\r\n> e\u0301\r\n>\r\n> 中文"),
    cps_to_utf8(U"```\r\n👩\u200d💻\r\n```"),
};

bool verify_source_operation(const std::string& input, SourceSelection selected,
    SourceRange replaced, std::u32string_view inserted, const Command& command,
    std::size_t& failures) {
    const auto before = utf8_to_cps(input);
    auto expected = before;
    expected.replace(replaced.start, replaced.length(), inserted);
    const auto after = SourceSelection::caret(replaced.start + inserted.size());
    const auto changed = before != expected;
    const auto effect = changed || selected != after;
    Editor editor(input);
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(selected);
    reset_core_operation_counters();
    const auto applied = editor.execute_command(command);
    const auto counters = read_core_operation_counters();
    auto good = true;
    const auto check = [&](bool condition, std::string_view phase) {
        expect(condition) << phase << " input=" << std::quoted(input)
            << " anchor=" << selected.anchor << " active=" << selected.active
            << " replacement=" << std::quoted(cps_to_utf8(inserted));
        good = good && condition;
    };
    check(applied == effect, "command outcome");
    check(editor.source_editor()->source() == expected, "direct source splice");
    check(editor.source_editor()->selection() == after, "replacement selection");
    check(counters.full_document_parses == 0u
        && counters.full_document_serializations == 0u
        && counters.full_tree_transaction_diffs == 0u, "bounded transaction");
    check(validate_document(editor.document()).empty(), "document invariants");
    if (!good) {
        Editor fresh(cps_to_utf8(expected));
        std::cerr << "expected=" << std::quoted(cps_to_utf8(expected))
            << " fresh save=" << std::quoted(fresh.markdown_utf8()) << '\n';
        return ++failures < 24;
    }
    if (changed) {
        check(editor.undo(), "source undo");
        check(editor.source_editor()->source() == before
            && editor.source_editor()->selection() == selected, "exact source undo");
        check(editor.redo(), "source redo");
        check(editor.source_editor()->source() == expected
            && editor.source_editor()->selection() == after, "exact source redo");
    } else {
        check(!editor.has_undo(), "selection-only edit has no history entry");
    }
    check(editor.exit_source_mode(), "rendered mode");
    check(editor.markdown_utf8() == cps_to_utf8(expected), "rendered save");
    if (changed) {
        check(editor.undo(), "rendered undo");
        check(editor.markdown_utf8() == input, "exact rendered undo");
        check(editor.redo(), "rendered redo");
        check(editor.markdown_utf8() == cps_to_utf8(expected), "exact rendered redo");
    }
    check(editor.enter_source_mode(), "return to source mode");
    check(editor.source_editor()->source() == expected, "source projection after view switch");
    Editor reloaded(cps_to_utf8(expected));
    check(reloaded.markdown_utf8() == cps_to_utf8(expected), "exact reload");
    check(validate_document(reloaded.document()).empty(), "reloaded invariants");
    if (!good) ++failures;
    return failures < 24;
}

} // namespace

suite source_operation_acceptance_tests = [] {

"source per-offset insertion and directional deletion are accepted"_test = [] {
    std::size_t failures = 0;
    for (const auto& input : source_inputs) {
        const auto before = utf8_to_cps(input);
        for (std::size_t offset = 0; offset <= before.size(); ++offset) {
            for (const auto& inserted : {std::u32string{U"x"}, std::u32string{U"\n"},
                std::u32string{U" "}, std::u32string{U"\r"}, std::u32string{U"`"},
                std::u32string{U"*"}, std::u32string{U">"}, std::u32string{U"\r\n"}}) {
                if (!verify_source_operation(input, SourceSelection::caret(offset), {offset, offset},
                    inserted, Command::InsertText(inserted), failures)) return;
            }
            const auto previous = prev_grapheme_boundary_char(before, offset);
            if (!verify_source_operation(input, SourceSelection::caret(offset), {previous, offset}, {},
                {.kind = CommandKind::DeleteBackward}, failures)) return;
            const auto next = next_grapheme_boundary_char(before, offset);
            if (!verify_source_operation(input, SourceSelection::caret(offset), {offset, next}, {},
                {.kind = CommandKind::DeleteForward}, failures)) return;
        }
    }
};

"source range replacement is exact in both selection directions"_test = [] {
    std::size_t failures = 0;
    for (const auto& input : source_inputs) {
        const auto before = utf8_to_cps(input);
        for (std::size_t start = 0; start < before.size(); ++start) {
            // Every starting offset crosses an adjacent character, a short
            // local range, and the document tail. Small inputs use all spans.
            std::vector<std::size_t> ends;
            if (before.size() <= 6) {
                for (auto end = start + 1; end <= before.size(); ++end) ends.push_back(end);
            } else {
                ends = {start + 1, (std::min)(start + 4, before.size()), before.size()};
                ends.erase(std::unique(ends.begin(), ends.end()), ends.end());
            }
            for (const auto end : ends) {
                for (const auto reverse : {false, true}) {
                    const SourceSelection selected = reverse
                        ? SourceSelection{end, start, TextAffinity::Upstream, TextAffinity::Downstream}
                        : SourceSelection{start, end, TextAffinity::Downstream, TextAffinity::Upstream};
                    for (const auto& inserted : {std::u32string{}, std::u32string{U"X"},
                        std::u32string{U"\n"}, std::u32string{U"中\n👩\u200d💻e\u0301\r\n"}}) {
                        if (!verify_source_operation(input, selected, {start, end}, inserted,
                            Command::InsertText(inserted), failures)) return;
                    }
                    for (const auto deletion : {CommandKind::DeleteBackward, CommandKind::DeleteForward}) {
                        if (!verify_source_operation(input, selected, {start, end}, {},
                            {.kind = deletion}, failures)) return;
                    }
                }
            }
        }
    }
};

};
