#include "support/folia_test.hpp"

import folia.core.document_ids;
import folia.core.document_operation_apply;
import folia.core.document_transaction;
import folia.core.editor;
import folia.core.render_builder;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.container_source;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;
import folia.markdown.parser;
import folia.markdown.selection;
import folia.markdown.text_edit;

using namespace boost::ut;
using namespace folia;

namespace {

std::uint64_t metadata_key(BlockNode const& block) {
    render_key_detail::Hasher hash;
    render_key_detail::append_metadata(hash, block);
    return hash.value;
}

void expect_same_delta(DocumentTreeDelta const& actual, DocumentTreeDelta const& expected) {
    expect(actual.kind == expected.kind);
    expect(actual.parent_id == expected.parent_id);
    expect(actual.other_parent_id == expected.other_parent_id);
    expect(actual.root_id == expected.root_id);
    expect(actual.index == expected.index);
    expect(actual.other_index == expected.other_index);
    expect(actual.first_owner == expected.first_owner);
    expect(actual.last_owner == expected.last_owner);
    expect(actual.source_payload_changed == expected.source_payload_changed);
    expect(actual.before_payload.has_value() == expected.before_payload.has_value());
    expect(actual.after_payload.has_value() == expected.after_payload.has_value());
    if (actual.before_payload && expected.before_payload)
        expect(metadata_key(*actual.before_payload) == metadata_key(*expected.before_payload));
    if (actual.after_payload && expected.after_payload)
        expect(metadata_key(*actual.after_payload) == metadata_key(*expected.after_payload));
}

void expect_inverse_delta(DocumentTreeDelta const& actual, DocumentTreeDelta const& forward) {
    auto expected = forward;
    if (expected.kind == DocumentTreeEditKind::Insert) expected.kind = DocumentTreeEditKind::Remove;
    else if (expected.kind == DocumentTreeEditKind::Remove) expected.kind = DocumentTreeEditKind::Insert;
    else if (expected.kind == DocumentTreeEditKind::Move) {
        std::swap(expected.parent_id, expected.other_parent_id);
        std::swap(expected.index, expected.other_index);
    } else {
        std::swap(expected.before_payload, expected.after_payload);
    }
    expect_same_delta(actual, expected);
}

std::vector<NodeId> child_ids(BlockNode const& parent) {
    std::vector<NodeId> result;
    for (auto const& child : parent.children) result.push_back(child.id);
    return result;
}

// A projection consumer sees already applied deltas, so it must replay them
// in ordinary forward order without consulting the history direction again.
std::vector<NodeId> project_child_splices(std::vector<NodeId> ids,
                                        std::span<DocumentTreeDelta const> deltas,
                                        NodeId parent) {
    for (auto const& delta : deltas) {
        if (delta.kind == DocumentTreeEditKind::UpdatePayload) continue;
        expect(fatal(delta.parent_id == parent));
        if (delta.kind == DocumentTreeEditKind::Insert) {
            expect(fatal(delta.index <= ids.size()));
            ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(delta.index), delta.root_id);
        } else {
            expect(fatal(delta.index < ids.size()));
            expect(ids[delta.index] == delta.root_id);
            ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(delta.index));
            if (delta.kind == DocumentTreeEditKind::Move) {
                expect(fatal(delta.other_parent_id == parent));
                expect(fatal(delta.other_index <= ids.size()));
                ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(delta.other_index), delta.root_id);
            }
        }
    }
    return ids;
}

void expect_compact_payload(BlockNode const& shell) {
    expect(shell.children.empty());
    expect(shell.inline_content.source.empty());
    expect(shell.inline_content.tree.nodes.empty());
    expect(!shell.block_source.storage);
    expect(!shell.has_container_source());
    expect(shell.atomic_special().raw.empty());
    expect(shell.html_special().source.empty());
    expect(shell.html_special().content_slots.empty());
    expect(shell.html_special().structure_shape.empty());
    expect(shell.table_special().table_separator_source.empty());
    expect(shell.table_special().table_internal_line_endings.empty());
    expect(!shell.table_special().row_syntax);
}

} // namespace

