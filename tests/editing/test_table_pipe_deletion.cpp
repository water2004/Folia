#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.inline_document;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite table_pipe_deletion_tests = [] {

"table escaped pipe deletion removes its protecting marker in one transaction"_test = [] {
    for (const auto& source : {std::u32string{U"\\|"}, std::u32string{U"`\\|`"}}) {
        for (const auto forward : {false, true}) {
            const auto markdown = U"| h |\n| --- |\n| " + source + U" |";
            Editor editor(cps_to_utf8(markdown));
            const auto owner = editor.document().root.children.front().children[1].children.front().id;
            const auto start = source.find(U"\\|");
            const auto selected = TextSelection::caret({owner, start + (forward ? 0u : 2u)});
            editor.set_selection(selected);
            expect(fatal(editor.execute_command({.kind = forward ? CommandKind::DeleteForward : CommandKind::DeleteBackward})));
            auto expected = source;
            expected.erase(start, 2);
            expect(fatal(find_document_block(editor.document(), owner)->inline_content.source == expected));
            expect(fatal(validate_document(editor.document()).empty()));
            const auto saved = editor.markdown_utf8();
            Editor reloaded(saved);
            expect(reloaded.document().root.children.front().children[1].children.size() == 1u);
            expect(reloaded.document().root.children.front().children[1].children.front().inline_content.source == expected);
            expect(fatal(editor.undo()));
            expect(editor.markdown_utf8() == cps_to_utf8(markdown));
            expect(editor.selection() == selected);
            expect(fatal(editor.redo()));
            expect(editor.markdown_utf8() == saved);
        }
    }
};

"ordinary escaped pipe source remains independently editable outside a table"_test = [] {
    Editor editor("\\|");
    const auto owner = editor.document().root.children.front().id;
    editor.set_selection(TextSelection::caret({owner, 0}));
    expect(fatal(editor.execute_command({.kind = CommandKind::DeleteForward})));
    expect(editor.markdown_utf8() == "|");
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == "\\|");
};

};
