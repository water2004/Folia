#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.container_source;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

using PrefixSnapshot = std::vector<std::pair<NodeId, ContainerSource>>;

const BlockNode* find_paragraph(const EditorDocument& document, std::u32string_view source) {
    const BlockNode* result = nullptr;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (!result && block.kind == BlockKind::Paragraph
            && block.inline_content.source == source) result = &block;
    });
    return result;
}

const BlockNode* find_indented_code(const EditorDocument& document) {
    const BlockNode* result = nullptr;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (!result && block.kind == BlockKind::CodeBlock
            && block.atomic_special().code_indented) result = &block;
    });
    return result;
}

const BlockNode* find_kind(const BlockNode& block, BlockKind kind) {
    if (block.kind == kind) return &block;
    for (const auto& child : block.children) {
        if (const auto* found = find_kind(child, kind)) return found;
    }
    return nullptr;
}

PrefixSnapshot prefix_snapshot(const EditorDocument& document) {
    PrefixSnapshot result;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (block.has_container_source()) result.emplace_back(block.id, block.container_source());
    });
    return result;
}

bool inline_sources_are_lossless(const BlockNode& block) {
    if ((block.kind == BlockKind::Paragraph || block.kind == BlockKind::Heading
         || block.kind == BlockKind::CalloutTitle || block.kind == BlockKind::TableCell)
        && (!tokens_partition_source(block.inline_content.tree, block.inline_content.source.size())
            || !roots_partition_source(block.inline_content.tree, block.inline_content.source.size())
            || flatten_tokens(block.inline_content.tree, block.inline_content.source)
                != block.inline_content.source
            || serialize_lossless(block.inline_content.tree, block.inline_content.source)
                != block.inline_content.source)) {
        return false;
    }
    for (const auto& child : block.children) {
        if (!inline_sources_are_lossless(child)) return false;
    }
    return true;
}

void expect_local_counters(std::string_view label) {
    const auto counters = read_core_operation_counters();
    expect(fatal(counters.full_document_parses == 0u)) << label << " full parse";
    expect(fatal(counters.full_document_serializations == 0u)) << label << " full serialization";
    expect(fatal(counters.full_tree_transaction_diffs == 0u)) << label << " full tree diff";
}

std::vector<BlockLineIndentation> indentation_contexts(const EditorDocument& document, NodeId id) {
    const auto* block = find_block(document.root, id);
    if (!block || !block->block_source.storage) return {};
    return block->block_source.storage->indentation_contexts;
}

bool indentation_contexts_fit_source(const BlockNode& block) {
    if (!block.block_source.storage) return false;
    const auto& source = block.block_source.source();
    const auto& contexts = block.block_source.storage->indentation_contexts;
    if (contexts.empty() || !std::ranges::is_sorted(
            contexts, {}, &BlockLineIndentation::source_offset)) return false;
    return std::ranges::all_of(contexts, [&](const BlockLineIndentation& context) {
        return context.source_offset <= source.size();
    });
}

bool raw_source_is_lossless(const BlockNode& block) {
    return block.block_source.storage
        && block_source_tokens_partition(block.block_source)
        && flatten_block_source_tokens(block.block_source) == block.block_source.source();
}

