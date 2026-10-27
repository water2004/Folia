#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

void collect_paragraphs(const BlockVec& blocks, std::vector<const BlockNode*>& out) {
    for (const auto& block : blocks) {
        if (block.kind == BlockKind::Paragraph) out.push_back(&block);
        collect_paragraphs(block.children, out);
    }
}

bool has_emphasis(const InlineCstNodes& nodes) {
    for (const auto& node : nodes) {
        if (node.kind == InlineCstKind::Emphasis || node.kind == InlineCstKind::Strong) return true;
        if (has_emphasis(node.children)) return true;
    }
    return false;
}

void collect_html_owned_source(const BlockVec& blocks, std::vector<std::u32string>& out) {
    for (const auto& block : blocks) {
        if (block.inline_content.syntax_mode == InlineSyntaxMode::HtmlText) {
            out.push_back(block.inline_content.source);
        }
        if (block.has_html_source()) out.push_back(block.html_special().source);
        if (block.kind == BlockKind::UnsupportedMarkup
            && (block.atomic_special().unsup_reason == UnsupportedMarkupReason::UnsafeHtml
                || block.atomic_special().unsup_reason == UnsupportedMarkupReason::MalformedSyntax)) {
            out.push_back(utf8_to_cps(block.atomic_special().raw));
        }
        collect_html_owned_source(block.children, out);
    }
}

bool inline_source_is_lossless(const BlockNode& block) {
    const auto& document = block.inline_content;
    return tokens_partition_source(document.tree, document.source.size())
        && roots_partition_source(document.tree, document.source.size())
        && flatten_tokens(document.tree, document.source) == document.source
        && serialize_lossless(document.tree, document.source) == document.source;
}

const BlockNode* paragraph_with_source(const BlockVec& blocks, std::u32string_view source) {
    std::vector<const BlockNode*> paragraphs;
    collect_paragraphs(blocks, paragraphs);
    const auto found = std::ranges::find_if(paragraphs, [source](const BlockNode* block) {
        return block->inline_content.source == source;
    });
    return found == paragraphs.end() ? nullptr : *found;
}

} // namespace

