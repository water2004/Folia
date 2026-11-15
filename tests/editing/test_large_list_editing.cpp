#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.editor;
import folia.core.document_ids;
import folia.core.document_index_update;
import folia.core.document_operation_apply;
import folia.core.document_transaction;
import folia.markdown.ids;
import folia.markdown.selection;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;
import folia.markdown.parser;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

namespace {

std::string repeated_list(std::size_t bytes) {
    // Synthetic input only: never load a user attachment in a regression test.
    constexpr std::string_view line = "- Sample markdown list item for parser and editing tests.\n";
    std::string source;
    source.reserve(bytes + line.size());
    while (source.size() < bytes) source += line;
    return source;
}

void expect_local_text_work(std::string_view operation) {
    const auto work = read_core_operation_counters();
    expect(work.full_document_parses == 0u) << operation;
    expect(work.full_document_serializations == 0u) << operation;
    expect(work.full_document_block_index_scans == 0u) << operation;
    expect(work.block_tree_lookup_nodes == 0u) << operation << " root DFS visits";
    expect(work.block_index_entries_updated == 0u) << operation;
    expect(work.editable_order_entries_updated == 0u) << operation;
    expect(work.inline_scanned_characters <= 256u) << operation << " parsed characters";
    expect(work.local_symbol_derivations <= 1u) << operation;
}

void expect_owner_source(const Editor& editor, NodeId id, std::u32string_view expected) {
    const auto* block = find_document_block(editor.document(), id);
    expect(fatal(block != nullptr));
    if (!block) return;
    expect(block->inline_content.source == expected);
    expect(flatten_tokens(block->inline_content.tree, block->inline_content.source) == expected);
}

} // namespace

suite large_list_editing_tests = [] {

"ten_megabyte_single_list_text_edits_and_history_visit_only_the_source_owner"_test = [] {
    const auto source = repeated_list(10u * 1024u * 1024u);
    Editor editor(source);
    const auto& list = editor.document().root.children.front();
    expect(fatal(list.kind == BlockKind::List));
    expect(fatal(list.children.size() > 100000u));
    const auto& last = list.children.back().children.front();
    const auto id = last.id;
    const auto original = last.inline_content.source;
    const auto before = TextSelection::caret({id, 8, TextAffinity::Downstream});
    editor.set_selection(before);

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"\u03a9").has_value()));
    expect_local_text_work("insert at end of large list");
    auto inserted = original;
    inserted.insert(8, U"\u03a9");
    expect_owner_source(editor, id, inserted);
    const auto after_insert = editor.selection();

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_delete_backward(editor.selection()).has_value()));
    expect_local_text_work("backspace at end of large list");
    expect_owner_source(editor, id, original);

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_text_work("undo backspace");
    expect_owner_source(editor, id, inserted);
    expect(editor.selection() == after_insert);

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_text_work("undo insertion");
    expect_owner_source(editor, id, original);
    expect(editor.selection() == before);

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_text_work("redo insertion");
    expect_owner_source(editor, id, inserted);

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_text_work("redo backspace");
    expect_owner_source(editor, id, original);
    // Full serialization is an explicit save assertion, outside the edit gate.
    expect(editor.markdown_utf8() == source);
};

"list_tail_enter_and_history_do_not_rewrite_the_existing_navigation_index"_test = [] {
    Editor editor(repeated_list(256u * 1024u));
    const auto& list = editor.document().root.children.front();
    const auto count = list.children.size();
    const auto& last = list.children.back().children.front();
    editor.set_selection(TextSelection::caret(
        {last.id, last.inline_content.source.size(), TextAffinity::Downstream}));
    const auto before = editor.selection();

    auto check_work = [](std::string_view action) {
        const auto work = read_core_operation_counters();
        expect(work.full_document_parses == 0u) << action;
        expect(work.full_document_serializations == 0u) << action;
        expect(work.full_document_block_index_scans == 0u) << action;
        expect(work.block_tree_lookup_nodes == 0u) << action << " root DFS visits";
        expect(work.block_index_entries_updated <= 2u) << action << " locator writes";
        expect(work.editable_order_entries_updated <= 1u) << action << " order writes";
    };

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_enter(editor.selection()).has_value()));
    check_work("append list item");
    expect(editor.document().root.children.front().children.size() == count + 1);
    const auto after = editor.selection();

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    check_work("undo appended list item");
    expect(editor.selection() == before);
    expect(editor.document().root.children.front().children.size() == count);

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    check_work("redo appended list item");
    expect(editor.selection() == after);
    expect(editor.document().root.children.front().children.size() == count + 1);
};

