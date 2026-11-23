#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite source_list_opening_tests = [] {

"source newline after a list marker preserves the following physical line"_test = [] {
    const std::vector<std::pair<std::u32string, std::size_t>> cases{
        {U"- one\n- two", 7}, {U"- one\n- two", 8},
        {U"1. first\n2. second", 11}, {U"1. first\n2. second", 12},
        {U"- [ ] one\n- [x] two", 11}, {U"- [ ] one\n- [x] two", 12},
        {U"- [ ] one\n- [x] two", 15}, {U"- [ ] one\n- [x] two", 16},
        {U"- first\n\n  second\n\n- third", 20},
        {U"- first\n  - nested\n- last", 20},
    };
    for (const auto& [before, offset] : cases) {
        for (const auto ending : {U"\n", U"\r\n", U"\r"}) {
            auto expected = before;
            expected.insert(offset, ending);
            Editor reloaded(cps_to_utf8(expected));
            expect(fatal(reloaded.markdown_utf8() == cps_to_utf8(expected)));
            Editor editor(cps_to_utf8(before));
            expect(fatal(editor.enter_source_mode()));
            const auto selected = SourceSelection::caret(offset);
            editor.set_source_selection(selected);
            reset_core_operation_counters();
            expect(fatal(editor.execute_command(Command::InsertText(ending))));
            const auto counters = read_core_operation_counters();
            expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
            expect(editor.source_editor()->source() == expected);
            expect(validate_document(editor.document()).empty());
            expect(editor.undo());
            expect(editor.source_editor()->source() == before);
            expect(editor.source_editor()->selection() == selected);
            expect(editor.redo());
            expect(editor.exit_source_mode());
            expect(editor.markdown_utf8() == cps_to_utf8(expected));
        }
    }
};

};
