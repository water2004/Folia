#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite source_word_command_tests = [] {

"source word commands move extend and collapse selections"_test = [] {
    Editor editor("alpha beta");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(10));
    expect(editor.execute_command({.kind = CommandKind::MoveWordLeft}));
    expect(editor.source_editor()->selection() == SourceSelection::caret(6));
    expect(editor.execute_command({.kind = CommandKind::MoveWordRight, .extend_selection = true}));
    expect(editor.source_editor()->selection().anchor == 6u);
    expect(editor.source_editor()->selection().active == 10u);
    expect(editor.execute_command({.kind = CommandKind::MoveWordLeft}));
    expect(editor.source_editor()->selection() == SourceSelection::caret(6));

    editor.set_source_selection({10, 2});
    expect(editor.execute_command({.kind = CommandKind::MoveWordRight}));
    expect(editor.source_editor()->selection() == SourceSelection::caret(10));
    editor.set_source_selection({10, 2});
    expect(editor.execute_command({.kind = CommandKind::MoveWordLeft}));
    expect(editor.source_editor()->selection() == SourceSelection::caret(2));
    expect(!editor.has_undo());
};

"source word deletion uses the shared history and preserves view switching"_test = [] {
    for (const auto backward : {false, true}) {
        Editor editor("alpha beta");
        expect(fatal(editor.enter_source_mode()));
        const auto before = SourceSelection::caret(backward ? 10 : 6);
        editor.set_source_selection(before);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = backward
            ? CommandKind::DeleteWordBackward : CommandKind::DeleteWordForward})));
        const auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
        expect(editor.source_editor()->source() == U"alpha ");
        expect(editor.source_editor()->selection() == SourceSelection::caret(6));
        expect(fatal(validate_document(editor.document()).empty()));
        expect(editor.undo());
        expect(editor.source_editor()->source() == U"alpha beta");
        expect(editor.source_editor()->selection() == before);
        expect(editor.redo());
        expect(editor.exit_source_mode());
        expect(editor.markdown_utf8() == "alpha ");
        expect(editor.undo());
        expect(editor.markdown_utf8() == "alpha beta");
        expect(editor.enter_source_mode());
        expect(editor.source_editor()->selection() == before);
    }
};

"source word deletion replaces either selection direction before finding a boundary"_test = [] {
    for (const auto kind : {CommandKind::DeleteWordBackward, CommandKind::DeleteWordForward}) {
        for (const auto reversed : {false, true}) {
            Editor editor("first\r\nsecond");
            expect(fatal(editor.enter_source_mode()));
            const auto before = reversed ? SourceSelection{10, 3} : SourceSelection{3, 10};
            editor.set_source_selection(before);
            expect(editor.execute_command({.kind = kind}));
            expect(editor.source_editor()->source() == U"firond");
            expect(editor.source_editor()->selection() == SourceSelection::caret(3));
            expect(editor.undo());
            expect(editor.source_editor()->source() == U"first\r\nsecond");
            expect(editor.source_editor()->selection() == before);
        }
    }
};

"source word commands cannot split combining or emoji clusters"_test = [] {
    const std::u32string source = U"e\u0301lan \U0001f469\u200d\U0001f4bb \U0001f1e8\U0001f1f3\r\nnext";
    auto is_boundary = [&](std::size_t offset) {
        std::size_t cursor = 0;
        while (cursor < offset) cursor = next_grapheme_boundary_char(source, cursor);
        return cursor == offset;
    };
    SourceEditor editor(source);
    for (auto offset = std::size_t{0}; offset < source.size();) {
        editor.set_selection(SourceSelection::caret(offset));
        editor.move_word_right();
        expect(editor.selection().active > offset);
        expect(is_boundary(editor.selection().active));
        offset = next_grapheme_boundary_char(source, offset);
    }
    for (auto offset = source.size(); offset != 0;) {
        editor.set_selection(SourceSelection::caret(offset));
        editor.move_word_left();
        expect(editor.selection().active < offset);
        expect(is_boundary(editor.selection().active));
        offset = prev_grapheme_boundary_char(source, offset);
    }
    editor.set_selection(SourceSelection::caret(0));
    expect(!editor.delete_word_backward());
    editor.set_selection(SourceSelection::caret(source.size()));
    expect(!editor.delete_word_forward());
    expect(editor.source() == source);
};

"source paragraph commands advance over blank separated multiline runs"_test = [] {
    const std::u32string source = U"first\nwrap\n\n \nsecond\r\nmore\n\nlast";
    Editor editor(cps_to_utf8(source));
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(3));
    expect(editor.execute_command({.kind = CommandKind::MoveParagraphEnd}));
    expect(editor.source_editor()->selection().active == source.find(U"\n\n"));
    expect(editor.execute_command({.kind = CommandKind::MoveParagraphEnd}));
    expect(editor.source_editor()->selection().active == source.find(U"\n\nlast"));
    expect(editor.execute_command({.kind = CommandKind::MoveParagraphEnd}));
    expect(editor.source_editor()->selection().active == source.size());
    expect(editor.execute_command({.kind = CommandKind::MoveParagraphStart}));
    expect(editor.source_editor()->selection().active == source.find(U"last"));
    expect(editor.execute_command({.kind = CommandKind::MoveParagraphStart}));
    expect(editor.source_editor()->selection().active == source.find(U"second"));
    expect(editor.execute_command({.kind = CommandKind::MoveParagraphStart, .extend_selection = true}));
    expect(editor.source_editor()->selection().active == 0u);
    expect(editor.source_editor()->selection().anchor == source.find(U"second"));
    expect(editor.source_editor()->source() == source);
    expect(!editor.has_undo());
};

"source paragraph commands leave whitespace and empty document boundaries valid"_test = [] {
    for (const auto source : {U"", U"\n\n", U" \r\n\t\r\n "}) {
        SourceEditor editor(source);
        editor.move_paragraph_end();
        expect(editor.selection().active == editor.source().size());
        editor.move_paragraph_start();
        expect(editor.selection().active == 0u);
        editor.move_paragraph_start(true);
        expect(editor.selection().is_caret());
        expect(editor.source() == source);
    }
};

};
