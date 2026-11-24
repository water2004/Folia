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

suite editing_history_indexed_edits_tests = [] {

"large_document_footnote_queries_use_the_cached_symbol_and_block_indexes"_test = [] {
    std::string markdown = "first[^note]";
    constexpr std::size_t block_count = 2048;
    for (std::size_t index = 1; index < block_count; ++index) {
        markdown += "\n\nparagraph " + std::to_string(index);
    }
    markdown += "\n\n[^note]: preview body";
    Editor editor(markdown);

    reset_core_operation_counters();
    const auto definition = footnote_definition_target(
        editor.document(), editor.symbols(), "note");
    const auto reference = first_footnote_reference_target(
        editor.symbols(), "note");
    const auto preview = footnote_preview(
        editor.document(), editor.symbols(), "note", 240);
    const auto counters = read_core_operation_counters();

    expect(fatal(bool(definition.has_value())));
    expect(fatal(bool(reference.has_value())));
    expect(fatal(bool(preview == "preview body")));
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(counters.full_document_text_projections == 0u)));
};

"normal_source_edits_never_parse_or_serialize_the_full_document"_test = [] {
    Editor editor("**alpha**\n\nbeta");
    editor.set_selection(caret(first_text(editor), 3));
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_command(Command::InsertText(U"X")))));
    const auto edit = read_core_operation_counters();
    expect(fatal(bool(edit.full_document_parses == 0u)));
    expect(fatal(bool(edit.full_document_serializations == 0u)));
    expect(fatal(bool(edit.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(edit.full_document_block_index_scans == 0u)));
    expect(fatal(bool(edit.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(edit.full_document_outline_derivations == 0u)));
    expect(fatal(bool(edit.local_symbol_derivations == 1u)));
    expect(fatal(bool(edit.inline_reparses == 1u)));

    reset_core_operation_counters();
    expect(fatal(bool(editor.undo())));
    const auto undo = read_core_operation_counters();
    expect(fatal(bool(undo.full_document_parses == 0u)));
    expect(fatal(bool(undo.full_document_serializations == 0u)));
    expect(fatal(bool(undo.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(undo.full_document_block_index_scans == 0u)));
    expect(fatal(bool(undo.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(undo.full_document_outline_derivations == 0u)));
    expect(fatal(bool(undo.local_symbol_derivations == 1u)));
    expect(fatal(bool(undo.inline_reparses == 1u)));
};

"opening_a_nonempty_document_builds_the_compact_block_locator_index_once"_test = [] {
    reset_core_operation_counters();
    Editor editor("alpha\n\nbeta\n\ngamma");
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_block_index_scans == 1u)));
    expect(fatal(bool(counters.incremental_document_block_index_repairs == 0u)));
    expect(fatal(bool(find_document_block(
        editor.document(), editor.document().root.children.back().id) != nullptr)));
};

"local_symbol_contributions_refresh_only_the_edited_owner"_test = [] {
    Editor prose("# title\n\nalpha\n\nbeta\n\ngamma");
    const auto& paragraph = prose.document().root.children.back();
    auto const initial_outline_content_revision = prose.outline().content_revision;
    prose.set_selection(caret(paragraph, paragraph.inline_content().source.size()));
    reset_core_operation_counters();
    expect(fatal(prose.execute_command(Command::InsertText(U"!"))));
    auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.local_symbol_derivations == 1u)));
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(counters.full_document_outline_derivations == 0u)));
    expect(fatal(bool(prose.outline().flat_items().front()->title_plain_text == "title")));
    expect(fatal(bool(prose.outline().content_revision == initial_outline_content_revision)));

    const auto& heading = prose.document().root.children.front();
    prose.set_selection(caret(heading, heading.inline_content().source.size()));
    reset_core_operation_counters();
    expect(fatal(prose.execute_command(Command::InsertText(U"!"))));
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.local_symbol_derivations == 1u)));
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(counters.full_document_outline_derivations == 1u)));
    expect(fatal(bool(prose.outline().flat_items().front()->title_plain_text == "title!")));
    expect(fatal(bool(prose.outline().content_revision == prose.revision())));

    Editor link("[x](a)\n\nplain");
    const auto& link_owner = link.document().root.children.front();
    link.set_selection(range(link_owner, 4, 5));
    reset_core_operation_counters();
    expect(fatal(link.execute_command(Command::InsertText(U"b"))));
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.local_symbol_derivations == 1u)));
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(counters.full_document_outline_derivations == 0u)));
    expect(fatal(bool(link.symbols().links.size() == 1u)));
    expect(fatal(bool(link.symbols().links.front().href == "b")));
    expect(fatal(link.undo()));
    expect(fatal(bool(link.symbols().links.front().href == "a")));
};

