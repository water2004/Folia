#include <algorithm>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.selection;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

const BlockNode* type7_paragraph(const BlockVec& blocks) {
    for (const auto& block : blocks) {
        if (block.kind == BlockKind::Paragraph
            && block.inline_content.syntax_mode == InlineSyntaxMode::HtmlText) {
            return &block;
        }
        if (const auto* nested = type7_paragraph(block.children)) return nested;
    }
    return nullptr;
}

bool inline_source_is_lossless(const BlockNode& block) {
    const auto& document = block.inline_content;
    return tokens_partition_source(document.tree, document.source.size())
        && roots_partition_source(document.tree, document.source.size())
        && flatten_tokens(document.tree, document.source) == document.source
        && serialize_lossless(document.tree, document.source) == document.source;
}

void expect_no_global_rebuild(std::string_view operation) {
    const auto counters = read_core_operation_counters();
    expect(fatal(counters.full_document_parses == 0u)) << operation << " parsed whole document";
    expect(fatal(counters.full_document_serializations == 0u))
        << operation << " serialized whole document";
    expect(fatal(counters.full_tree_transaction_diffs == 0u))
        << operation << " diffed the full tree";
}

TextSelection caret_at(const BlockNode& block, std::size_t offset) {
    const TextPosition position{block.id, offset, TextAffinity::Downstream};
    return TextSelection{position, position};
}

void collect_inline_presentation(
    const std::vector<InlineRenderItem>& items,
    std::string_view href,
    bool& has_link,
    bool& has_bold_visible) {
    for (const auto& item : items) {
        if (item.kind == InlineRenderItem::Kind::Link
            && item.special().semantic().href == href) {
            has_link = true;
        }
        if (item.kind == InlineRenderItem::Kind::Text
            && item.style.bold
            && item.text.find(U"visible") != std::u32string::npos) {
            has_bold_visible = true;
        }
        if (item.payload) {
            collect_inline_presentation(
                item.special().semantic().children,
                href,
                has_link,
                has_bold_visible);
        }
    }
}

const RenderBlock* render_block_for(const RenderModel& model, NodeId id) {
    const auto find = [&](auto&& self, const RenderBlock& block) -> const RenderBlock* {
        if (block.id == id) return &block;
        for (const auto& child : block.child_blocks) {
            if (const auto* found = self(self, child)) return found;
        }
        return nullptr;
    };
    for (const auto& block : model.blocks) {
        if (const auto* found = find(find, block)) return found;
    }
    return nullptr;
}

constexpr std::string_view kHtmlType7Source =
    "<a href='https://example.test'>\n"
    "<strong>visible</strong>\n"
    "</a>\n";
constexpr std::string_view kHtmlType7Body =
    "<a href='https://example.test'>\n"
    "<strong>visible</strong>\n"
    "</a>";

} // namespace

suite html_block_editing_tests = [] {

"adjacent_html_roots_keep_exact_source_and_local_edit_history"_test = [] {
    const std::string source = "<p align='center'>one</p>\n<p align='right'>two</p>";
    Editor editor{source};
    expect(fatal(editor.document().root.children.size() == 1u));
    const auto& container = editor.document().root.children.front();
    expect(fatal(container.kind == BlockKind::HtmlContainer));
    expect(fatal(container.children.size() == 2u));
    const auto owner = container.children[1].id;
    const auto initial = caret_at(container.children[1], 1u);
    editor.set_selection(initial);

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(initial, U"!").has_value()));
    expect_no_global_rebuild("adjacent HTML source edit");
    expect(editor.markdown_utf8() == "<p align='center'>one</p>\n<p align='right'>t!wo</p>");
    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_no_global_rebuild("adjacent HTML source edit undo");
    expect(editor.markdown_utf8() == source);
    expect(editor.selection() == initial);

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_enter(initial).has_value()));
    expect_no_global_rebuild("adjacent HTML Enter");
    const auto split = editor.markdown_utf8();
    expect(split.starts_with("<p align='center'>one</p>"));
    expect(split.find("<div") == std::string::npos);
    expect(split.find("</div>") == std::string::npos);
    const auto* left = find_document_block(editor.document(), owner);
    expect(fatal(left != nullptr));
    if (left) expect(left->inline_content.source == U"t");
    const auto split_selection = editor.selection();
    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_no_global_rebuild("adjacent HTML Enter undo");
    expect(editor.markdown_utf8() == source);
    expect(editor.selection() == initial);
    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_no_global_rebuild("adjacent HTML Enter redo");
    expect(editor.markdown_utf8() == split);
    expect(editor.selection() == split_selection);
};

