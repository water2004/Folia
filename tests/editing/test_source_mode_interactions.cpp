#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

void expect_source_command_is_local() {
    const auto counters = read_core_operation_counters();
    expect(counters.full_document_parses == 0u);
    expect(counters.full_document_serializations == 0u);
    expect(counters.full_tree_transaction_diffs == 0u);
}

void expect_source_lines_match_fresh(const SourceEditor& editor) {
    const SourceEditor fresh(editor.source());
    expect(fatal(editor.lines().size() == fresh.lines().size()));
    std::unordered_set<std::uint64_t> identities;
    std::u32string reconstructed;
    for (std::size_t index = 0; index < editor.lines().size(); ++index) {
        const auto& line = editor.lines()[index];
        const auto& expected = fresh.lines()[index];
        expect(line.source_start == expected.source_start) << index;
        expect(line.text == expected.text) << index;
        expect(line.line_ending == expected.line_ending) << index;
        expect(line.state_before == expected.state_before) << index;
        expect(line.state_after == expected.state_after) << index;
        expect(line.styles == expected.styles) << index;
        expect(line.presentation_key == expected.presentation_key) << index;
        expect(line.id.v != 0 && identities.insert(line.id.v).second) << index;
        reconstructed += line.text;
        reconstructed += line.line_ending;
        for (auto offset : {line.source_start, line.source_end()}) {
            const auto position = editor.position_from_source_offset(offset);
            expect(editor.source_offset_from_position(position) == offset) << index;
        }
    }
    expect(reconstructed == editor.source());
}

}

