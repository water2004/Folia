#include <string>
#include <string_view>
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

namespace {

std::vector<BlockNode const*> lists_in(BlockNode const& root) {
    std::vector<BlockNode const*> lists;
    walk_blocks(root, [&](BlockNode const& block) {
        if (block.kind == BlockKind::List || block.kind == BlockKind::TaskList) {
            lists.push_back(&block);
        }
    });
    return lists;
}

} // namespace

suite editor_list_tightness_tests = [] {

"empty_middle_item_split_preserves_looseness_and_undo_payload"_test = [] {
    constexpr std::string_view markdown = "- first\n\n- \n\n- last";
    Editor editor{std::string(markdown)};
    auto lists = lists_in(editor.document().root);
    expect(fatal(bool(lists.size() == 1u))) << "one source list";
    if (lists.size() != 1) return;
    const auto* list = lists.front();
    expect(fatal(bool(!list->list_special().tight))) << "source list is loose";
    expect(fatal(bool(list->children.size() == 3u))) << "source list keeps empty middle item";
    if (list->children.size() != 3) return;

    const BlockNode* empty_paragraph = nullptr;
    for (const auto& child : list->children[1].children) {
        if (child.kind == BlockKind::Paragraph && child.inline_content.source.empty()) {
            empty_paragraph = &child;
            break;
        }
    }
    expect(fatal(bool(empty_paragraph != nullptr))) << "middle item owns editable empty paragraph";
    if (!empty_paragraph) return;

    const auto before_markdown = editor.markdown_utf8();
    const auto before_selection = TextSelection::caret(
        {empty_paragraph->id, 0, TextAffinity::Downstream});
    editor.set_selection(before_selection);
    auto transaction = editor.execute_document_enter(editor.selection());
    expect(fatal(bool(transaction.has_value()))) << "split empty middle item";
    if (!transaction) return;

    lists = lists_in(editor.document().root);
    expect(fatal(bool(lists.size() == 2u))) << "empty-item exit splits surrounding lists";
    for (const auto* split_list : lists) {
        expect(fatal(bool(!split_list->list_special().tight))) << "split list inherits loose syntax";
    }
    const auto after_markdown = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(fatal(bool(editor.undo()))) << "undo empty-item split";
    expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << "undo restores exact source";
    expect(fatal(bool(editor.selection() == before_selection))) << "undo restores source selection";
    lists = lists_in(editor.document().root);
    expect(fatal(bool(lists.size() == 1u))) << "undo restores one list";
    if (lists.size() == 1) {
        expect(fatal(bool(!lists.front()->list_special().tight))) << "undo restores loose payload";
    }
    expect(fatal(bool(editor.redo()))) << "redo empty-item split";
    expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << "redo restores split source";
    expect(fatal(bool(editor.selection() == after_selection))) << "redo restores split selection";
};

}; // suite editor_list_tightness_tests
