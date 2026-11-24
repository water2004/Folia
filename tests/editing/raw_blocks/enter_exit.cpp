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

suite editing_raw_blocks_enter_exit_tests = [] {

"empty_raw_structures_are_deleted_as_one_reversible_tree_edit"_test = [] {
    for (auto const& markdown : std::vector<std::string>{
            "```\n```",
            "```cpp\n```",
            "~~~\n~~~",
            "$$\n$$",
        }) {
        for (const auto delete_backward : {false, true}) {
            Editor editor(markdown);
            const auto& raw = editor.document().root.children.front();
            const auto position = block_source_editable_range(raw.block_source).start;
            editor.set_selection(TextSelection::caret({
                raw.id, position, TextAffinity::Downstream}));
            const auto before = editor.selection();
            const auto deleted = delete_backward
                ? editor.execute_document_delete_backward(editor.selection())
                : editor.execute_document_delete_forward(editor.selection());
            expect(fatal(bool(deleted.has_value()))) << markdown;
            expect(fatal(editor.document().root.children.size() == 1u)) << markdown;
            expect(fatal(editor.document().root.children.front().kind
                == BlockKind::Paragraph)) << markdown;
            expect(fatal(editor.markdown_utf8().empty())) << markdown;
            expect(fatal(editor.undo())) << markdown;
            expect(fatal(editor.markdown_utf8() == markdown)) << markdown;
            expect(fatal(editor.selection() == before)) << markdown;
            expect(fatal(editor.redo())) << markdown;
            expect(fatal(editor.document().root.children.front().kind
                == BlockKind::Paragraph)) << markdown;
        }
    }
};

"enter_on_trailing_empty_raw_line_exits_and_removes_that_line"_test = [] {
    for (auto const& test_case : std::vector<std::pair<std::string, std::string>>{
            {"```cpp\nvalue\n\n```", "```cpp\nvalue\n```"},
            {"$$\nx + y\n\n$$", "$$\nx + y\n$$"},
        }) {
        Editor editor(test_case.first);
        const auto raw_id = editor.document().root.children.front().id;
        const auto content_end = block_source_editable_range(
            editor.document().root.children.front().block_source).end;
        editor.set_selection(TextSelection::caret({
            raw_id, content_end, TextAffinity::Downstream}));
        expect(fatal(bool(editor.execute_document_enter(
            editor.selection()).has_value()))) << test_case.first;
        expect(fatal(editor.document().root.children.size() == 2u)) << test_case.first;
        expect(fatal(editor.document().root.children.front().block_source.source()
            == utf8_to_cps(test_case.second))) << test_case.first;
        expect(fatal(editor.document().root.children.back().kind
            == BlockKind::Paragraph)) << test_case.first;
        expect(fatal(editor.selection().active.container_id
            == editor.document().root.children.back().id)) << test_case.first;
        expect(fatal(editor.undo())) << test_case.first;
        expect(fatal(editor.markdown_utf8() == test_case.first)) << test_case.first;
    }
};

"enter_on_an_empty_raw_frame_exits_without_exposing_a_delimiter"_test = [] {
    for (auto const& markdown : std::vector<std::string>{
            "```cpp\n```",
            "$$\n$$",
        }) {
        Editor editor(markdown);
        const auto& raw = editor.document().root.children.front();
        const auto visible = block_source_editable_range(raw.block_source);
        editor.set_selection(TextSelection::caret({
            raw.id, visible.end, TextAffinity::Downstream}));
        expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())))
            << markdown;
        expect(fatal(editor.document().root.children.size() == 2u)) << markdown;
        expect(fatal(editor.document().root.children.front().block_source.source()
            == utf8_to_cps(markdown))) << markdown;
        expect(fatal(editor.document().root.children.back().kind == BlockKind::Paragraph))
            << markdown;
    }
};

"enter_in_incomplete_fenced_code_edits_local_source_without_crashing"_test = [] {
    Editor editor("```cpp\n");
    auto const& code = editor.document().root.children.front();
    expect(fatal(bool(code.kind == BlockKind::CodeBlock)));
    expect(fatal(bool(!code.block_source.tree().complete_closing)));
    editor.set_selection(caret(code, code.block_source.source().size()));
    const auto before = editor.selection();
    expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())));
    auto const* changed = find_block(editor.document().root, code.id);
    expect(fatal(bool(changed && changed->block_source.source() == U"```cpp\n\n")));
    expect(fatal(bool(changed && block_source_tokens_partition(changed->block_source))));
    expect(fatal(bool(changed && flatten_block_source_tokens(changed->block_source)
        == changed->block_source.source())));
    expect(fatal(bool(editor.selection().active.source_offset == 8u)));
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.selection() == before)));
    expect(fatal(bool(editor.redo())));
};

"enter_after_a_complete_raw_block_creates_a_sibling_paragraph"_test = [] {
    for (const auto& markdown : {
            std::string{"$$\nx + y\n$$"},
            std::string{"```cpp\nvalue\n```"},
        }) {
        Editor editor(markdown);
        const auto raw_id = editor.document().root.children.front().id;
        const auto raw_size = editor.document().root.children.front().block_source.source().size();
        const auto raw_source = editor.document().root.children.front().block_source.source();
        editor.set_selection(TextSelection::caret({
            raw_id,
            raw_size,
            TextAffinity::Downstream,
        }));
        expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value()))) << markdown;
        expect(fatal(bool(editor.document().root.children.size() == 2u))) << markdown;
        expect(fatal(bool(editor.document().root.children.front().id == raw_id))) << markdown;
        expect(fatal(bool(editor.document().root.children.front().block_source.source() == raw_source))) << markdown;
        expect(fatal(bool(editor.document().root.children.back().kind == BlockKind::Paragraph))) << markdown;
        expect(fatal(bool(editor.selection().active.container_id
            == editor.document().root.children.back().id))) << markdown;
        expect(fatal(bool(editor.selection().active.source_offset == 0u))) << markdown;
    }
};

"enter_after_nested_complete_raw_block_stays_in_its_direct_container"_test = [] {
    Editor editor("> - ```cpp\n>   value\n>   ```");
    const BlockNode* code = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (!code && block.kind == BlockKind::CodeBlock) code = &block;
    });
    expect(fatal(bool(code != nullptr)));
    if (!code) return;
    editor.set_selection(TextSelection::caret({
        code->id,
        code->block_source.source().size(),
        TextAffinity::Downstream,
    }));
    expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())));
    const auto* paragraph = find_block(
        editor.document().root,
        editor.selection().active.container_id);
    expect(fatal(bool(paragraph && paragraph->kind == BlockKind::Paragraph)));
    expect(fatal(bool(editor.document().root.children.front().kind == BlockKind::BlockQuote)));
    expect(fatal(bool(editor.document().root.children.front().children.front().kind == BlockKind::List)));
    const BlockNode* list_item = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (!list_item && block.kind == BlockKind::ListItem) list_item = &block;
    });
    expect(fatal(bool(list_item != nullptr)));
    if (!list_item) return;
    expect(fatal(bool(list_item->children.size() == 2u)));
    expect(fatal(bool(list_item->children.front().kind == BlockKind::CodeBlock)));
    expect(fatal(bool(list_item->children.back().id == editor.selection().active.container_id)));
};

};
