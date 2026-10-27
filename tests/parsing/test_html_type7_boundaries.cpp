#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

const BlockFragmentSourceRange* source_range_for(
    const std::vector<BlockFragmentSourceRange>& ranges,
    NodeId node_id) {
    const auto found = std::ranges::find_if(ranges, [node_id](const auto& range) {
        return range.node_id == node_id;
    });
    return found == ranges.end() ? nullptr : &*found;
}

const InlineCstNode* first_html_tag(
    const InlineCstNodes& nodes,
    std::string_view name) {
    for (const auto& node : nodes) {
        if (node.kind == InlineCstKind::HtmlElement
            && node.semantics().html_tag == name) {
            return &node;
        }
        if (const auto* nested = first_html_tag(node.children, name)) return nested;
    }
    return nullptr;
}

std::size_t count_html_tag(const InlineCstNodes& nodes, std::string_view name) {
    std::size_t count = 0;
    for (const auto& node : nodes) {
        if (node.kind == InlineCstKind::HtmlElement
            && node.semantics().html_tag == name) {
            ++count;
        }
        count += count_html_tag(node.children, name);
    }
    return count;
}

void expect_lossless(const InlineDocument& document) {
    expect(fatal(bool(tokens_partition_source(document.tree, document.source.size()))));
    expect(fatal(bool(roots_partition_source(document.tree, document.source.size()))));
    expect(fatal(bool(flatten_tokens(document.tree, document.source) == document.source)));
    expect(fatal(bool(serialize_lossless(document.tree, document.source) == document.source)));
}

void expect_type7_paragraph(const BlockNode& block, std::u32string_view content) {
    expect(block.kind == BlockKind::Paragraph);
    expect(block.inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
    expect(block.inline_content.source == content);
    expect(!block.has_html_source());
    expect(!block.has_html_element_provenance());
    expect_lossless(block.inline_content);
}

} // namespace

