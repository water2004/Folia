#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_history;
import folia.markdown.document_symbols;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.core.theme;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/document_edit_test_support.hpp"

namespace {

void collect_paragraphs(const BlockVec& blocks, std::vector<const BlockNode*>& output) {
    for (const auto& block : blocks) {
        if (block.kind == BlockKind::Paragraph) output.push_back(&block);
        collect_paragraphs(block.children, output);
    }
}

const BlockNode* paragraph_with_source(const BlockVec& blocks, std::u32string_view source) {
    std::vector<const BlockNode*> paragraphs;
    collect_paragraphs(blocks, paragraphs);
    const auto found = std::find_if(paragraphs.begin(), paragraphs.end(), [&](const auto* block) {
        return block->inline_content.source == source;
    });
    return found == paragraphs.end() ? nullptr : *found;
}

void expect_inline_source_is_lossless(const BlockNode& paragraph) {
    const auto& document = paragraph.inline_content;
    expect(tokens_partition_source(document.tree, document.source.size()));
    expect(roots_partition_source(document.tree, document.source.size()));
    expect(flatten_tokens(document.tree, document.source) == document.source);
    expect(serialize_lossless(document.tree, document.source) == document.source);
}

} // namespace

suite blank_line_source_tests = [] {

"commonmark_227_preserves_whitespace_paragraph_sources_and_exact_save"_test = [] {
    constexpr std::string_view source = "  \n\naaa\n  \n\n# aaa\n\n  \n";
    auto parsed = parse_text(1, std::string(source));
    expect(serialize_markdown(parsed.document) == source);
    const auto saved = serialize_markdown(parsed.document);
    auto reloaded = parse_text(2, saved);
    expect(serialize_markdown(reloaded.document) == source);

    std::vector<const BlockNode*> paragraphs;
    collect_paragraphs(parsed.document.root.children, paragraphs);
    expect(paragraphs.size() >= 3u);
    const auto* two_spaces = paragraph_with_source(parsed.document.root.children, U"  ");
    expect(two_spaces != nullptr);
    if (two_spaces) expect_inline_source_is_lossless(*two_spaces);

    const auto* terminal_spaces = paragraphs.empty() ? nullptr : paragraphs.back();
    expect(terminal_spaces != nullptr);
    if (terminal_spaces) {
        expect(terminal_spaces->inline_content.source == U"  ");
        expect_inline_source_is_lossless(*terminal_spaces);
    }
};

"ordinary_empty_blank_paragraphs_keep_empty_inline_source"_test = [] {
    auto parsed = parse_text(1, "\ntext");
    const auto* empty = paragraph_with_source(parsed.document.root.children, U"");
    expect(empty != nullptr);
    if (empty) {
        expect(empty->inline_content.source.empty());
        expect_inline_source_is_lossless(*empty);
    }
    expect(serialize_markdown(parsed.document) == "\ntext");
};

"editable_blank_lines_keep_spaces_tabs_and_eol_out_of_inline_source"_test = [] {
    struct Case { std::string source; std::u32string blank; };
    const std::vector<Case> cases{
        {" \t\n\nbody", U" \t"},
        {"body\r\n\r\n \t\r\n\r\nend", U" \t"},
        {"body\n\n\t  \n\n", U"\t  "},
    };

    for (const auto& test_case : cases) {
        auto parsed = parse_text(1, test_case.source);
        const auto* paragraph = paragraph_with_source(
            parsed.document.root.children, test_case.blank);
        expect(paragraph != nullptr) << test_case.source;
        if (!paragraph) continue;
        expect_inline_source_is_lossless(*paragraph);
        expect(serialize_markdown(parsed.document) == test_case.source)
            << "actual=" << serialize_markdown(parsed.document)
            << "; expected=" << test_case.source;
    }
};

"blank_paragraphs_inside_quote_and_list_own_their_raw_whitespace"_test = [] {
    const std::vector<std::pair<std::string, std::u32string>> cases{
        {"> first\n> \t\n> \t\n> second\n", U"\t"},
        {"- first\n  \t\n  \t\n  second\n- next\n", U"\t"},
    };
    for (const auto& [source, expected_blank] : cases) {
        auto parsed = parse_text(1, source);
        const auto* paragraph = paragraph_with_source(parsed.document.root.children, expected_blank);
        expect(paragraph != nullptr) << source;
        if (paragraph) expect_inline_source_is_lossless(*paragraph);
    }
};

"only_all_space_or_tab_paragraphs_are_hidden_as_one_editable_marker"_test = [] {
    const auto parsed = parse_text(1, " \t\n\nbody");
    const auto* whitespace = paragraph_with_source(parsed.document.root.children, U" \t");
    expect(whitespace != nullptr);
    if (!whitespace) return;
    const auto ranges = inline_block_semantic_whitespace_ranges(
        whitespace->inline_content, InlineBlockWhitespacePolicy::Paragraph);
    expect(ranges.size() == 1u);
    if (ranges.size() == 1u) {
        expect(ranges.front().start == 0u);
        expect(ranges.front().end == 2u);
    }

    const std::vector<std::u32string_view> nonblank_sources{
        U"&amp;", U"\\ ", U"`  `", U"  &amp;"};
    for (const auto source : nonblank_sources) {
        auto document = parse_text(2, cps_to_utf8(source)).document;
        const auto* paragraph = paragraph_with_source(document.root.children, source);
        expect(paragraph != nullptr);
        if (!paragraph) continue;
        const auto semantic = inline_block_semantic_whitespace_ranges(
            paragraph->inline_content, InlineBlockWhitespacePolicy::Paragraph);
        const auto raw_prefix_only = source == U"  &amp;";
        expect(semantic.size() == (raw_prefix_only ? 1u : 0u)) << cps_to_utf8(source);
        if (raw_prefix_only && semantic.size() == 1u) {
            expect(semantic.front().start == 0u);
            expect(semantic.front().end == 2u);
        }
    }
};

"renderer_keeps_the_blank_source_span_as_hidden_but_editable"_test = [] {
    constexpr std::string_view source = " \t\n\nbody";
    auto parsed = parse_text(1, std::string(source));
    const auto* paragraph = paragraph_with_source(parsed.document.root.children, U" \t");
    expect(paragraph != nullptr);
    if (!paragraph) return;
    auto model = build_render_model(
        parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
    const auto render = std::find_if(model.blocks.begin(), model.blocks.end(), [&](const auto& block) {
        return block.id == paragraph->id;
    });
    expect(render != model.blocks.end());
    if (render == model.blocks.end()) return;
    const auto marker = std::find_if(render->inline_items.begin(), render->inline_items.end(), [](const auto& item) {
        return item.kind == InlineRenderItem::Kind::Marker
            && item.special().visibility == MarkerVisibility::HiddenButEditable;
    });
    expect(marker != render->inline_items.end());
    if (marker != render->inline_items.end()) {
        expect(marker->text == U" \t");
        expect(marker->source_span.container_id == paragraph->id);
        expect(marker->source_span.source_range.start == 0u);
        expect(marker->source_span.source_range.end == 2u);
    }
};

"editing_blank_whitespace_undo_redo_restores_source_and_selection"_test = [] {
    constexpr std::string_view source = " \t\n\nbody";
    auto document = parse_document(std::string(source));
    const auto* found = paragraph_with_source(document.root.children, U" \t");
    expect(found != nullptr);
    if (!found) return;
    const auto paragraph_id = found->id;
    const auto before = serialize_markdown(document);
    auto selection = range(*found, 0, 2);
    auto transaction = folia::document_insert_text(document, selection, U"\t  ");
    expect(transaction.has_value());
    if (!transaction) return;
    selection = transaction->selection_after;
    const auto* edited = find_document_block(document, paragraph_id);
    expect(edited != nullptr);
    if (edited) {
        expect(edited->inline_content.source == U"\t  ");
        expect_inline_source_is_lossless(*edited);
    }

    DocumentHistory history;
    history.push(*transaction);
    expect(history.undo(document, selection));
    expect(serialize_markdown(document) == before);
    expect(selection == transaction->selection_before);
    const auto* undone = find_document_block(document, paragraph_id);
    expect(undone != nullptr);
    if (undone) expect(undone->inline_content.source == U" \t");

    expect(history.redo(document, selection));
    expect(serialize_markdown(document) == "\t  \n\nbody");
    expect(selection == transaction->selection_after);
    const auto* redone = find_document_block(document, paragraph_id);
    expect(redone != nullptr);
    if (redone) expect(redone->inline_content.source == U"\t  ");
};

};
