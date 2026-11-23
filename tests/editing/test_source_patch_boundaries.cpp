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

suite source_patch_boundary_tests = [] {

"source deletion of a final paragraph body transfers newline ownership"_test = [] {
    for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}}) {
        const auto before = U"a" + ending + ending + U"b" + ending + ending + U"c" + ending + ending + U"d";
        const auto expected = before.substr(0, before.size() - 1);
        for (const auto backward : {false, true}) {
            Editor editor(cps_to_utf8(before));
            expect(fatal(editor.enter_source_mode()));
            const auto selection = SourceSelection::caret(before.size() - (backward ? 0 : 1));
            editor.set_source_selection(selection);
            reset_core_operation_counters();
            expect(fatal(editor.execute_command({.kind = backward
                ? CommandKind::DeleteBackward : CommandKind::DeleteForward})));
            const auto counters = read_core_operation_counters();
            expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
            expect(editor.source_editor()->source() == expected);
            expect(validate_document(editor.document()).empty());
            expect(editor.undo());
            expect(editor.source_editor()->source() == before);
            expect(editor.source_editor()->selection() == selection);
            expect(editor.redo());
            expect(editor.exit_source_mode());
            expect(editor.markdown_utf8() == cps_to_utf8(expected));
        }
    }
};

};
