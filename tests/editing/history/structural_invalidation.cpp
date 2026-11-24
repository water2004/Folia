#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.command;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_ids;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.core.input;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite editing_history_structural_invalidation_tests = [] {

"large_document_text_edits_use_the_validated_block_path_index"_test = [] {
    std::string markdown;
    constexpr std::size_t block_count = 2048;
    for (std::size_t index = 0; index < block_count; ++index) {
        if (index != 0) markdown += "\n\n";
        markdown += "paragraph " + std::to_string(index);
    }

    Editor editor(markdown);
    const auto& last = editor.document().root.children.back();
    editor.set_selection(caret(last, last.inline_content.source.size()));

    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command::InsertText(U"!"))));
    auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));

    expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
    const auto inserted = editor.selection().active.container_id;
    expect(fatal(bool(find_document_block(editor.document(), inserted) != nullptr)));

    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command::InsertText(U"new block"))));
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
};

"large_document_navigation_uses_the_editable_tree_order_index"_test = [] {
    std::string markdown;
    constexpr std::size_t block_count = 2048;
    for (std::size_t index = 0; index < block_count; ++index) {
        if (index != 0) markdown += "\n\n";
        markdown += "paragraph " + std::to_string(index);
    }

    Editor editor(markdown);
    expect(fatal(bool(editor.document().root.children.size() == block_count)));
    if (editor.document().root.children.size() != block_count) return;
    auto const& previous = editor.document().root.children[block_count - 2u];
    auto const& last = editor.document().root.children.back();
    const auto previous_id = previous.id;
    const auto last_id = last.id;
    const auto previous_size = previous.inline_content.source.size();
    editor.set_selection(caret(last, 0));

    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::MoveLeft})));
    auto counters = read_core_operation_counters();
    expect(fatal(bool(editor.selection().active.container_id == previous_id)));
    expect(fatal(bool(editor.selection().active.source_offset == previous_size)));

    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::MoveRight})));
    counters = read_core_operation_counters();
    expect(fatal(bool(editor.selection().active.container_id == last_id)));
    expect(fatal(bool(editor.selection().active.source_offset == 0u)));
    expect(fatal(bool(counters.full_document_text_projections == 0u)));
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));

    editor.set_selection(caret(
        editor.document().root.children.back(),
        editor.document().root.children.back().inline_content.source.size()));
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
    const auto inserted_id = editor.selection().active.container_id;
    expect(fatal(bool(inserted_id != last_id)));

    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::MoveLeft})));
    counters = read_core_operation_counters();
    expect(fatal(bool(editor.selection().active.container_id == last_id)));
    expect(fatal(bool(counters.full_document_text_projections == 0u)));
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));

    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::SelectAll})));
    counters = read_core_operation_counters();
    expect(fatal(bool(editor.selection().anchor.container_id
        == editor.document().root.children.front().id)));
    expect(fatal(bool(editor.selection().active.container_id == inserted_id)));
    expect(fatal(bool(counters.full_document_text_projections == 0u)));
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
};