"type7_block_is_one_paragraph_whose_html_text_source_is_lossless"_test = [] {
    const auto parsed = parse_text(1, std::string(kHtmlType7Source));
    expect(fatal(parsed.document.root.children.size() == 1u));
    if (parsed.document.root.children.size() != 1u) return;
    const auto& paragraph = parsed.document.root.children.front();
    expect(paragraph.kind == BlockKind::Paragraph);
    expect(paragraph.inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
    expect(paragraph.inline_content.source == utf8_to_cps(kHtmlType7Body));
    expect(parsed.document.trailing_line_ending == U"\n");
    expect(inline_source_is_lossless(paragraph));
    expect(serialize_markdown(parsed.document) == kHtmlType7Source);
};

"type7_source_renderer_keeps_anchor_href_and_nested_inline_style"_test = [] {
    auto parsed = parse_text(1, std::string(kHtmlType7Source));
    const auto* paragraph = type7_paragraph(parsed.document.root.children);
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;

    const auto model = build_render_model(
        parsed.document,
        parsed.outline,
        parsed.symbols,
        default_theme_profile());
    const auto* rendered = render_block_for(model, paragraph->id);
    expect(fatal(rendered != nullptr));
    if (!rendered) return;
    bool has_link = false;
    bool has_bold_visible = false;
    collect_inline_presentation(
        rendered->inline_items,
        "https://example.test",
        has_link,
        has_bold_visible);
    expect(has_link);
    expect(has_bold_visible);
};

"type7_paragraph_insert_delete_undo_redo_use_source_transactions"_test = [] {
    Editor editor{std::string(kHtmlType7Source)};
    expect(fatal(editor.document().root.children.size() == 1u));
    if (editor.document().root.children.size() != 1u) return;
    const auto& initial = editor.document().root.children.front();
    expect(initial.kind == BlockKind::Paragraph);
    expect(initial.inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
    expect(initial.inline_content.source == utf8_to_cps(kHtmlType7Body));
    const auto owner_id = initial.id;
    const auto before_source = initial.inline_content.source;
    const auto visible = before_source.find(U"visible");
    expect(visible != std::u32string::npos);
    if (visible == std::u32string::npos) return;
    const auto caret = caret_at(initial, visible + std::u32string_view(U"visible").size());
    auto expected_inserted_source = before_source;
    expected_inserted_source.insert(caret.active.source_offset, U"!");
    editor.set_selection(caret);

    reset_core_operation_counters();
    const auto insertion = editor.execute_document_insert_text(editor.selection(), U"!");
    expect(fatal(insertion.has_value()));
    if (!insertion) return;
    expect_no_global_rebuild("type7 insert");
    const auto inserted_selection = editor.selection();
    const auto* inserted = find_document_block(editor.document(), owner_id);
    expect(fatal(inserted != nullptr));
    if (!inserted) return;
    expect(inserted->kind == BlockKind::Paragraph);
    expect(inserted->inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
    expect(inserted->inline_content.source == expected_inserted_source);
    expect(inline_source_is_lossless(*inserted));
    expect(inserted_selection.active.container_id == owner_id);
    expect(inserted_selection.active.source_offset == caret.active.source_offset + 1u);
    const auto inserted_markdown = editor.markdown_utf8();
    expect(inserted_markdown == "<a href='https://example.test'>\n<strong>visible!</strong>\n</a>\n");

    reset_core_operation_counters();
    const auto deletion = editor.execute_document_delete_backward(editor.selection());
    expect(fatal(deletion.has_value()));
    if (!deletion) return;
    expect_no_global_rebuild("type7 delete");
    const auto deleted_selection = editor.selection();
    const auto* deleted = find_document_block(editor.document(), owner_id);
    expect(fatal(deleted != nullptr));
    if (!deleted) return;
    expect(deleted->inline_content.source == before_source);
    expect(inline_source_is_lossless(*deleted));
    expect(deleted_selection == caret);
    expect(editor.markdown_utf8() == kHtmlType7Source);

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_no_global_rebuild("type7 delete undo");
    expect(editor.selection() == inserted_selection);
    const auto* undo_delete = find_document_block(editor.document(), owner_id);
    expect(fatal(undo_delete != nullptr));
    if (undo_delete) {
        expect(undo_delete->inline_content.source == expected_inserted_source);
        expect(inline_source_is_lossless(*undo_delete));
    }
    expect(editor.markdown_utf8() == inserted_markdown);

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_no_global_rebuild("type7 insert undo");
    expect(editor.selection() == caret);
    const auto* undo_insert = find_document_block(editor.document(), owner_id);
    expect(fatal(undo_insert != nullptr));
    if (undo_insert) {
        expect(undo_insert->inline_content.source == before_source);
        expect(inline_source_is_lossless(*undo_insert));
    }
    expect(editor.markdown_utf8() == kHtmlType7Source);

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_no_global_rebuild("type7 insert redo");
    expect(editor.selection() == inserted_selection);
    expect(editor.markdown_utf8() == inserted_markdown);

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_no_global_rebuild("type7 delete redo");
    expect(editor.selection() == caret);
    const auto* redo_delete = find_document_block(editor.document(), owner_id);
    expect(fatal(redo_delete != nullptr));
    if (redo_delete) {
        expect(redo_delete->inline_content.source == before_source);
        expect(inline_source_is_lossless(*redo_delete));
    }
    expect(editor.markdown_utf8() == kHtmlType7Source);
};

"enter_splits_type7_raw_source_and_reload_keeps_both_blocks_exact"_test = [] {
    Editor editor{std::string(kHtmlType7Source)};
    expect(fatal(editor.document().root.children.size() == 1u));
    if (editor.document().root.children.size() != 1u) return;
    const auto& paragraph = editor.document().root.children.front();
    expect(paragraph.kind == BlockKind::Paragraph);
    expect(paragraph.inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
    const auto owner_id = paragraph.id;
    const auto raw_body = paragraph.inline_content.source;
    expect(raw_body == utf8_to_cps(kHtmlType7Body));
    const auto visible = raw_body.find(U"visible");
    expect(visible != std::u32string::npos);
    if (visible == std::u32string::npos) return;
    const auto split_offset = visible + 3u;
    const auto before_selection = caret_at(paragraph, split_offset);
    editor.set_selection(before_selection);
    const auto original_markdown = editor.markdown_utf8();
    expect(original_markdown == kHtmlType7Source);

    reset_core_operation_counters();
    const auto split = editor.execute_document_enter(editor.selection());
    expect(fatal(split.has_value()));
    if (!split) return;
    expect_no_global_rebuild("type7 Enter split");
    expect(fatal(editor.document().root.children.size() == 2u));
    if (editor.document().root.children.size() != 2u) return;
    const auto& left = editor.document().root.children[0];
    const auto& right = editor.document().root.children[1];
    expect(left.kind == BlockKind::Paragraph);
    expect(right.kind == BlockKind::Paragraph);
    expect(left.inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
    expect(right.inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
    expect(left.inline_content.source == raw_body.substr(0, split_offset));
    expect(right.inline_content.source == raw_body.substr(split_offset));
    expect(inline_source_is_lossless(left));
    expect(inline_source_is_lossless(right));
    const auto after_selection = editor.selection();
    expect(after_selection.active.container_id == right.id);
    expect(after_selection.active.source_offset == 0u);

    const auto split_markdown = editor.markdown_utf8();
    const auto expected_split_markdown =
        raw_body.substr(0, split_offset) + U"\n\n" + raw_body.substr(split_offset) + U"\n";
    expect(utf8_to_cps(split_markdown) == expected_split_markdown);

    // A normal full parse is expected at this explicit save/reload boundary.
    Editor reloaded(split_markdown);
    expect(fatal(reloaded.document().root.children.size() == 2u));
    if (reloaded.document().root.children.size() == 2u) {
        for (const auto& block : reloaded.document().root.children) {
            expect(block.kind == BlockKind::Paragraph);
            expect(inline_source_is_lossless(block));
        }
    }
    expect(reloaded.markdown_utf8() == split_markdown);

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_no_global_rebuild("type7 Enter undo");
    expect(fatal(editor.document().root.children.size() == 1u));
    if (editor.document().root.children.size() == 1u) {
        const auto& restored = editor.document().root.children.front();
        expect(restored.id == owner_id);
        expect(restored.inline_content.source == raw_body);
        expect(inline_source_is_lossless(restored));
    }
    expect(editor.selection() == before_selection);
    expect(editor.markdown_utf8() == original_markdown);

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_no_global_rebuild("type7 Enter redo");
    expect(fatal(editor.document().root.children.size() == 2u));
    if (editor.document().root.children.size() == 2u) {
        expect(editor.document().root.children[0].inline_content.source
            == raw_body.substr(0, split_offset));
        expect(editor.document().root.children[1].inline_content.source
            == raw_body.substr(split_offset));
    }
    expect(editor.selection() == after_selection);
    expect(editor.markdown_utf8() == split_markdown);
};

};