void exercise_lazy_quote_edit(
    std::string_view label,
    std::string const& before,
    std::u32string_view before_inline,
    std::size_t edit_offset,
    std::u32string_view after_inline,
    std::string const& after) {
    Editor editor(before);
    expect(fatal(editor.markdown_utf8() == before)) << label << " initial exact save";
    const auto& roots = editor.document().root.children;
    expect(fatal(roots.size() == 1u)) << label << " initial root count";
    if (roots.size() != 1u) return;
    const auto& quote = roots.front();
    expect(fatal(quote.kind == BlockKind::BlockQuote)) << label << " initial quote";
    if (quote.kind != BlockKind::BlockQuote) return;
    expect(fatal(quote.children.size() == 1u)) << label << " initial child count";
    if (quote.children.size() != 1u) return;
    const auto* paragraph = find_paragraph(editor.document(), before_inline);
    expect(fatal(paragraph != nullptr)) << label << " initial lazy paragraph";
    if (!paragraph) return;

    const auto owner_id = paragraph->id;
    const auto before_selection = TextSelection::caret(
        {owner_id, edit_offset, TextAffinity::Downstream});
    editor.set_selection(before_selection);
    const auto before_prefixes = prefix_snapshot(editor.document());

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"x").has_value()))
        << label << " edit";
    expect_local_counters("lazy quote edit");
    const auto after_selection = editor.selection();
    const auto after_prefixes = prefix_snapshot(editor.document());
    expect(fatal(after_selection.active.container_id == owner_id)) << label << " selection owner";
    expect(fatal(after_selection.active.source_offset == edit_offset + 1u)) << label << " source offset";
    expect(fatal(inline_sources_are_lossless(editor.document().root))) << label << " edited CST";

    // Capture the save only after the operation counters above have been read.
    const auto after_markdown = editor.markdown_utf8();
    expect(fatal(after_markdown == after)) << label << " edited exact save";
    const auto* changed_paragraph = find_paragraph(editor.document(), after_inline);
    expect(fatal(changed_paragraph != nullptr)) << label << " edited quote paragraph";
    if (changed_paragraph) expect(fatal(changed_paragraph->id == owner_id)) << label << " owner identity";

    reset_core_operation_counters();
    expect(fatal(editor.undo())) << label << " undo";
    expect_local_counters("lazy quote undo");
    expect(fatal(editor.markdown_utf8() == before)) << label << " undo exact bytes";
    expect(fatal(editor.selection() == before_selection)) << label << " undo selection";
    expect(fatal(prefix_snapshot(editor.document()) == before_prefixes)) << label << " undo prefixes";

    reset_core_operation_counters();
    expect(fatal(editor.redo())) << label << " redo";
    expect_local_counters("lazy quote redo");
    expect(fatal(editor.markdown_utf8() == after)) << label << " redo exact bytes";
    expect(fatal(editor.selection() == after_selection)) << label << " redo selection";
    expect(fatal(prefix_snapshot(editor.document()) == after_prefixes)) << label << " redo prefixes";

    Editor reloaded(after_markdown);
    expect(fatal(reloaded.markdown_utf8() == after)) << label << " reload exact bytes";
    expect(fatal(reloaded.document().root.children.size() == 1u)) << label << " reload root count";
    if (reloaded.document().root.children.size() == 1u) {
        const auto& reloaded_quote = reloaded.document().root.children.front();
        expect(fatal(reloaded_quote.kind == BlockKind::BlockQuote)) << label << " reload quote";
        expect(fatal(reloaded_quote.children.size() == 1u)) << label << " reload quote children";
        if (reloaded_quote.children.size() == 1u) {
            expect(fatal(reloaded_quote.children.front().kind == BlockKind::Paragraph))
                << label << " reload paragraph remains literal";
            expect(fatal(reloaded_quote.children.front().inline_content.source == after_inline))
                << label << " reload block-local source";
        }
    }
    expect(fatal(inline_sources_are_lossless(reloaded.document().root))) << label << " reload CST";
}

} // namespace