"large_document_plain_block_splits_skip_unrelated_symbol_and_outline_rebuilds"_test = [] {
    std::string markdown;
    constexpr std::size_t block_count = 2048;
    for (std::size_t index = 0; index < block_count; ++index) {
        if (index != 0) markdown += "\n\n";
        markdown += "paragraph " + std::to_string(index);
    }
    Editor editor(markdown);
    auto render_model = build_render_model(
        editor.document(),
        editor.outline(),
        editor.symbols(),
        default_theme_profile());
    const auto initial_render_block_count = render_model.blocks.size();
    auto update_render_model = [&](const EditorDocumentChange& change) {
        RenderModelUpdate update;
        update.structural = change.structural;
        update.structural_locality_known = change.structural_locality_known;
        update.structural_anchors = change.structural_anchors;
        for (const auto& operation : change.text_operations) {
            update.changed_owners.push_back(
                (change.forward ? operation.forward : operation.inverse).container_id);
        }
        reset_core_operation_counters();
        render_model = build_render_model_incremental(
            editor.document(),
            editor.outline(),
            editor.symbols(),
            default_theme_profile(),
            std::move(render_model),
            update);
        return read_core_operation_counters();
    };
    const auto& last = editor.document().root.children.back();
    editor.set_selection(caret(last, last.inline_content.source.size() / 2));

    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(counters.incremental_document_block_index_repairs == 1u)));
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(counters.full_document_outline_derivations == 0u)));
    expect(fatal(bool(counters.local_symbol_derivations == 1u)));
    expect(fatal(bool(validate_document(editor.document()).empty())));
    expect(fatal(document_indexes_are_exact(editor.document())));

    auto change = editor.take_last_document_change();
    expect(fatal(bool(change.has_value())));
    if (!change) return;
    const auto render_counters = update_render_model(*change);
    expect(fatal(bool(render_counters.render_source_key_derivations == 2u)));
    expect(fatal(bool(render_model.rebuilt_block_count == 2u)));
    expect(fatal(bool(render_model.reused_block_count == initial_render_block_count - 1u)));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    auto history_counters = read_core_operation_counters();
    expect(fatal(bool(history_counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(history_counters.incremental_document_block_index_repairs == 1u)));
    expect(fatal(bool(history_counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(history_counters.full_document_outline_derivations == 0u)));
    expect(fatal(bool(history_counters.local_symbol_derivations == 1u)));
    expect(fatal(document_indexes_are_exact(editor.document())));
    change = editor.take_last_document_change();
    expect(fatal(bool(change.has_value())));
    if (!change) return;
    auto history_render_counters = update_render_model(*change);
    expect(fatal(bool(history_render_counters.render_source_key_derivations == 1u)));
    expect(fatal(bool(render_model.rebuilt_block_count == 1u)));
    expect(fatal(bool(render_model.reused_block_count == initial_render_block_count - 1u)));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    history_counters = read_core_operation_counters();
    expect(fatal(bool(history_counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(history_counters.incremental_document_block_index_repairs == 1u)));
    expect(fatal(bool(history_counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(history_counters.full_document_outline_derivations == 0u)));
    expect(fatal(bool(history_counters.local_symbol_derivations == 1u)));
    expect(fatal(document_indexes_are_exact(editor.document())));
    change = editor.take_last_document_change();
    expect(fatal(bool(change.has_value())));
    if (!change) return;
    history_render_counters = update_render_model(*change);
    expect(fatal(bool(history_render_counters.render_source_key_derivations == 2u)));
    expect(fatal(bool(render_model.rebuilt_block_count == 2u)));
    expect(fatal(bool(render_model.reused_block_count == initial_render_block_count - 1u)));
};

"nested_structural_edits_invalidate_only_the_stable_top_level_render_block"_test = [] {
    Editor editor("- first\n- second\n\noutside");
    auto render_model = build_render_model(
        editor.document(),
        editor.outline(),
        editor.symbols(),
        default_theme_profile());
    expect(fatal(bool(render_model.blocks.size() == 2u)));

    const auto owner = first_text(editor).id;
    editor.set_selection(TextSelection::caret({owner, 2, TextAffinity::Downstream}));
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
    auto change = editor.take_last_document_change();
    expect(fatal(bool(change.has_value())));
    if (!change) return;
    expect(fatal(bool(change->structural)));
    expect(fatal(bool(change->structural_locality_known)));

    auto update_model = [&](const EditorDocumentChange& current) {
        RenderModelUpdate update;
        update.structural = current.structural;
        update.structural_locality_known = current.structural_locality_known;
        update.structural_anchors = current.structural_anchors;
        for (const auto& operation : current.text_operations) {
            update.changed_owners.push_back(
                (current.forward ? operation.forward : operation.inverse).container_id);
        }
        reset_core_operation_counters();
        render_model = build_render_model_incremental(
            editor.document(),
            editor.outline(),
            editor.symbols(),
            default_theme_profile(),
            std::move(render_model),
            update);
        return read_core_operation_counters();
    };

    auto counters = update_model(*change);
    expect(fatal(bool(counters.render_source_key_derivations == 1u)));
    expect(fatal(bool(render_model.incremental_update)));
    expect(fatal(bool(render_model.changed_block_indices == std::vector<std::size_t>{0})));
    expect(fatal(bool(render_model.rebuilt_block_count == 1u)));
    expect(fatal(bool(render_model.reused_block_count == 1u)));
    auto full = build_render_model(
        editor.document(),
        editor.outline(),
        editor.symbols(),
        default_theme_profile());
    expect(fatal(bool(render_model.blocks.size() == full.blocks.size())));
    for (std::size_t index = 0; index < full.blocks.size(); ++index) {
        expect(fatal(bool(render_model.blocks[index].presentation_key
            == full.blocks[index].presentation_key)));
    }

    expect(fatal(editor.undo()));
    change = editor.take_last_document_change();
    expect(fatal(bool(change.has_value())));
    if (!change) return;
    counters = update_model(*change);
    expect(fatal(bool(counters.render_source_key_derivations == 1u)));
    expect(fatal(bool(render_model.incremental_update)));
    expect(fatal(bool(render_model.changed_block_indices == std::vector<std::size_t>{0})));
    expect(fatal(bool(render_model.rebuilt_block_count == 1u)));
    expect(fatal(bool(render_model.reused_block_count == 1u)));
};

"tree_moves_keep_render_invalidation_local_to_their_stable_top_level"_test = [] {
    Editor editor("- one\n- two\n\noutside");
    auto model = build_render_model(
        editor.document(),
        editor.outline(),
        editor.symbols(),
        default_theme_profile());
    const BlockNode* two = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (block.inline_content.source == U"two") two = &block;
    });
    expect(fatal(bool(two != nullptr)));
    if (!two) return;
    editor.set_selection(caret(*two, 0));
    expect(fatal(bool(editor.execute_document_indent_list_item(editor.selection()).has_value())));
    auto change = editor.take_last_document_change();
    expect(fatal(bool(change.has_value())));
    if (!change) return;
    expect(fatal(bool(change->structural_locality_known)));
    expect(fatal(bool(!change->moved_roots.empty())));

    RenderModelUpdate update;
    update.structural = change->structural;
    update.structural_locality_known = change->structural_locality_known;
    update.structural_anchors = change->structural_anchors;
    reset_core_operation_counters();
    model = build_render_model_incremental(
        editor.document(),
        editor.outline(),
        editor.symbols(),
        default_theme_profile(),
        std::move(model),
        update);
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.render_source_key_derivations == 1u)));
    expect(fatal(bool(model.incremental_update)));
    expect(fatal(bool(model.changed_block_indices == std::vector<std::size_t>{0})));
    expect(fatal(bool(model.rebuilt_block_count == 1u)));
    expect(fatal(bool(model.reused_block_count == 1u)));
};

"externally_imported_blocks_reserve_node_ids_explicitly"_test = [] {
    Editor donor("external");
    BlockNode paragraph = donor.document().root.children.front();
    paragraph.id = NodeId{99};
    expect(fatal(bool(!paragraph.inline_content.tree.nodes.empty())));
    if (paragraph.inline_content.tree.nodes.empty()) return;
    paragraph.inline_content.tree.nodes.front().id = NodeId{149};

    EditorDocument invalid = EditorDocument::empty(1);
    invalid.root.children.clear();
    invalid.root.children.push_back(paragraph);
    expect(fatal(bool(!validate_document(invalid).empty())));

    EditorDocument document = EditorDocument::empty(1);
    document.root.children.clear();
    reserve_document_node_ids(document, paragraph);
    document.root.children.push_back(paragraph);
    rebuild_document_block_index(document);

    expect(fatal(bool(allocate_document_node_id(document) == NodeId{150})));
    expect(fatal(bool(allocate_document_node_id(document) == NodeId{151})));
    expect(fatal(bool(validate_document(document).empty())));
};

};