suite source_mode_interactions = [] {

"source interaction Enter uses the surrounding physical line ending"_test = [] {
    struct Case {
        std::u32string source;
        SourceSelection selection;
        std::u32string expected;
    };
    for (const auto& test : std::vector<Case>{
        {U"a\r\nb", SourceSelection::caret(1), U"a\r\n\r\nb"},
        {U"a\r\nb", SourceSelection::caret(4), U"a\r\nb\r\n"},
        {U"a\rb", SourceSelection::caret(3), U"a\rb\r"},
        {U"a\nb", SourceSelection::caret(3), U"a\nb\n"},
        {U"a\rb\r\nc\nend", SourceSelection::caret(3), U"a\rb\r\n\r\nc\nend"},
        {U"first\r\nsecond\r\nlast", {16, 3}, U"fir\r\nast"},
        {U"plain", SourceSelection::caret(5), U"plain\n"},
        {U"", SourceSelection::caret(0), U"\n"},
    }) {
        SourceEditor editor(test.source);
        editor.set_selection(test.selection);
        expect(fatal(editor.insert_newline()));
        expect(fatal(editor.source() == test.expected)) << cps_to_utf8(test.source);
        expect_source_lines_match_fresh(editor);
        const auto after_selection = editor.selection();
        auto patch = editor.take_patch();
        expect(fatal(patch.has_value()));
        expect(fatal(editor.apply_projection_patch(*patch, false)));
        expect(editor.source() == test.source);
        expect(editor.selection() == test.selection);
        expect_source_lines_match_fresh(editor);
        expect(fatal(editor.apply_projection_patch(*patch, true)));
        expect(editor.source() == test.expected);
        expect(editor.selection() == after_selection);
    }
};

"source interaction repair includes edited or newly joined CRLF terminators"_test = [] {
    struct Case { std::u32string source; SourceRange range; std::u32string replacement; };
    for (const auto& test : std::vector<Case>{
        {U"a\r\nb", {2, 2}, U"X"},
        {U"a\r\nb", {1, 2}, U""},
        {U"a\r\nb", {2, 3}, U""},
        {U"a\r\nb", {1, 3}, U"\n"},
        {U"a\rb", {2, 2}, U"\n"},
        {U"\r\n", {1, 1}, U"\r"},
        {U"a\r\n\r\nb", {3, 4}, U"X"},
        {U"a\r\nb\r\nc", {2, 5}, U"\n"},
        {U"a\r\n```\r\nx\r\n```\r\nz", {2, 2}, U"X"},
        {U"a\r\n```\r\nx\r\n```\r\nz", {3, 3}, U"\n"},
    }) {
        SourceEditor editor(test.source);
        editor.set_selection({test.range.end, test.range.start});
        auto expected = test.source;
        expected.replace(test.range.start, test.range.length(), test.replacement);
        expect(fatal(editor.replace(test.range, test.replacement)));
        expect(fatal(editor.source() == expected));
        expect_source_lines_match_fresh(editor);
        auto patch = editor.take_patch();
        expect(fatal(patch.has_value()));
        expect(fatal(editor.apply_projection_patch(*patch, false)));
        expect(fatal(editor.source() == test.source));
        expect_source_lines_match_fresh(editor);
        expect(fatal(editor.apply_projection_patch(*patch, true)));
        expect_source_lines_match_fresh(editor);
    }
};

"source interaction vertical navigation preserves a column through short lines"_test = [] {
    SourceEditor editor(U"abcd\r\nx\r\nwxyz");
    editor.set_selection(SourceSelection::caret(3));
    editor.move_down();
    expect(editor.selection() == SourceSelection::caret(7));
    editor.move_down();
    expect(editor.selection() == SourceSelection::caret(12));
    editor.move_up();
    expect(editor.selection() == SourceSelection::caret(7));
    editor.move_up();
    expect(editor.selection() == SourceSelection::caret(3));

    editor.move_down(true);
    expect(editor.selection().anchor == 3u && editor.selection().active == 7u);
    editor.move_down(true);
    expect(editor.selection().anchor == 3u && editor.selection().active == 12u);
    editor.set_selection(SourceSelection::caret(7));
    editor.move_left();
    editor.move_down();
    expect(editor.selection() == SourceSelection::caret(9));
    editor.move_document_start();
    editor.move_up();
    expect(editor.selection() == SourceSelection::caret(0));
    editor.move_document_end();
    editor.move_down();
    expect(editor.selection() == SourceSelection::caret(editor.source().size()));
};

"source interaction vertical navigation does not split a combining grapheme"_test = [] {
    SourceEditor editor(U"ab\ne\u0301x\nyz");
    editor.set_selection(SourceSelection::caret(1));
    editor.move_down();
    expect(editor.selection() == SourceSelection::caret(3));
    editor.move_down();
    expect(editor.selection() == SourceSelection::caret(8));
    editor.move_up();
    expect(editor.selection() == SourceSelection::caret(3));
    editor.move_up();
    expect(editor.selection() == SourceSelection::caret(1));
};

"source interaction identical replacement collapses selection without a text patch"_test = [] {
    SourceEditor editor(U"alpha beta");
    editor.set_selection({10, 6});
    const auto revision = editor.revision();
    expect(editor.insert_text(U"beta"));
    expect(editor.selection() == SourceSelection::caret(10));
    expect(editor.source() == U"alpha beta");
    expect(editor.revision() == revision);
    expect(!editor.take_patch().has_value());
    expect(!editor.insert_text(U""));
    expect(!editor.replace_with_selection({0, 5}, U"x", SourceSelection::caret(100)));
    expect(editor.source() == U"alpha beta");
};

"source interaction mixed tab indentation removes one complete visual level"_test = [] {
    SourceEditor editor(U" \tone\r\n  \t  two\r\n\t three\r\n    four\r\nend");
    const auto before = editor.source();
    const SourceSelection selection{before.size() - 3, 1, TextAffinity::Upstream, TextAffinity::Downstream};
    editor.set_selection(selection);
    expect(fatal(editor.outdent()));
    expect(editor.source() == U"one\r\n  two\r\n three\r\nfour\r\nend");
    expect(editor.selection().active == 0u);
    expect(editor.selection().anchor == editor.source().size() - 3);
    expect_source_lines_match_fresh(editor);
    auto patch = editor.take_patch();
    expect(fatal(patch.has_value()));
    expect(fatal(editor.apply_projection_patch(*patch, false)));
    expect(editor.source() == before);
    expect(editor.selection() == selection);
    expect(fatal(editor.apply_projection_patch(*patch, true)));
    expect_source_lines_match_fresh(editor);
};

"source interaction disjoint replacements record one explicit resulting selection"_test = [] {
    SourceEditor editor(U"text\n\ntail");
    const auto original = editor.source();
    editor.set_selection(SourceSelection::caret(2));
    const std::array edits{
        SourceReplacement{{2, 2}, U"[^n]"},
        SourceReplacement{{10, 10}, U"\n\n[^n]: "},
    };
    const SourceSelection after{22, 22, TextAffinity::Upstream, TextAffinity::Upstream};
    expect(fatal(editor.replace_all_with_selection(edits, after)));
    expect(editor.source() == U"te[^n]xt\n\ntail\n\n[^n]: ");
    expect(editor.selection() == after);
    auto patch = editor.take_patch();
    expect(fatal(patch.has_value()));
    expect(patch->edits.size() == 2u);
    expect(fatal(editor.apply_projection_patch(*patch, false)));
    expect(editor.source() == original);
    expect(editor.selection() == SourceSelection::caret(2));
    expect(fatal(editor.apply_projection_patch(*patch, true)));
    expect_source_lines_match_fresh(editor);

    const auto unchanged = editor.source();
    const std::array duplicate{
        SourceReplacement{{1, 1}, U"a"}, SourceReplacement{{1, 1}, U"b"},
    };
    expect(!editor.replace_all_with_selection(duplicate, SourceSelection::caret(3)));
    expect(editor.source() == unchanged);
    expect(!editor.replace_all_with_selection(std::span<const SourceReplacement>{edits.data(), 1},
        SourceSelection::caret(1000)));
    expect(editor.source() == unchanged);
};

"source interaction editor commands preserve history across both view modes"_test = [] {
    const std::vector<std::string> inputs{
        "", "text", "![alt](image.png)", "---", "- one\n- two",
        "> [!NOTE] title\n> body", "[^n]: footnote\n\nreference[^n]",
        "```cpp\nvalue\n```", "$$\nx+1\n$$", "<div>\nbody\n</div>",
        "| a | b |\n|---|---|\n|x|y|", "---\ntitle: sample\n---\n\nbody",
    };
    for (const auto& input : inputs) {
        for (auto kind : {CommandKind::InsertText, CommandKind::Paste, CommandKind::InsertNewline,
            CommandKind::DeleteBackward, CommandKind::DeleteForward}) {
            for (auto location : {0u, 1u, 2u}) {
                Editor editor(input);
                expect(fatal(editor.enter_source_mode()));
                const auto before = editor.source_editor()->source();
                const auto offset = location == 0 ? 0 : location == 1 ? before.size() / 2 : before.size();
                const auto selection = SourceSelection::caret(offset);
                editor.set_source_selection(selection);
                Command command;
                command.kind = kind;
                command.text = kind == CommandKind::Paste ? U"[x](url)\n---\n" : U"X";
                auto expected = before;
                bool changes = true;
                if (kind == CommandKind::DeleteBackward) {
                    if (offset == 0) changes = false;
                    else expected.erase(offset - 1, 1);
                } else if (kind == CommandKind::DeleteForward) {
                    if (offset == before.size()) changes = false;
                    else expected.erase(offset, 1);
                } else {
                    expected.insert(offset, kind == CommandKind::InsertNewline ? U"\n" : command.text);
                }
                reset_core_operation_counters();
                const auto applied = editor.execute_command(command);
                expect_source_command_is_local();
                expect(fatal(applied == changes)) << input << " command " << static_cast<int>(kind) << " at " << offset;
                expect(fatal(editor.source_editor()->source() == expected));
                expect(fatal(editor.markdown_utf8() == cps_to_utf8(expected)));
                expect(fatal(validate_document(editor.document()).empty()));
                expect_source_lines_match_fresh(*editor.source_editor());
                if (!changes) continue;
                expect(fatal(editor.exit_source_mode()));
                reset_core_operation_counters();
                expect(fatal(editor.undo()));
                expect_source_command_is_local();
                expect(fatal(editor.markdown_utf8() == cps_to_utf8(before)));
                expect(fatal(editor.enter_source_mode()));
                reset_core_operation_counters();
                expect(fatal(editor.redo()));
                expect_source_command_is_local();
                expect(fatal(editor.source_editor()->source() == expected));
                expect(fatal(validate_document(editor.document()).empty()));
                reset_core_operation_counters();
                expect(fatal(editor.undo()));
                expect_source_command_is_local();
                expect(fatal(editor.source_editor()->source() == before));
                expect(fatal(editor.source_editor()->selection() == selection));
            }
        }
    }
};

"source interaction Enter keeps CRLF through editor history and mode switches"_test = [] {
    Editor editor("alpha\r\nbeta");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(3));
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
    expect(editor.source_editor()->source() == U"alp\r\nha\r\nbeta");
    expect(fatal(editor.exit_source_mode()));
    expect(editor.markdown_utf8() == "alp\r\nha\r\nbeta");
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == "alpha\r\nbeta");
    expect(fatal(editor.redo()));
    expect(fatal(editor.enter_source_mode()));
    expect(editor.source_editor()->source() == U"alp\r\nha\r\nbeta");
    expect(editor.source_editor()->selection() == SourceSelection::caret(5));
};

"source interaction core vertical commands and identical paste remain handled"_test = [] {
    Editor editor("longer\nx\nlonger");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(4));
    expect(fatal(editor.execute_command(Command::MoveDown(false))));
    expect(editor.source_editor()->selection() == SourceSelection::caret(8));
    expect(fatal(editor.execute_command(Command::MoveDown(true))));
    expect(editor.source_editor()->selection().anchor == 8u);
    expect(editor.source_editor()->selection().active == 13u);
    expect(fatal(editor.execute_command(Command::MoveUp(false))));
    expect(editor.source_editor()->selection() == SourceSelection::caret(8));
    editor.set_source_selection({6, 0});
    Command paste;
    paste.kind = CommandKind::Paste;
    paste.text = U"longer";
    expect(fatal(editor.execute_command(paste)));
    expect(editor.source_editor()->selection() == SourceSelection::caret(6));
    expect(!editor.has_undo());
};

};