"node_ids_are_document_owned_and_lexical_edits_reuse_identities_without_tree_scans"_test = [] {
    Editor editor("abc");
    const auto parsed_cursor = editor.document().next_node_id;
    expect(fatal(bool(parsed_cursor > 0u)));

    const auto literal_id = first_text(editor).inline_content().tree.nodes.front().id;
    editor.set_selection(caret(first_text(editor), 1));
    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command::InsertText(U"X"))));
    const auto edited_cursor = editor.document().next_node_id;
    expect(fatal(bool(edited_cursor == parsed_cursor)));
    expect(fatal(bool(read_core_operation_counters().full_document_block_index_scans == 0u)));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    const auto undone_cursor = editor.document().next_node_id;
    expect(fatal(bool(undone_cursor == edited_cursor)));
    expect(fatal(bool(read_core_operation_counters().full_document_block_index_scans == 0u)));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect(fatal(bool(editor.document().next_node_id == undone_cursor)));
    expect(fatal(bool(first_text(editor).inline_content().tree.nodes.front().id == literal_id)));
    expect(fatal(bool(read_core_operation_counters().full_document_block_index_scans == 0u)));
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
    expect(fatal(bool(editor.document().next_node_id > undone_cursor)));
};

"incremental inline nested edits keep source selection and history exact in block containers"_test = [] {
    for (auto source : {"**alpha *beta gamma* omega**", "# **alpha *beta gamma* omega**",
        "> **alpha *beta gamma* omega**", "- **alpha *beta gamma* omega**",
        "- [ ] **alpha *beta gamma* omega**", "| **alpha *beta gamma* omega** |\n| --- |",
        "> [!NOTE]\n> **alpha *beta gamma* omega**", "[^note]: **alpha *beta gamma* omega**"}) {
        Editor editor(source);
        auto saved = editor.markdown_utf8();
        const BlockNode* owner = nullptr;
        walk_blocks(editor.document().root, [&](auto const& node) {
            if (node.inline_content().source.find(U"gamma") != std::u32string::npos) owner = &node;
        });
        expect(fatal(owner != nullptr));
        auto originalSource = owner->inline_content().source;
        auto before = caret(*owner, originalSource.find(U"gamma") + 2);
        auto expected = originalSource;
        expected.insert(before.active.source_offset, U"X");
        editor.set_selection(before);
        auto verify = [&](auto action) {
            reset_core_operation_counters();
            expect(fatal(action()));
            auto counters = read_core_operation_counters();
            expect(counters.inline_reparses == 1_u);
            expect(counters.inline_scanned_characters < 128_u);
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
        };
        verify([&] { return editor.execute_command(Command::InsertText(U"X")); });
        auto after = editor.selection();
        expect(after.active.source_offset == before.active.source_offset + 1);
        expect(bool(find_document_block(editor.document(), before.active.container_id)->inline_content().source == expected));
        verify([&] { return editor.undo(); });
        expect(editor.selection() == before);
        expect(bool(find_document_block(editor.document(), before.active.container_id)->inline_content().source == originalSource));
        expect(editor.markdown_utf8() == saved);
        verify([&] { return editor.redo(); });
        expect(editor.selection() == after);
        expect(bool(find_document_block(editor.document(), before.active.container_id)->inline_content().source == expected));
    }
};

"long paragraph literal edits avoid owner-wide parsing and identity history"_test = [] {
    std::string source;
    for (int i = 0; i < 10000; ++i) source += "Sample paragraph with words.\n";
    Editor editor(source);
    editor.set_selection(caret(first_text(editor), 3));
    auto verify = [&](auto action) {
        reset_core_operation_counters();
        expect(fatal(action()));
        auto counters = read_core_operation_counters();
        expect(counters.inline_reparses == 1_u);
        expect(counters.inline_scanned_characters < 128_u);
        expect(counters.full_document_parses == 0_u);
        expect(counters.full_document_serializations == 0_u);
    };
    verify([&] { return editor.execute_command(Command::InsertText(U"X")); });
    verify([&] { return editor.undo(); });
    verify([&] { return editor.redo(); });
};

};
