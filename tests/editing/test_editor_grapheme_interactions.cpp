#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.document_text;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite editor_grapheme_interactions = [] {

"unicode grapheme movement and deletion agree in rendered and source modes"_test = [] {
    const std::vector<std::u32string> clusters{
        U"e\u0301", U"x\u1ab0", U"\U0001f469\u200D\U0001f4bb",
        U"\U0001f468\u200D\U0001f469\u200D\U0001f467\u200D\U0001f466",
        U"\U0001f469\U0001f3fd\u200D\U0001f4bb", U"\U0001f1e8\U0001f1f3",
        U"\u1100\u1161\u11a8", U"\u0915\u094d\u0937", U"1\ufe0f\u20e3",
    };
    for (const auto& cluster : clusters) {
        for (const bool source_mode : {false, true}) {
            for (const bool raw : {false, true}) {
                const auto body = U"A" + cluster + U"Z";
                const auto source = raw ? U"```text\n" + body + U"\n```" : body;
                Editor editor(cps_to_utf8(source));
                if (source_mode) expect(fatal(editor.enter_source_mode()));
                const auto owner = editor.document().root.children.front().id;
                const auto start = raw ? std::size_t{9} : std::size_t{1};
                const auto stop = start + cluster.size();
                auto select = [&](std::size_t offset) {
                    if (source_mode) editor.set_source_selection(SourceSelection::caret(offset));
                    else editor.set_selection(TextSelection::caret({owner, offset}));
                };
                auto offset = [&] {
                    return source_mode ? editor.source_editor()->selection().active : editor.selection().active.source_offset;
                };
                select(start);
                expect(fatal(editor.execute_command(Command::MoveRight(false))));
                expect(fatal(offset() == stop)) << cps_to_utf8(cluster) << source_mode << raw;
                expect(fatal(editor.execute_command(Command::MoveLeft(false))));
                expect(fatal(offset() == start)) << cps_to_utf8(cluster) << source_mode << raw;
                select(stop);
                reset_core_operation_counters();
                expect(fatal(editor.execute_command(Command{.kind = CommandKind::DeleteBackward})));
                auto counters = read_core_operation_counters();
                expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
                const auto expected = raw ? "```text\nAZ\n```" : "AZ";
                expect(fatal(editor.markdown_utf8() == expected)) << cps_to_utf8(cluster) << source_mode << raw;
                expect(fatal(validate_document(editor.document()).empty()));
                expect(fatal(editor.undo()));
                expect(fatal(editor.markdown_utf8() == cps_to_utf8(source)));
                expect(fatal(offset() == stop));
                expect(fatal(editor.redo()));
                expect(fatal(editor.markdown_utf8() == expected));
                expect(fatal(editor.undo()));
                select(start);
                expect(fatal(editor.execute_command(Command{.kind = CommandKind::DeleteForward})));
                expect(fatal(editor.markdown_utf8() == expected));
                expect(fatal(editor.undo()));
                expect(fatal(offset() == start));
            }
        }
    }
};

"unicode grapheme backspace removes one flag from consecutive flags"_test = [] {
    const auto source = U"\U0001f1e8\U0001f1f3\U0001f1fa\U0001f1f8\U0001f1ef";
    for (const bool source_mode : {false, true}) {
        Editor editor(cps_to_utf8(source));
        if (source_mode) {
            expect(fatal(editor.enter_source_mode()));
            editor.set_source_selection(SourceSelection::caret(5));
        } else {
            editor.set_selection(TextSelection::caret({editor.document().root.children.front().id, 5}));
        }
        expect(fatal(editor.execute_command(Command{.kind = CommandKind::DeleteBackward})));
        expect(editor.markdown_utf8() == cps_to_utf8(std::u32string_view(source).substr(0, 4)));
        expect(fatal(editor.execute_command(Command{.kind = CommandKind::DeleteBackward})));
        expect(editor.markdown_utf8() == cps_to_utf8(std::u32string_view(source).substr(0, 2)));
        expect(fatal(editor.undo()));
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == cps_to_utf8(source));
    }
};

};
