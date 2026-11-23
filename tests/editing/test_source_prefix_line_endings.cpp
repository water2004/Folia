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

suite source_prefix_line_ending_tests = [] {

"container prefixes keep separate CR and LF physical lines"_test = [] {
    const std::vector<std::string> inputs{
        "> first\r>\n>\r\n> second\r\n",
        "> first\r> \n> second",
        "> > first\r> >\n> > second",
        "- > first\r  >\n  > second",
        "> [!NOTE]\r>\n> body",
        "> [!TIP] Title\r> \n> body\r\n",
        "> [!NOTE]\r\n> first\r>\n> second",
        "> first\r\n>\r\n> second\r\n",
        "> [!NOTE]\r\n> body\r\n",
        "- first\r\n  continued\r\n",
    };
    for (const auto& input : inputs) {
        Editor editor(input);
        expect(editor.markdown_utf8() == input) << std::quoted(input);
        expect(validate_document(editor.document()).empty());
        expect(editor.enter_source_mode());
        expect(cps_to_utf8(editor.source_editor()->source()) == input);
        expect(editor.exit_source_mode());
        expect(editor.markdown_utf8() == input);
    }
};

"source insertion between CR and LF keeps quote prefix ownership"_test = [] {
    const std::vector<std::pair<std::u32string, std::u32string>> inputs{
        {U"> first\r\n>\r\n> second\r\n", U">"},
        {U"> first\r\n> second", U"> "},
        {U"> > first\r\n> > second", U"> >"},
        {U"- > first\r\n  > second", U"  >"},
        {U"> [!NOTE]\r\n> body", U">"},
        {U"> [!TIP] Title\r\n> body", U"> "},
    };
    for (const auto& [before, inserted] : inputs) {
        Editor editor(cps_to_utf8(before));
        expect(fatal(editor.enter_source_mode()));
        const auto offset = before.find(U'\r') + 1;
        const auto selection = SourceSelection::caret(offset);
        editor.set_source_selection(selection);
        auto expected = before;
        expected.insert(offset, inserted);
        reset_core_operation_counters();
        const auto applied = editor.execute_command(Command::InsertText(inserted));
        const auto counters = read_core_operation_counters();
        expect(fatal(applied)) << std::quoted(cps_to_utf8(expected));
        expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
        expect(editor.source_editor()->source() == expected);
        expect(validate_document(editor.document()).empty());
        expect(editor.undo());
        expect(editor.source_editor()->source() == before);
        expect(editor.source_editor()->selection() == selection);
        expect(editor.redo());
        expect(editor.exit_source_mode());
        expect(editor.markdown_utf8() == cps_to_utf8(expected));
        Editor reloaded(cps_to_utf8(expected));
        expect(reloaded.markdown_utf8() == cps_to_utf8(expected));
    }
};

};
