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

suite editing_raw_blocks_marker_boundaries_tests = [] {

"rendered_text_replacement_cannot_edit_fenced_code_info"_test = [] {
    Editor editor;
    Command insert_code;
    insert_code.kind = CommandKind::InsertCodeBlock;
    insert_code.lang = U"cpp";
    expect(fatal(bool(editor.execute_document_insert_atomic_block(
        editor.selection(), insert_code).has_value())));
    const auto code_id = editor.selection().active.container_id;
    const TextSelection language{
        {code_id, 3, TextAffinity::Downstream},
        {code_id, 6, TextAffinity::Downstream}};
    editor.set_selection(language);
    expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"js").has_value())));
    auto const* block = find_block(editor.document().root, code_id);
    expect(fatal(bool(block && block->block_source.source() == U"```cpp\njs\n```")));
    expect(fatal(bool(block && block->block_source.tree().language
        == std::optional<std::string>{"cpp"})));
    const auto after = editor.selection();
    expect(fatal(bool(editor.undo())));
    block = find_block(editor.document().root, code_id);
    expect(fatal(bool(block && block->block_source.source() == U"```cpp\n\n```")));
    expect(fatal(bool(editor.selection() == language)));
    expect(fatal(bool(editor.redo())));
    block = find_block(editor.document().root, code_id);
    expect(fatal(bool(block && block->block_source.source() == U"```cpp\njs\n```")));
    expect(fatal(bool(editor.selection() == after)));
};

"rendered_raw_block_navigation_never_enters_hidden_markers"_test = [] {
    Editor editor("before\n\n```cpp\nab\n```\n\nafter");
    const auto& before = editor.document().root.children[0];
    const auto& code = editor.document().root.children[1];
    const auto& after = editor.document().root.children[2];
    const auto code_range = block_source_editable_range(code.block_source);

    editor.set_selection(TextSelection::caret({
        code.id, code_range.start, TextAffinity::Downstream}));
    expect(fatal(editor.execute_command(Command::MoveLeft(false))));
    expect(fatal(editor.selection().active.container_id == before.id));
    expect(fatal(editor.selection().active.source_offset
        == before.inline_content().source.size()));

    expect(fatal(editor.execute_command(Command::MoveRight(false))));
    expect(fatal(editor.selection().active.container_id == code.id));
    expect(fatal(editor.selection().active.source_offset == code_range.start));
    editor.set_selection(TextSelection::caret({
        code.id, code_range.end, TextAffinity::Upstream}));
    expect(fatal(editor.execute_command(Command::MoveRight(false))));
    expect(fatal(editor.selection().active.container_id == after.id));
    expect(fatal(editor.selection().active.source_offset == 0u));
};

"raw_content_deletion_preserves_every_structural_delimiter"_test = [] {
    Editor editor("```cpp\nab\n```");
    const auto code_id = editor.document().root.children.front().id;
    const auto mapping = editor.document().root.children.front()
        .block_source.tree().content_to_source;
    editor.set_selection(TextSelection::caret({
        code_id, mapping[1], TextAffinity::Downstream}));
    expect(fatal(bool(editor.execute_document_delete_backward(
        editor.selection()).has_value())));
    auto const* code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code && code->block_source.source() == U"```cpp\nb\n```")));
    expect(fatal(bool(code && code->block_source.tree().complete_opening)));
    expect(fatal(bool(code && code->block_source.tree().complete_closing)));
    expect(fatal(editor.undo()));

    editor.set_selection(TextSelection::caret({
        code_id, mapping[1], TextAffinity::Downstream}));
    expect(fatal(bool(editor.execute_document_delete_forward(
        editor.selection()).has_value())));
    code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code && code->block_source.source() == U"```cpp\na\n```")));
    expect(fatal(bool(code && code->block_source.tree().complete_opening)));
    expect(fatal(bool(code && code->block_source.tree().complete_closing)));
};

"rendered_raw_marker_ranges_are_inert_and_never_reclassify_the_block"_test = [] {
    for (const auto& markdown : std::vector<std::string>{
            "```cpp\nvalue\n```",
            "```math\nvalue\n```",
        }) {
        Editor editor(markdown);
        const auto owner_id = editor.document().root.children.front().id;
        const auto before = editor.markdown_utf8();
        const auto hidden_opening_end = editor.document().root.children.front()
            .block_source.tree().content_to_source.front();
        editor.set_selection({
            {owner_id, 0, TextAffinity::Downstream},
            {owner_id, hidden_opening_end, TextAffinity::Downstream},
        });

        reset_core_operation_counters();
        expect(fatal(!editor.execute_document_delete_selection(
            editor.selection()).has_value())) << markdown;
        const auto counters = read_core_operation_counters();
        expect(fatal(counters.full_document_parses == 0u)) << markdown;
        expect(fatal(counters.full_document_serializations == 0u)) << markdown;
        expect(fatal(editor.markdown_utf8() == before)) << markdown;
        expect(fatal(validate_document(editor.document()).empty())) << markdown;
    }
};

};