suite commonmark_html_type7_boundary_tests = [] {
    "type7_paragraph_owns_raw_source_and_separates_physical_eol_mapping"_test = [] {
        const std::u32string source = U"<a>\ninside\n</a>\n\noutside";
        const std::u32string inline_source = U"<a>\ninside\n</a>";
        const auto block_end = inline_source.size() + 1; // Include the closing line's LF.
        const auto parsed = parse_block_fragment(source);

        expect(fatal(parsed.blocks.size() == 2u));
        if (parsed.blocks.size() != 2u) return;
        const auto& html_paragraph = parsed.blocks.front();
        expect_type7_paragraph(html_paragraph, inline_source);
        expect(parsed.blocks.back().kind == BlockKind::Paragraph);
        expect(parsed.blocks.back().inline_content.syntax_mode == InlineSyntaxMode::Markdown);
        expect(parsed.blocks.back().inline_content.source == U"outside");

        const auto* range = source_range_for(parsed.source_ranges, html_paragraph.id);
        expect(fatal(range != nullptr));
        if (range) {
            expect(range->source_range.start == 0u);
            expect(range->source_range.end == block_end);
            expect(range->content_range.start == 0u);
            expect(range->content_range.end == inline_source.size());
        }

        const auto saved = parse_text(1, cps_to_utf8(source));
        expect(serialize_markdown(saved.document) == cps_to_utf8(source));
    };

    "type7_inline_source_excludes_only_terminal_lf_cr_or_crlf"_test = [] {
        auto checks_terminal_eol = [](const std::u32string& physical_source,
            const std::u32string& inline_source) {
            const auto parsed = parse_block_fragment(physical_source);
            expect(fatal(parsed.blocks.size() == 1u));
            if (parsed.blocks.size() != 1u) return;

            const auto& block = parsed.blocks.front();
            expect_type7_paragraph(block, inline_source);
            const auto* range = source_range_for(parsed.source_ranges, block.id);
            expect(fatal(range != nullptr));
            if (range) {
                expect(range->source_range.start == 0u);
                expect(range->source_range.end == physical_source.size());
                expect(range->content_range.start == 0u);
                expect(range->content_range.end == inline_source.size());
            }
            const auto saved = parse_text(1, cps_to_utf8(physical_source));
            expect(serialize_markdown(saved.document) == cps_to_utf8(physical_source));
        };

        checks_terminal_eol(U"<a>\nbody\n</a>\n", U"<a>\nbody\n</a>");
        checks_terminal_eol(U"<a>\rbody\r</a>\r", U"<a>\rbody\r</a>");
        checks_terminal_eol(U"<a>\r\nbody\r\n</a>\r\n", U"<a>\r\nbody\r\n</a>");
    };

    "type7_htmltext_cst_preserves_single_root_link_and_nested_style_tags"_test = [] {
        const std::u32string source =
            U"<a href='https://example.test' title='tip'>\n"
            U"<strong>linked</strong>\n"
            U"</a>";
        const auto parsed = parse_block_fragment(source);
        expect(fatal(parsed.blocks.size() == 1u));
        if (parsed.blocks.size() != 1u) return;

        const auto& block = parsed.blocks.front();
        expect_type7_paragraph(block, source);
        const auto* link = first_html_tag(block.inline_content.tree.nodes, "a");
        const auto* strong = first_html_tag(block.inline_content.tree.nodes, "strong");
        expect(fatal(link != nullptr));
        expect(fatal(strong != nullptr));
        if (link) {
            expect(link->semantics().href == "https://example.test");
            expect(link->semantics().title == std::optional<std::string>{"tip"});
            expect(link->range.start == 0u);
            expect(link->range.end == source.size());
        }
        if (strong) expect(strong->semantics().html_tag == "strong");
    };

    "type7_htmltext_cst_keeps_multiple_roots_and_markdown_markers_literal"_test = [] {
        const std::u32string source =
            U"<a href='/one'>\none\n</a>\n"
            U"<em>\n*two*\n</em>";
        const auto parsed = parse_block_fragment(source);
        expect(fatal(parsed.blocks.size() == 1u));
        if (parsed.blocks.size() != 1u) return;

        const auto& block = parsed.blocks.front();
        expect_type7_paragraph(block, source);
        expect(count_html_tag(block.inline_content.tree.nodes, "a") == 1u);
        expect(count_html_tag(block.inline_content.tree.nodes, "em") == 1u);
        const auto* link = first_html_tag(block.inline_content.tree.nodes, "a");
        expect(fatal(link != nullptr));
        if (link) expect(link->semantics().href == "/one");
        expect(!inline_contains_kind(block.inline_content, InlineCstKind::Emphasis));
        expect(!inline_contains_kind(block.inline_content, InlineCstKind::Strong));
        expect(!inline_contains_kind(block.inline_content, InlineCstKind::Link));
        expect(block.inline_content.source.find(U"*two*") != std::u32string::npos);
    };

    "type7_incomplete_inline_html_remains_an_editable_incomplete_node"_test = [] {
        const std::u32string physical_source = U"<a href='x'>\nunfinished\n";
        const std::u32string inline_source = U"<a href='x'>\nunfinished";
        const auto parsed = parse_block_fragment(physical_source);
        expect(fatal(parsed.blocks.size() == 1u));
        if (parsed.blocks.size() != 1u) return;

        const auto& block = parsed.blocks.front();
        expect_type7_paragraph(block, inline_source);
        const InlineCstNode* incomplete = nullptr;
        for (const auto& node : block.inline_content.tree.nodes) {
            if (node.kind == InlineCstKind::Incomplete) {
                incomplete = &node;
                break;
            }
        }
        expect(fatal(incomplete != nullptr));
        if (incomplete) {
            expect(incomplete->status == ParseStatus::MissingCloser);
            expect(incomplete->range.start == 0u);
            // Missing inline closers retain the existing line-local incomplete range;
            // the following body text remains a separate lossless CST span.
            expect(incomplete->range.end == inline_source.find(U'\n') + 1u);
        }

        const auto* range = source_range_for(parsed.source_ranges, block.id);
        expect(fatal(range != nullptr));
        if (range) {
            expect(range->source_range.start == 0u);
            expect(range->source_range.end == physical_source.size());
            expect(range->content_range.start == 0u);
            expect(range->content_range.end == inline_source.size());
        }
    };

    "complete_closing_html_tags_are_raw_htmltext_nodes"_test = [] {
        auto preserves_closing_tag = [](const std::u32string& physical_source,
            const std::u32string& inline_source) {
            const auto parsed = parse_block_fragment(physical_source);
            expect(fatal(parsed.blocks.size() == 1u));
            if (parsed.blocks.size() != 1u) return;

            const auto& block = parsed.blocks.front();
            expect_type7_paragraph(block, inline_source);
            expect(inline_contains_kind(block.inline_content, InlineCstKind::Raw));
            expect(!inline_contains_kind(block.inline_content, InlineCstKind::Incomplete));
            const auto* range = source_range_for(parsed.source_ranges, block.id);
            expect(fatal(range != nullptr));
            if (range) {
                expect(range->source_range.start == 0u);
                expect(range->source_range.end == physical_source.size());
                expect(range->content_range.end == inline_source.size());
            }
        };

        preserves_closing_tag(U"</a>\n", U"</a>");
        preserves_closing_tag(U"</pre>\n", U"</pre>");
    };

    "type7_does_not_interrupt_an_open_markdown_paragraph"_test = [] {
        const std::string source = "lead\n<a>\nbody\n</a>\nafter";
        const auto parsed = parse_text(1, source);
        expect(fatal(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.size() != 1u) return;

        const auto& paragraph = parsed.document.root.children.front();
        expect(paragraph.kind == BlockKind::Paragraph);
        expect(paragraph.inline_content.syntax_mode == InlineSyntaxMode::Markdown);
        expect(paragraph.inline_content.source == utf8_to_cps(source));
    };

    "link_definition_scanner_skips_type7_htmltext_body"_test = [] {
        const std::string source = "<a>\n[ref]: /inside\n</a>\n\n[ref]";
        const auto parsed = parse_text(1, source);
        expect(fatal(parsed.document.root.children.size() == 2u));
        if (parsed.document.root.children.size() != 2u) return;

        const auto& html_paragraph = parsed.document.root.children.front();
        expect(html_paragraph.kind == BlockKind::Paragraph);
        expect(html_paragraph.inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
        expect(html_paragraph.inline_content.source == U"<a>\n[ref]: /inside\n</a>");
        expect(!inline_contains_kind(html_paragraph.inline_content, InlineCstKind::Link));

        const auto& unresolved = parsed.document.root.children.back();
        expect(unresolved.kind == BlockKind::Paragraph);
        expect(unresolved.inline_content.syntax_mode == InlineSyntaxMode::Markdown);
        expect(unresolved.inline_content.source == U"[ref]");
        expect(!inline_contains_kind(unresolved.inline_content, InlineCstKind::Link));
        expect(std::ranges::none_of(parsed.document.root.children, [](const BlockNode& block) {
            return block.kind == BlockKind::LinkDefinition;
        }));
        expect(serialize_markdown(parsed.document) == source);
    };

    "existing_type1_to_type6_block_html_path_is_unchanged"_test = [] {
        const std::string source = "<div><p>body</p></div>";
        const auto parsed = parse_text(1, source);
        expect(fatal(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.size() != 1u) return;

        const auto& block = parsed.document.root.children.front();
        expect(block.kind == BlockKind::HtmlContainer);
        expect(block.has_html_source());
        expect(block.html_special().root_tag == "div");
        expect(serialize_markdown(parsed.document) == source);
    };
};
