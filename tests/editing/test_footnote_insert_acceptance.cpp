#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.document_footnotes;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.symbols;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

void footnote_insert_locality() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
    expect(counts.full_document_block_index_scans == 0_u);
    expect(counts.full_document_symbol_derivations == 0_u);
}

void footnote_insert_history(Editor& editor, const std::string& before, TextSelection selection,
    std::string_view label = "1") {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    expect(editor.selection().is_caret());
    expect(editor.selection().active.source_offset == 0_u);
    const auto target = footnote_definition_target(editor.document(), editor.symbols(), label);
    expect(fatal(target.has_value()));
    if (target) expect(editor.selection().active.container_id == target->container_id);
    Editor reloaded(after);
    expect(fatal(find_footnote_definition(reloaded.symbols(), label) != nullptr)) << after;
    expect(reloaded.markdown_utf8() == after);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selection);
    expect(document_indexes_are_exact(editor.document()));
    expect(!editor.has_undo());
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
}

suite footnote_insert_acceptance_tests = [] {

"footnote_insert_cross_owner_selection_preserves_text_and_uses_its_ordered_end"_test = [] {
    for (const auto source : {"left\n\nright", "# left\n\n> right", "- left\n- right",
            "| left | right |\n| --- | --- |"})
    for (const auto reversed : {false, true})
    for (const auto end_at_start : {false, true}) {
        Editor editor(source);
        const auto first = editor.document().cached_editable_order[0];
        const auto second = editor.document().cached_editable_order[1];
        const TextPosition from{first, 1}, to{second, end_at_start ? 0u : 3u};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::InsertFootnote}))) << source;
        footnote_insert_locality();
        expect(bool(find_document_block(editor.document(), first)->inline_content().source
            == (end_at_start ? U"left[^1]" : U"left")));
        expect(bool(find_document_block(editor.document(), second)->inline_content().source
            == (end_at_start ? U"right" : U"rig[^1]ht")));
        expect(editor.symbols().footnote_references.size() == 1_u);
        footnote_insert_history(editor, before, selected);
    }
};

"footnote_insert_atomic_boundaries_and_selections_keep_the_selected_object"_test = [] {
    for (const auto source : {"![old](old.png)", "***", "> ![old](old.png)", "- ![old](old.png)"})
    for (const auto position : {0u, 1u, 2u})
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.front();
        const auto* object = find_document_block(editor.document(), id);
        expect(fatal(object != nullptr));
        const auto object_kind = object->kind;
        const TextPosition from{id, 0}, to{id, 1};
        const auto selected = position == 2u
            ? (reversed ? TextSelection{to, from} : TextSelection{from, to})
            : TextSelection::caret({id, position});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::InsertFootnote}))) << source;
        footnote_insert_locality();
        object = find_document_block(editor.document(), id);
        expect(fatal(object != nullptr && object->kind == object_kind));
        expect(editor.symbols().footnote_references.size() == 1_u);
        const auto* reference = find_footnote_reference(editor.symbols(), "1");
        expect(fatal(reference != nullptr));
        if (reference) {
            const auto object_index = document_editable_order_position(editor.document(), id);
            const auto reference_index = document_editable_order_position(editor.document(), reference->container_id);
            expect(fatal(object_index.has_value() && reference_index.has_value()));
            if (object_index && reference_index)
                expect(position == 0u ? *reference_index < *object_index : *reference_index > *object_index);
        }
        footnote_insert_history(editor, before, selected);
    }
};

"footnote_insert_raw_body_literals_and_definitions_survive_reload"_test = [] {
    const std::vector<std::string> sources{
        "```cpp\nvalue\n```", "~~~cpp\r\nvalue\r\n~~~", "$$\nvalue\n$$",
        "```cpp\nvalue", "$$\nvalue", "```cpp",
        "> ```cpp\n> value\n> ```", "- ```cpp\n  value\n  ```",
    };
    for (const auto& source : sources) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.front();
        const auto* raw = find_document_block(editor.document(), id);
        expect(fatal(raw != nullptr && editable_raw_block_source(*raw) != nullptr)) << source;
        if (!raw || !editable_raw_block_source(*raw)) continue;
        const auto raw_kind = raw->kind;
        auto expected = *editable_raw_block_source(*raw);
        const auto value_at = expected.find(U"value");
        const auto visible = *editable_block_source_range(*raw);
        const auto offset = value_at == std::u32string::npos ? visible.start : value_at + 2;
        const auto selected = TextSelection::caret({id, offset});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        if (value_at == std::u32string::npos) expected += U"\n[^1]";
        else expected.insert(offset, U"[^1]");
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::InsertFootnote}))) << source;
        footnote_insert_locality();
        raw = find_document_block(editor.document(), id);
        expect(fatal(raw != nullptr && raw->kind == raw_kind));
        expect(bool(*editable_raw_block_source(*raw) == expected));
        expect(editor.symbols().footnote_references.empty());
        Editor reloaded(editor.markdown_utf8());
        const BlockNode* reloaded_raw = nullptr;
        walk_blocks(reloaded.document().root, [&](const BlockNode& block) {
            if (block.kind == raw_kind) reloaded_raw = &block;
        });
        expect(fatal(reloaded_raw != nullptr)) << editor.markdown_utf8();
        if (reloaded_raw) expect(bool(*editable_raw_block_source(*reloaded_raw) == expected));
        footnote_insert_history(editor, before, selected);
    }
};

