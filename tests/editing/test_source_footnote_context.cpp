#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.document_footnotes;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.symbols;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite source_footnote_context_tests = [] {

"source footnotes remain definitions before trailing unclosed raw blocks"_test = [] {
    for (const auto marker : {U"```cpp", U"~~~", U"$$", U"\\[", U"<!--", U"<?pi", U"<!DOCTYPE", U"<![CDATA[", U"<script>"})
    for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}})
    for (const auto leading : {false, true})
    for (const auto definition_only : {false, true}) {
        const auto prefix = leading ? U"lead" + ending + ending : std::u32string{};
        const auto before = prefix + marker + ending + U"body" + ending;
        const auto reference = definition_only ? std::u32string{U"[^named]"} : std::u32string{U"[^1]"};
        const auto label = definition_only ? "named" : "1";
        for (const auto offset : {std::size_t{0}, before.find(U"body") + 2, before.size()})
        for (const auto reverse : {false, true}) {
            Editor editor(cps_to_utf8(before));
            expect(fatal(editor.enter_source_mode()));
            const SourceSelection selected = reverse ? SourceSelection{offset, 0} : SourceSelection{0, offset};
            editor.set_source_selection(selected);
            auto expected = before;
            if (!definition_only) expected.insert(offset, reference);
            const auto definition_offset = prefix.size()
                + (!definition_only && offset < prefix.size() ? reference.size() : 0);
            const auto definition = reference + U": " + ending + ending;
            expected.insert(definition_offset, definition);
            const auto target = SourceSelection::caret(definition_offset + reference.size() + 2);
            reset_core_operation_counters();
            const auto applied = editor.execute_command(definition_only
                ? Command{.kind = CommandKind::CreateFootnoteDefinition, .footnote_label = label}
                : Command{.kind = CommandKind::InsertFootnote});
            const auto counts = read_core_operation_counters();
            expect(fatal(applied)) << std::quoted(cps_to_utf8(before)) << offset;
            expect(counts.full_document_parses == 0u && counts.full_document_serializations == 0u
                && counts.full_tree_transaction_diffs == 0u);
            expect(editor.source_editor()->source() == expected);
            expect(editor.source_editor()->selection() == target);
            expect(find_footnote_definition(editor.symbols(), label) != nullptr);
            expect(validate_document(editor.document()).empty());
            expect(editor.undo());
            expect(editor.source_editor()->source() == before && editor.source_editor()->selection() == selected);
            expect(editor.redo());
            expect(editor.source_editor()->source() == expected && editor.source_editor()->selection() == target);
            expect(editor.exit_source_mode());
            expect(editor.markdown_utf8() == cps_to_utf8(expected));
            Editor reloaded(cps_to_utf8(expected));
            expect(reloaded.markdown_utf8() == cps_to_utf8(expected));
            expect(find_footnote_definition(reloaded.symbols(), label) != nullptr);
        }
    }
};

"source footnote definition appends normally after closed and nested raw blocks"_test = [] {
    for (const auto before : {std::u32string{U""}, std::u32string{U"```\nbody\n```"},
        std::u32string{U"> ```\n> body"}, std::u32string{U"- ```\n  body"}}) {
        Editor editor(cps_to_utf8(before));
        expect(fatal(editor.enter_source_mode()));
        const auto suffix = (before.empty() ? std::u32string{} : std::u32string{U"\n\n"}) + U"[^named]: ";
        expect(fatal(editor.execute_command({.kind = CommandKind::CreateFootnoteDefinition, .footnote_label = "named"})));
        expect(bool(editor.source_editor()->source() == before + suffix));
        expect(editor.source_editor()->selection() == SourceSelection::caret(before.size() + suffix.size()));
        expect(find_footnote_definition(editor.symbols(), "named") != nullptr);
        expect(editor.exit_source_mode());
        Editor reloaded(editor.markdown_utf8());
        expect(find_footnote_definition(reloaded.symbols(), "named") != nullptr);
        expect(editor.undo());
        expect(editor.markdown_utf8() == cps_to_utf8(before));
    }
};

};