suite lazy_container_source_history_tests = [] {

"editing_commonmark_93_keeps_the_setext_looking_quote_line_literal"_test = [] {
    exercise_lazy_quote_edit(
        "CommonMark #93",
        "> foo\nbar\n===\n",
        U"foo\nbar\n===",
        9,
        U"foo\nbar\n=x==",
        "> foo\nbar\n=x==\n");
};

"editing_commonmark_238_keeps_the_lazy_list_marker_in_the_quote_paragraph"_test = [] {
    exercise_lazy_quote_edit(
        "CommonMark #238",
        "> foo\n    - bar\n",
        U"foo\n- bar",
        7,
        U"foo\n- bxar",
        "> foo\n    - bxar\n");
};

"raw_indented_code_column_context_survives_edit_enter_delete_and_history"_test = [] {
    const std::vector<std::pair<std::string, std::u32string>> cases{
        {"- foo\n\n\t\tbar\n", U"  bar"}, // CommonMark #5
        {">\t\tfoo\n", U"  foo"},          // CommonMark #6
        {"-\t\tfoo\n", U"  foo"},           // CommonMark #7
    };
    for (const auto& [markdown, expected_projection] : cases) {
        Editor editor(markdown);
        const auto* code = find_indented_code(editor.document());
        expect(fatal(code != nullptr)) << markdown;
        if (!code) continue;
        const auto code_id = code->id;
        const auto initial_raw_source = code->block_source.source();
        const auto initial_contexts = indentation_contexts(editor.document(), code_id);
        expect(fatal(!initial_contexts.empty())) << markdown << " source indentation context";
        expect(fatal(indentation_contexts_fit_source(*code))) << markdown << " initial context ranges";
        expect(fatal(raw_source_is_lossless(*code))) << markdown << " initial raw source/CST";
        expect(fatal(block_source_code_projection(code->block_source) == expected_projection))
            << markdown << " initial semantic projection";
        const auto initial_markdown = editor.markdown_utf8();
        const auto initial_prefixes = prefix_snapshot(editor.document());
        const auto edit_offset = code->block_source.tree().content_to_source.front();
        const auto before_selection = TextSelection::caret(
            {code_id, edit_offset, TextAffinity::Downstream});
        editor.set_selection(before_selection);

        reset_core_operation_counters();
        expect(fatal(editor.execute_document_insert_text(editor.selection(), U"X").has_value()))
            << markdown << " character edit";
        expect_local_counters("raw code character edit");
        expect(fatal(read_core_operation_counters().inline_reparses == 0u))
            << markdown << " raw source does not reparse inline CST";
        const auto edited_raw = document_editable_text(editor.document(), code_id);
        expect(fatal(edited_raw.has_value())) << markdown;
        const auto edited_contexts = indentation_contexts(editor.document(), code_id);
        const auto edited_selection = editor.selection();
        const auto* edited_block = find_block(editor.document().root, code_id);
        expect(fatal(edited_block != nullptr)) << markdown << " edited code block";
        if (edited_block) {
            expect(fatal(indentation_contexts_fit_source(*edited_block)))
                << markdown << " edited context ranges";
            expect(fatal(raw_source_is_lossless(*edited_block))) << markdown << " edited raw source/CST";
        }
        const auto edited_markdown = editor.markdown_utf8();

        reset_core_operation_counters();
        expect(fatal(editor.execute_document_enter(editor.selection()).has_value()))
            << markdown << " raw newline";
        expect_local_counters("raw code Enter");
        expect(fatal(read_core_operation_counters().inline_reparses == 0u))
            << markdown << " Enter does not reparse inline CST";
        const auto entered_raw = document_editable_text(editor.document(), code_id);
        expect(fatal(entered_raw.has_value())) << markdown << " Enter source";
        const auto entered_contexts = indentation_contexts(editor.document(), code_id);
        const auto entered_selection = editor.selection();
        const auto* entered_block = find_block(editor.document().root, code_id);
        expect(fatal(entered_block != nullptr)) << markdown;
        if (entered_block) {
            expect(fatal(indentation_contexts_fit_source(*entered_block)))
                << markdown << " Enter context ranges";
            expect(fatal(raw_source_is_lossless(*entered_block))) << markdown << " Enter raw source/CST";
        }
        const auto entered_markdown = editor.markdown_utf8();

        reset_core_operation_counters();
        expect(fatal(editor.execute_document_delete_backward(editor.selection()).has_value()))
            << markdown << " remove inserted line break";
        expect_local_counters("raw code newline deletion");
        expect(fatal(read_core_operation_counters().inline_reparses == 0u))
            << markdown << " raw delete does not reparse inline CST";
        expect(fatal(document_editable_text(editor.document(), code_id) == edited_raw))
            << markdown << " source after newline deletion";
        expect(fatal(indentation_contexts(editor.document(), code_id) == edited_contexts))
            << markdown << " exact contexts after newline deletion";
        const auto* deleted_block = find_block(editor.document().root, code_id);
        expect(fatal(deleted_block != nullptr)) << markdown << " deleted code block";
        if (deleted_block) expect(fatal(raw_source_is_lossless(*deleted_block)))
            << markdown << " deleted raw source/CST";
        const auto deleted_selection = editor.selection();
        const auto deleted_markdown = editor.markdown_utf8();
        expect(fatal(deleted_markdown == edited_markdown)) << markdown;

        reset_core_operation_counters();
        expect(fatal(editor.undo())) << markdown << " undo newline deletion";
        expect_local_counters("undo raw newline deletion");
        expect(fatal(document_editable_text(editor.document(), code_id) == entered_raw)) << markdown;
        expect(fatal(indentation_contexts(editor.document(), code_id) == entered_contexts)) << markdown;
        expect(fatal(editor.selection() == entered_selection)) << markdown;
        expect(fatal(editor.markdown_utf8() == entered_markdown)) << markdown;

        reset_core_operation_counters();
        expect(fatal(editor.undo())) << markdown << " undo Enter";
        expect_local_counters("undo raw Enter");
        expect(fatal(document_editable_text(editor.document(), code_id) == edited_raw)) << markdown;
        expect(fatal(indentation_contexts(editor.document(), code_id) == edited_contexts)) << markdown;

        reset_core_operation_counters();
        expect(fatal(editor.undo())) << markdown << " undo character edit";
        expect_local_counters("undo raw character edit");
        expect(fatal(document_editable_text(editor.document(), code_id) == initial_raw_source)) << markdown;
        expect(fatal(indentation_contexts(editor.document(), code_id) == initial_contexts)) << markdown;
        expect(fatal(prefix_snapshot(editor.document()) == initial_prefixes)) << markdown;
        expect(fatal(editor.markdown_utf8() == initial_markdown)) << markdown;
        expect(fatal(editor.selection() == before_selection)) << markdown;

        reset_core_operation_counters();
        expect(fatal(editor.redo())) << markdown << " redo character edit";
        expect_local_counters("redo raw character edit");
        expect(fatal(document_editable_text(editor.document(), code_id) == edited_raw)) << markdown;
        expect(fatal(indentation_contexts(editor.document(), code_id) == edited_contexts)) << markdown;
        expect(fatal(editor.selection() == edited_selection)) << markdown;

        reset_core_operation_counters();
        expect(fatal(editor.redo())) << markdown << " redo Enter";
        expect_local_counters("redo raw Enter");
        expect(fatal(document_editable_text(editor.document(), code_id) == entered_raw)) << markdown;
        expect(fatal(indentation_contexts(editor.document(), code_id) == entered_contexts)) << markdown;

        reset_core_operation_counters();
        expect(fatal(editor.redo())) << markdown << " redo newline deletion";
        expect_local_counters("redo raw newline deletion");
        expect(fatal(editor.selection() == deleted_selection)) << markdown;
        expect(fatal(document_editable_text(editor.document(), code_id) == edited_raw)) << markdown;
        expect(fatal(indentation_contexts(editor.document(), code_id) == edited_contexts)) << markdown;
        expect(fatal(editor.markdown_utf8() == edited_markdown)) << markdown;

        Editor reloaded(edited_markdown);
        const auto* reloaded_code = find_indented_code(reloaded.document());
        expect(fatal(reloaded_code != nullptr)) << markdown << " reload code block";
        if (reloaded_code) {
            expect(fatal(reloaded_code->block_source.storage->indentation_contexts == edited_contexts))
                << markdown << " reload exact context records";
            const auto* current_code = find_block(editor.document().root, code_id);
            expect(fatal(current_code != nullptr)) << markdown << " current code block";
            if (current_code) {
                expect(fatal(bool(block_source_code_projection(reloaded_code->block_source)
                    == block_source_code_projection(current_code->block_source))))
                    << markdown << " reload semantic code projection";
            }
        }
    }
};

"item_opening_prefix_uses_updated_task_and_ordered_markers"_test = [] {
    {
        const std::string source = "- [ ] task\n";
        Editor editor(source);
        const auto* item = find_kind(editor.document().root, BlockKind::TaskListItem);
        expect(fatal(item != nullptr));
        if (!item) return;
        const auto* paragraph = find_paragraph(editor.document(), U"task");
        expect(fatal(paragraph != nullptr));
        if (!paragraph) return;
        editor.set_selection(TextSelection::caret(
            {paragraph->id, 1, TextAffinity::Downstream}));
        const auto selection = editor.selection();
        const auto before_prefixes = prefix_snapshot(editor.document());

        reset_core_operation_counters();
        expect(fatal(editor.execute_document_toggle_task_checkbox(editor.selection()).has_value()));
        expect_local_counters("task checkbox marker change");
        const auto checked_source = editor.markdown_utf8();
        const auto checked_prefixes = prefix_snapshot(editor.document());
        expect(fatal(checked_source == "- [x] task\n"));
        expect(fatal(editor.selection() == selection));

        reset_core_operation_counters();
        expect(fatal(editor.undo()));
        expect_local_counters("task checkbox undo");
        expect(fatal(editor.markdown_utf8() == source));
        expect(fatal(editor.selection() == selection));
        expect(fatal(prefix_snapshot(editor.document()) == before_prefixes));

        reset_core_operation_counters();
        expect(fatal(editor.redo()));
        expect_local_counters("task checkbox redo");
        expect(fatal(editor.markdown_utf8() == checked_source));
        expect(fatal(editor.selection() == selection));
        expect(fatal(prefix_snapshot(editor.document()) == checked_prefixes));
    }
    {
        const std::string source = "8. first\n9. second\n";
        Editor editor(source);
        const auto* paragraph = find_paragraph(editor.document(), U"second");
        expect(fatal(paragraph != nullptr));
        if (!paragraph) return;
        editor.set_selection(TextSelection::caret(
            {paragraph->id, 0, TextAffinity::Downstream}));
        const auto before_selection = editor.selection();
        const auto before_prefixes = prefix_snapshot(editor.document());

        reset_core_operation_counters();
        expect(fatal(editor.execute_document_enter(editor.selection()).has_value()));
        expect_local_counters("ordered item Enter at source offset zero");
        const auto split_source = editor.markdown_utf8();
        const auto split_selection = editor.selection();
        const auto split_prefixes = prefix_snapshot(editor.document());
        expect(fatal(split_source == "8. first\n9. \n10. second\n"));
        expect(fatal(split_source.find("9. 9. ") == std::string::npos));
        expect(fatal(split_source.find("10. 10. ") == std::string::npos));

        reset_core_operation_counters();
        expect(fatal(editor.undo()));
        expect_local_counters("ordered item Enter undo");
        expect(fatal(editor.markdown_utf8() == source));
        expect(fatal(editor.selection() == before_selection));
        expect(fatal(prefix_snapshot(editor.document()) == before_prefixes));

        reset_core_operation_counters();
        expect(fatal(editor.redo()));
        expect_local_counters("ordered item Enter redo");
        expect(fatal(editor.markdown_utf8() == split_source));
        expect(fatal(editor.selection() == split_selection));
        expect(fatal(prefix_snapshot(editor.document()) == split_prefixes));
    }
};

"nested_task_item_edits_preserve_source_and_exact_undo_redo"_test = [] {
    const std::string source =
        "- [x] foo\n"
        "  - [ ] bar\n"
        "  - [x] baz\n"
        "- [ ] bim\n";
    Editor editor(source);
    const auto* nested_list = find_kind(editor.document().root, BlockKind::TaskList);
    expect(fatal(nested_list != nullptr));
    if (!nested_list || nested_list->children.size() != 2) return;
    const auto* paragraph = find_paragraph(editor.document(), U"bar");
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    expect(fatal(inline_sources_are_lossless(editor.document().root)));
    const auto before_prefixes = prefix_snapshot(editor.document());
    const auto before_selection = TextSelection::caret(
        {paragraph->id, 1, TextAffinity::Downstream});
    editor.set_selection(before_selection);

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"X").has_value()));
    expect_local_counters("nested task inline edit");
    const std::string edited =
        "- [x] foo\n"
        "  - [ ] bXar\n"
        "  - [x] baz\n"
        "- [ ] bim\n";
    const auto after_selection = editor.selection();
    const auto after_prefixes = prefix_snapshot(editor.document());
    expect(fatal(editor.markdown_utf8() == edited));
    expect(fatal(inline_sources_are_lossless(editor.document().root)));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_counters("nested task inline undo");
    expect(fatal(editor.markdown_utf8() == source));
    expect(fatal(editor.selection() == before_selection));
    expect(fatal(prefix_snapshot(editor.document()) == before_prefixes));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_counters("nested task inline redo");
    expect(fatal(editor.markdown_utf8() == edited));
    expect(fatal(editor.selection() == after_selection));
    expect(fatal(prefix_snapshot(editor.document()) == after_prefixes));
    Editor reloaded(edited);
    expect(fatal(reloaded.markdown_utf8() == edited));
};

}; // suite lazy_container_source_history_tests