"footnote_insert_new_definition_precedes_a_trailing_unclosed_literal_owner"_test = [] {
    for (const auto source : {"```cpp\nvalue", "lead\n\n~~~cpp\r\nvalue", "$$\nvalue"}) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.back();
        const auto* raw = find_document_block(editor.document(), id);
        expect(fatal(raw != nullptr && editable_raw_block_source(*raw) != nullptr));
        if (!raw || !editable_raw_block_source(*raw)) continue;
        const auto original = *editable_raw_block_source(*raw);
        const auto selected = TextSelection::caret({id, editable_block_source_range(*raw)->end});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::CreateFootnoteDefinition,
            .footnote_label = "new"})));
        footnote_insert_locality();
        expect(editor.document().root.children.back().id == id);
        expect(bool(*editable_raw_block_source(editor.document().root.children.back()) == original));
        footnote_insert_history(editor, before, selected, "new");
    }
};

"footnote_insert_definitions_remain_outside_unterminated_html_literals"_test = [] {
    for (const auto literal : {"<!-- raw", "<script>\nraw", "<STYLE>\nraw", "<pre>\nraw",
            "<textarea>\nraw", "<?raw", "<!DOCTYPE raw", "<![CDATA[raw"})
    for (const auto insert_reference : {false, true}) {
        Editor editor(std::string{"lead\n\n"} + literal);
        const auto id = editor.document().root.children.back().id;
        expect(editor.document().root.children.back().kind == BlockKind::UnsupportedMarkup);
        const auto selected = caret(first_text(editor), 4);
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = insert_reference
            ? CommandKind::InsertFootnote : CommandKind::CreateFootnoteDefinition,
            .footnote_label = "1"}))) << literal;
        footnote_insert_locality();
        expect(editor.document().root.children.back().id == id);
        expect(editor.document().root.children.back().atomic_special().raw == literal);
        expect(editor.markdown_utf8().find("[^1]: ") < editor.markdown_utf8().find(literal));
        footnote_insert_history(editor, before, selected);
    }
};

"footnote_insert_after_unterminated_html_completes_its_boundary_and_preserves_order"_test = [] {
    const std::vector<std::pair<std::string, std::string>> cases{
        {"<!-- raw", "-->"}, {"<script>\nraw", "</script>"}, {"<STYLE>\nraw", "</style>"},
        {"<pre>\nraw", "</pre>"}, {"<textarea>\nraw", "</textarea>"},
        {"<?raw", "?>"}, {"<!DOCTYPE raw", ">"}, {"<![CDATA[raw", "]]>"},
    };
    for (const auto& [literal, closer] : cases) {
        Editor editor(literal);
        const auto id = editor.document().root.children.front().id;
        const auto selected = TextSelection::caret({id, 1});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::InsertFootnote}))) << literal;
        footnote_insert_locality();
        const auto after = editor.markdown_utf8();
        expect(after.starts_with(literal));
        expect(after.find(closer) < after.find("[^1]"));
        expect(after.find("[^1]") < after.find("[^1]: "));
        Editor reloaded(after);
        expect(reloaded.symbols().footnote_references.size() == 1_u) << after;
        footnote_insert_history(editor, before, selected);
    }
};

"footnote_insert_blank_line_terminated_blocks_keep_definitions_at_document_end"_test = [] {
    for (const auto source : {"    value", "<div>\nraw", "<!-- closed -->", "<script>raw</ScRiPt>"}) {
        Editor editor(source);
        // Invisible-only documents also contain a zero-source destination.
        // Track the original literal block, which must remain at the front.
        const auto id = editor.document().root.children.front().id;
        const auto selected = editor.selection();
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = CommandKind::CreateFootnoteDefinition,
            .footnote_label = "1"})));
        expect(editor.document().root.children.front().id == id);
        expect(editor.document().root.children.back().kind == BlockKind::FootnoteDefinition);
        expect(editor.markdown_utf8().starts_with(before));
        footnote_insert_history(editor, before, selected);
    }
};

};

} // namespace