"inserting_a_root_sibling_preserves_large_list_descendant_locators"_test = [] {
    Editor editor(std::string("first\n\n") + repeated_list(256u * 1024u));
    const auto& first = editor.document().root.children.front();
    const auto last_id = editor.document().root.children.back().children.back().children.front().id;
    editor.set_selection(TextSelection::caret({first.id, 2, TextAffinity::Downstream}));

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_enter(editor.selection()).has_value()));
    const auto work = read_core_operation_counters();
    expect(work.block_index_entries_updated <= 2u) << "only new paragraph and shifted list edge";
    expect(work.full_document_block_index_scans == 0u);
    expect(work.block_tree_lookup_nodes == 0u);
    // Descendant paths follow the list's one repaired edge.
    expect_owner_source(editor, last_id, U"Sample markdown list item for parser and editing tests.");
    expect(read_core_operation_counters().full_document_block_index_scans == 0u);
};

"new_container_locator_indexing_reuses_a_moved_large_list_subtree"_test = [] {
    auto document = parse_text(1, repeated_list(256u * 1024u)).document;
    const auto list_id = document.root.children.front().id;
    const auto owner_id = document.root.children.front().children.back().children.front().id;
    BlockNode quote;
    quote.id = allocate_document_node_id(document);
    quote.kind = BlockKind::BlockQuote;
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = document.root.id;
    insert.index = 0;
    insert.after = quote;
    DocumentTreeEdit move;
    move.kind = DocumentTreeEditKind::Move;
    move.parent_id = document.root.id;
    move.index = 1;
    move.other_parent_id = quote.id;
    move.other_index = 0;
    move.moved_id = list_id;
    const std::vector<DocumentOperation> operations{insert, move};

    expect(fatal(apply_document_operations(document, operations, true)));
    reset_core_operation_counters();
    expect(fatal(update_document_block_index(document, operations, true)));
    expect(read_core_operation_counters().block_index_entries_updated <= 3u)
        << "new wrapper and moved list edge, no descendant locator rebuild";
    expect(find_document_block(document, owner_id) != nullptr);
    expect(read_core_operation_counters().full_document_block_index_scans == 0u);

    expect(fatal(apply_document_operations(document, operations, false)));
    reset_core_operation_counters();
    expect(fatal(update_document_block_index(document, operations, false)));
    expect(read_core_operation_counters().block_index_entries_updated <= 1u);
    expect(find_document_block(document, owner_id) != nullptr);
    expect(read_core_operation_counters().full_document_block_index_scans == 0u);
};

"typing_in_a_long_quoted_list_does_not_copy_the_quotes_prefix_provenance"_test = [] {
    std::string source;
    for (std::size_t index = 0; index < 4096; ++index) source += "> - ordinary item\n";
    Editor editor(source);
    const auto& quote = editor.document().root.children.front();
    expect(fatal(quote.kind == BlockKind::BlockQuote));
    const auto& list = quote.children.front();
    expect(fatal(list.kind == BlockKind::List));
    const auto& owner = list.children.back().children.front();
    const auto id = owner.id;
    editor.set_selection(TextSelection::caret({id, 4, TextAffinity::Downstream}));
    const auto before = editor.selection();

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"X").has_value()));
    expect_local_text_work("quoted list insertion");
    const auto work = read_core_operation_counters();
    expect(work.container_prefix_records_copied == 0u);
    expect(work.container_prefix_records_examined < 64u) << "owner lookup probes";
    expect_owner_source(editor, id, U"ordiXnary item");

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_text_work("quoted list insertion undo");
    expect(read_core_operation_counters().container_prefix_records_copied == 0u);
    expect(editor.selection() == before);
    expect(editor.markdown_utf8() == source);

    // A physical newline really does change ancestor provenance. Preserve the
    // exact source and history rather than treating this as an unchanged path.
    expect(fatal(editor.execute_document_insert_soft_break(editor.selection()).has_value()));
    const auto changed = editor.markdown_utf8();
    expect(changed != source);
    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"X").has_value()));
    expect_local_text_work("quoted list insertion after a physical line edit");
    expect(read_core_operation_counters().container_prefix_records_copied == 0u);
    expect(read_core_operation_counters().container_prefix_records_examined < 64u);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == changed);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == source);
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == changed);
};

}; // suite large_list_editing_tests
