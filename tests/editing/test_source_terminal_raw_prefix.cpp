#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite source_terminal_raw_prefix_tests = [] {

"unclosed raw source retains terminal prefixes without a physical ending"_test = [] {
    for (const auto marker : {U"```", U"~~~", U"$$", U"\\["})
    for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}})
    for (const auto prefix : {U">", U"> ", U"> >", U"> > ", U"  ", U"  > "}) {
        const auto opening_prefix = std::u32string_view(prefix).starts_with(U"  ")
            ? (std::u32string_view(prefix).find(U'>') == std::u32string_view::npos ? U"- " : U"- > ")
            : prefix;
        const auto before = std::u32string(opening_prefix) + marker + ending + prefix;
        Editor editor(cps_to_utf8(before));
        expect(fatal(editor.markdown_utf8() == cps_to_utf8(before))) << std::quoted(cps_to_utf8(before));
        expect(validate_document(editor.document()).empty());
        expect(fatal(editor.enter_source_mode()));
        editor.set_source_selection(SourceSelection::caret(before.size()));
        expect(fatal(editor.execute_command(Command::InsertText(U"x"))));
        expect(bool(editor.source_editor()->source() == before + U"x"));
        expect(editor.undo());
        expect(editor.source_editor()->source() == before);
        expect(editor.redo());
        expect(editor.exit_source_mode());
        expect(editor.markdown_utf8() == cps_to_utf8(before + U"x"));
    }
};

"source deletion may leave a raw terminal prefix in either direction"_test = [] {
    for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}})
    for (const auto keep_body : {false, true})
    for (const auto terminal : {U">", U"> "})
    for (const auto reverse : {false, true})
    for (const auto deletion : {CommandKind::DeleteBackward, CommandKind::DeleteForward}) {
        const auto before = U"> ```" + ending + U"> code" + ending + U"> ```";
        const auto expected = U"> ```" + ending + (keep_body ? U"> code" + ending : std::u32string{}) + terminal;
        Editor editor(cps_to_utf8(before));
        expect(fatal(editor.enter_source_mode()));
        const SourceSelection selected = reverse ? SourceSelection{before.size(), expected.size()}
            : SourceSelection{expected.size(), before.size()};
        editor.set_source_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = deletion}))) << std::quoted(cps_to_utf8(expected));
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0u && counts.full_document_serializations == 0u
            && counts.full_tree_transaction_diffs == 0u);
        expect(editor.source_editor()->source() == expected);
        expect(editor.undo());
        expect(editor.source_editor()->source() == before && editor.source_editor()->selection() == selected);
        expect(editor.redo());
        expect(editor.exit_source_mode());
        expect(editor.markdown_utf8() == cps_to_utf8(expected));
    }
};

};