suite document_change_delta_tests = [] {
    "tree deltas replay mixed structural operations and undo in applied order"_test = [] {
        auto document = parse_text(1, "alpha\n\nbeta\n\ngamma").document;
        const auto original = child_ids(document.root);
        const auto parent = document.root.id;
        BlockNode inserted;
        inserted.id = allocate_document_node_id(document);
        inserted.kind = BlockKind::Paragraph;
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent;
        insert.index = 1;
        insert.after = inserted;
        DocumentTreeEdit move;
        move.kind = DocumentTreeEditKind::Move;
        move.parent_id = parent;
        move.index = 3;
        move.other_parent_id = parent;
        move.other_index = 0;
        move.moved_id = original[2];
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent;
        remove.index = 3;
        remove.before = document.root.children[1];
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(document.root.children[2]);
        update.after = update.before;
        update.after.kind = BlockKind::Heading;
        update.after.ensure_text_special().level = 2;
        const std::vector<DocumentOperation> operations{insert, move, remove, update};

        expect(fatal(apply_document_operations(document, operations, true)));
        auto forward = summarize_document_change(operations, true, 1, 2);
        const auto changed = child_ids(document.root);
        expect(std::ranges::equal(project_child_splices(original, forward.tree_deltas, parent), changed));
        expect(forward.tree_deltas.front().first_owner == inserted.id);
        expect(forward.tree_deltas.front().last_owner == inserted.id);
        expect(forward.tree_deltas.back().before_payload->kind == BlockKind::Paragraph);
        expect(forward.tree_deltas.back().after_payload->kind == BlockKind::Heading);

        expect(fatal(apply_document_operations(document, operations, false)));
        auto undo = summarize_document_change(operations, false, 2, 3);
        expect(std::ranges::equal(project_child_splices(changed, undo.tree_deltas, parent), child_ids(document.root)));
        expect(std::ranges::equal(child_ids(document.root), original));
        expect(fatal(undo.tree_deltas.size() == forward.tree_deltas.size()));
        for (std::size_t index = 0; index < undo.tree_deltas.size(); ++index)
            expect_inverse_delta(undo.tree_deltas[index], forward.tree_deltas[forward.tree_deltas.size() - index - 1]);

        // Empty wrappers are real intermediate snapshots, not an owner with
        // id zero masquerading as a valid range. A projection must decline it.
        DocumentTreeEdit empty;
        empty.after.id = NodeId{9000};
        empty.after.kind = BlockKind::BlockQuote;
        const auto delta = summarize_tree_delta(empty, true);
        expect(delta.root_id == empty.after.id);
        expect(delta.first_owner.v == 0u);
        expect(delta.last_owner.v == 0u);
        move.other_parent_id = NodeId{9000};
        const auto inverse_move = summarize_tree_delta(move, false);
        expect(inverse_move.parent_id == move.other_parent_id);
        expect(inverse_move.other_parent_id == move.parent_id);
        expect(inverse_move.index == move.other_index);
        expect(inverse_move.other_index == move.index);
    };

    "tree delta metadata preserves rendering keys without retaining source or provenance"_test = [] {
        auto check = [](BlockNode const& block) {
            DocumentTreeEdit update;
            update.kind = DocumentTreeEditKind::UpdatePayload;
            update.before = document_transaction_detail::payload_shell(block);
            update.after = update.before;
            const auto expected = metadata_key(block);
            reset_core_operation_counters();
            const auto delta = summarize_tree_delta(update, true);
            expect(read_core_operation_counters().container_prefix_records_copied == 0u);
            expect(fatal(delta.before_payload && delta.after_payload));
            expect(metadata_key(*delta.before_payload) == expected);
            expect(metadata_key(*delta.after_payload) == expected);
            expect_compact_payload(*delta.before_payload);
            expect_compact_payload(*delta.after_payload);
            expect(!delta.source_payload_changed);
        };
        for (auto const& source : std::vector<std::string>{
            "# A heading\n",
            "98. one\n99. two\n",
            "- [x] task\n- [ ] next\n",
            "> [!WARNING]\n> **body**\n",
            "```cpp\nint x = 1;\n```\n",
            "$$\nx+y\n$$\n",
            "| A | B |\n| :--- | ---: |\n| C | D |\n",
            "<table><tr><td align=\"right\">cell</td></tr></table>\n",
            "<div align=\"center\"><p>body</p></div>\n",
            "![alternative](image.png \"title\")\n",
            "[^note]: footnote\n",
            "---\ntitle: test\n---\n",
        }) {
            auto document = parse_text(1, source).document;
            auto visit = [&](auto& self, BlockNode const& node) -> void {
                check(node);
                for (auto const& child : node.children) self(self, child);
            };
            visit(visit, document.root);
        }
        std::string quote;
        for (std::size_t index = 0; index < 4096; ++index) quote += "> - body\n";
        auto document = parse_text(1, quote).document;
        expect(fatal(document.root.children.front().has_container_source()));
        expect(document.root.children.front().container_source().lines.size() >= 4096u);
        check(document.root.children.front());
    };

    "tree deltas flag source-bearing raw payload changes even when metadata is equal"_test = [] {
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before.id = NodeId{42};
        update.before.kind = BlockKind::UnsupportedMarkup;
        update.before.ensure_atomic_special().raw = "before raw content";
        update.after = update.before;
        update.after.ensure_atomic_special().raw = "after raw content";
        for (bool direction : {true, false}) {
            const auto delta = summarize_tree_delta(update, direction);
            expect(delta.source_payload_changed);
            expect(metadata_key(*delta.before_payload) == metadata_key(*delta.after_payload));
            expect(delta.before_payload->atomic_special().raw.empty());
            expect(delta.after_payload->atomic_special().raw.empty());
        }
    };

    "quoted source and nested list history keep exact provenance with normalized tree deltas"_test = [] {
        for (bool soft_break : {false, true}) {
            const std::string source = soft_break
                ? "> - alpha\n> - beta\n"
                : "> - alpha\n>\n>   beta\n>   - child\n> - tail\n";
            Editor editor(source);
            auto owner = editor.document().cached_editable_order.front();
            const auto selection = TextSelection::caret({owner, 2, TextAffinity::Downstream});
            editor.set_selection(selection);
            auto transaction = soft_break
                ? editor.execute_document_insert_soft_break(selection)
                : editor.execute_document_enter(selection);
            expect(fatal(transaction.has_value()));
            auto forward = editor.take_last_document_change();
            expect(fatal(forward.has_value()));
            expect(!forward->tree_deltas.empty());
            if (!soft_break)
                expect(std::ranges::any_of(forward->tree_deltas, [](auto const& delta) {
                    return delta.kind == DocumentTreeEditKind::Move;
                }));
            const auto changed = editor.markdown_utf8();
            const auto changed_selection = editor.selection();
            expect(changed != source);
            for (auto const& delta : forward->tree_deltas) {
                if (delta.before_payload) expect_compact_payload(*delta.before_payload);
                if (delta.after_payload) expect_compact_payload(*delta.after_payload);
            }

            expect(fatal(editor.undo()));
            expect(editor.markdown_utf8() == source);
            expect(editor.selection() == selection);
            auto undo = editor.take_last_document_change();
            expect(fatal(undo.has_value()));
            expect(fatal(undo->tree_deltas.size() == forward->tree_deltas.size()));
            for (std::size_t index = 0; index < undo->tree_deltas.size(); ++index)
                expect_inverse_delta(undo->tree_deltas[index],
                    forward->tree_deltas[forward->tree_deltas.size() - index - 1]);

            expect(fatal(editor.redo()));
            expect(editor.markdown_utf8() == changed);
            expect(editor.selection() == changed_selection);
            auto redo = editor.take_last_document_change();
            expect(fatal(redo.has_value()));
            expect(fatal(redo->tree_deltas.size() == forward->tree_deltas.size()));
            for (std::size_t index = 0; index < redo->tree_deltas.size(); ++index)
                expect_same_delta(redo->tree_deltas[index], forward->tree_deltas[index]);
        }
    };
};