suite commonmark_html_block_boundary_regressions = [] {
    "type1_through_type5_terminators_consume_the_whole_closing_line"_test = [] {
        const std::vector<std::pair<std::string, std::string>> cases{
            {"<!-- body -->tail\n\n*after*", "-->tail"},
            {"<script>\nbody\n</ScRiPt> tail\n\n*after*", "</ScRiPt> tail"},
            {"<?php body ?> tail\n\n*after*", "?> tail"},
            {"<!DOCTYPE html> tail\n\n*after*", "<!DOCTYPE html> tail"},
            {"<![CDATA[body]]> tail\n\n*after*", "]]" "> tail"},
        };
        for (const auto& [source, expected_tail] : cases) {
            const auto parsed = parse_text(1, source);
            std::vector<std::u32string> html_source;
            collect_html_owned_source(parsed.document.root.children, html_source);
            expect(fatal(std::ranges::any_of(html_source, [&expected_tail](std::u32string_view owned) {
                return owned.find(utf8_to_cps(expected_tail)) != std::u32string_view::npos;
            })));
            const auto* after = paragraph_with_source(parsed.document.root.children, U"*after*");
            expect(fatal(after != nullptr));
            if (after) {
                expect(has_emphasis(after->inline_content.tree.nodes));
                expect(after->inline_content.syntax_mode == InlineSyntaxMode::Markdown);
                expect(inline_source_is_lossless(*after));
            }
            expect(serialize_markdown(parsed.document) == source);
        }
    };

    "type6_block_tag_stops_before_blank_line_and_markdown_resumes"_test = [] {
        const std::vector<std::string> sources{
            "<div>\ninside\n</div>\n\n*after*",
            "  <div>\n    indented body\n\n*after*",
            "<div class\nfoo\n\n*after*",
            "</div>\nbody\n\n*after*",
            "<table>\n<tr><td>cell\n\n*after*",
        };
        for (const auto& source : sources) {
            const auto parsed = parse_text(1, source);
            const auto* after = paragraph_with_source(parsed.document.root.children, U"*after*");
            expect(fatal(after != nullptr));
            if (after) {
                expect(has_emphasis(after->inline_content.tree.nodes));
                expect(after->inline_content.syntax_mode == InlineSyntaxMode::Markdown);
                expect(inline_source_is_lossless(*after));
            }
            expect(serialize_markdown(parsed.document) == source);
        }
    };

    "reference_definitions_inside_raw_html_do_not_escape_its_block"_test = [] {
        const std::string source = "<script>\n[ref]: /inside\n</script>\n\n[ref]";
        const auto parsed = parse_text(1, source);
        const auto* unresolved = paragraph_with_source(parsed.document.root.children, U"[ref]");
        expect(fatal(unresolved != nullptr));
        if (unresolved) {
            expect(unresolved->inline_content.syntax_mode == InlineSyntaxMode::Markdown);
            expect(inline_source_is_lossless(*unresolved));
            expect(!has_emphasis(unresolved->inline_content.tree.nodes));
            expect(std::ranges::none_of(unresolved->inline_content.tree.nodes, [](const InlineCstNode& node) {
                return node.kind == InlineCstKind::Link;
            }));
        }
        expect(serialize_markdown(parsed.document) == source);
    };

    "type7_tags_do_not_interrupt_open_paragraph"_test = [] {
        for (const auto& source : {
                 std::string{"lead\n<a>\n*inside*"},
                 std::string{"lead\n<divx>\n*inside*"},
             }) {
            const auto parsed = parse_text(1, source);
            std::vector<const BlockNode*> paragraphs;
            collect_paragraphs(parsed.document.root.children, paragraphs);
            expect(fatal(paragraphs.size() == 1u));
            if (paragraphs.size() == 1u) {
                expect(paragraphs.front()->inline_content.source == utf8_to_cps(source));
                expect(paragraphs.front()->inline_content.syntax_mode == InlineSyntaxMode::Markdown);
            }
        }
    };

    "malformed_type6_block_tag_prefix_does_interrupt_open_paragraph"_test = [] {
        const std::string source = "lead\n<div *???-&&&-<---\n*inside*";
        const auto parsed = parse_text(1, source);
        std::vector<const BlockNode*> paragraphs;
        collect_paragraphs(parsed.document.root.children, paragraphs);
        expect(fatal(paragraphs.size() == 1u));
        if (paragraphs.size() == 1u) {
            expect(paragraphs.front()->inline_content.source == U"lead");
            expect(paragraphs.front()->inline_content.syntax_mode == InlineSyntaxMode::Markdown);
        }
        expect(parsed.document.root.children.size() == 2u);
        expect(serialize_markdown(parsed.document) == source);
    };

    "markdown_after_html_boundary_keeps_local_source_edit_save_reload_and_undo"_test = [] {
        const std::string source = "<script>\nbody\n</script> tail\n\n*after*";
        Editor editor{source};
        const auto* paragraph = paragraph_with_source(editor.document().root.children, U"*after*");
        expect(fatal(paragraph != nullptr));
        if (!paragraph) return;
        const auto owner = paragraph->id;
        const auto inline_source = paragraph->inline_content.source;
        const auto edit_offset = inline_source.size();
        const TextPosition caret{owner, edit_offset, TextAffinity::Downstream};
        editor.set_selection(TextSelection{caret, caret});
        expect(inline_source_is_lossless(*paragraph));
        expect(fatal(editor.execute_document_insert_text(editor.selection(), U"!").has_value()));
        const auto edited = editor.markdown_utf8();
        expect(edited == "<script>\nbody\n</script> tail\n\n*after*!");
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == source);
        Editor reloaded{edited};
        expect(reloaded.markdown_utf8() == edited);
        const auto* restored = paragraph_with_source(reloaded.document().root.children, U"*after*!");
        expect(fatal(restored != nullptr));
        if (restored) {
            expect(has_emphasis(restored->inline_content.tree.nodes));
            expect(inline_source_is_lossless(*restored));
        }
    };
};
