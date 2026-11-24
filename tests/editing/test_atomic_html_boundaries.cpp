#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite atomic_html_boundary_tests = [] {

"atomic HTML after boundary completes the literal and keeps new text outside it"_test = [] {
    struct Case { std::string source; std::string closing; };
    const std::vector<Case> cases{
        {"<!-- unfinished", "-->"}, {"<?instruction", "?>"},
        {"<!DOCTYPE html", ">"}, {"<![CDATA[value", "]]>"},
        {"<script>value", "</script>"}, {"<STYLE>value", "</style>"},
        {"<pre>value", "</pre>"}, {"<textarea>value", "</textarea>"},
        {"<!-- line\r\nnext\r\n", "-->"},
    };
    for (const auto& entry : cases)
    for (const auto prefix : {std::string{}, std::string{"> "}, std::string{"- "}})
    for (const auto kind : {CommandKind::InsertText, CommandKind::InsertNewline}) {
        // A single-line prefix fixture keeps its ownership unambiguous. The
        // separate root case covers physical CRLF inside the literal body.
        if (!prefix.empty() && entry.source.find('\n') != std::string::npos) continue;
        const auto input = prefix + entry.source;
        Editor editor(input);
        const auto id = editor.document().cached_editable_order.front();
        const auto* object = find_document_block(editor.document(), id);
        expect(fatal(object && object->kind == BlockKind::UnsupportedMarkup)) << input;
        const auto raw_before = object->atomic_special().raw;
        const auto selected = TextSelection::caret({id, 1, TextAffinity::Downstream});
        editor.set_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = kind, .text = U"next"}))) << input;
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0u && counts.full_document_serializations == 0u);
        expect(counts.full_tree_transaction_diffs == 0u && counts.full_document_block_index_scans == 0u);
        const auto after = editor.markdown_utf8();
        expect(after.find(raw_before + entry.closing) != std::string::npos) << after;
        const auto* target = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(target && target->kind == BlockKind::Paragraph));
        expect(bool(target->inline_content().source == (kind == CommandKind::InsertText ? U"next" : U"")));
        expect(validate_document(editor.document()).empty());
        expect(document_indexes_are_exact(editor.document()));
        Editor reloaded(after);
        expect(reloaded.markdown_utf8() == after);
        if (kind == CommandKind::InsertText) {
            const auto* last = find_document_block(reloaded.document(), reloaded.document().cached_editable_order.back());
            expect(fatal(last && last->kind == BlockKind::Paragraph));
            expect(last->inline_content().source == U"next");
        }
        expect(editor.undo());
        expect(editor.markdown_utf8() == input);
        expect(editor.selection() == selected);
        expect(!editor.has_undo());
        expect(editor.redo());
        expect(editor.markdown_utf8() == after);
    }
};

"atomic HTML before boundary preserves incomplete literal source"_test = [] {
    for (const auto input : {"<!-- unfinished", "<script>value", "<![CDATA[value"}) {
        Editor editor(input);
        const auto id = editor.document().cached_editable_order.front();
        editor.set_caret({id, 0});
        expect(fatal(editor.execute_command(Command::InsertText(U"before"))));
        const auto* object = find_document_block(editor.document(), id);
        expect(fatal(object && object->kind == BlockKind::UnsupportedMarkup));
        expect(object->atomic_special().raw == input);
        expect(editor.markdown_utf8() == std::string{"before\n\n"} + input);
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.markdown_utf8() == editor.markdown_utf8());
        expect(editor.undo());
        expect(editor.markdown_utf8() == input);
    }
};

};
