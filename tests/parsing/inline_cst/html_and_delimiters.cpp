#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.core.inline_source_edit;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "parsing/support/inline_cst.hpp"

suite parsing_inline_cst_html_and_delimiters_tests = [] {

"semantic_children_remain_editable_inside_unclosed_delimiters"_test = [] {
    const auto source = std::u32string{U"p**<br>abcq"};
    auto document = InlineDocument{source, parse_inline(source, test_context())};
    const auto* incomplete = first_node(document.tree, InlineCstKind::Incomplete);
    const auto* hard_break = first_html_node(document.tree.nodes, "br");
    expect(fatal(bool(incomplete != nullptr)));
    expect(fatal(bool(hard_break != nullptr)));
    if (!hard_break) return;
    expect(fatal(bool(source.substr(hard_break->range.start, hard_break->range.length()) == U"<br>")));
    const auto deletion = inline_backward_delete_range(document, hard_break->range.end);
    expect(fatal(bool(deletion.has_value())));
    if (!deletion) return;
    expect(fatal(bool(*deletion == hard_break->range)));
};

"underscore delimiters respect CommonMark word boundaries"_test = [] {
    const std::vector<std::u32string> literal_sources{
        U"0x1_fffff_ffff",
        U"trig_out",
        U"a_b_c",
        U"foo_",
        U"foo__",
        U"0x1_fffff_ffff and trig_out across\nsoft lines",
    };
    for (auto const& source : literal_sources) {
        const auto tree = parse_inline(source, test_context());
        expect(first_node(tree, InlineCstKind::Emphasis) == nullptr)
            << "source=" << cps_to_utf8(source);
        expect(first_node(tree, InlineCstKind::Strong) == nullptr)
            << "source=" << cps_to_utf8(source);
        expect(is_lossless(source));
    }

    const std::vector<std::pair<std::u32string, InlineCstKind>> formatted_sources{
        {U"_word_", InlineCstKind::Emphasis},
        {U"__word__", InlineCstKind::Strong},
        {U"a _word_ b", InlineCstKind::Emphasis},
        {U"(_word_)", InlineCstKind::Emphasis},
    };
    for (auto const& [source, kind] : formatted_sources) {
        const auto tree = parse_inline(source, test_context());
        expect(first_node(tree, kind) != nullptr)
            << "source=" << cps_to_utf8(source);
        expect(is_lossless(source));
    }

    for (auto const& source : {std::u32string{U"_"}, std::u32string{U"__"}}) {
        const auto tree = parse_inline(source, test_context());
        expect(first_node(tree, InlineCstKind::Incomplete) != nullptr);
        expect(is_lossless(source));
    }
};

"html_text_mode_keeps_markdown_markers_literal_across_edits"_test = [] {
    InlineDocument document{
        U"*literal* <strong>bold</strong> $math$",
        {},
        InlineSyntaxMode::HtmlText};
    reparse_inline_document(document, test_context());
    expect(is_lossless(document.source));
    expect(!inline_contains_kind(document, InlineCstKind::Emphasis));
    expect(!inline_contains_kind(document, InlineCstKind::InlineMath));
    expect(!inline_contains_kind(document, InlineCstKind::Strong));
    expect(first_html_node(document.tree.nodes, "strong") != nullptr);

    const auto inserted = document.source.find(U"literal") + 7;
    apply_inline_source_edit(
        NodeId{7},
        document,
        TextEdit{NodeId{7}, {inserted, inserted}, U"*"},
        test_context());
    expect(is_lossless(document.source));
    expect(!inline_contains_kind(document, InlineCstKind::Emphasis));
    expect(!inline_contains_kind(document, InlineCstKind::Strong));
    expect(first_html_node(document.tree.nodes, "strong") != nullptr);
};

"inline_html_retains_recursive_cst_and_surrounding_markdown_syntax"_test = [] {
    const auto source = std::u32string{
        U"**outside** <span STYLE=\"color:#f00\" onclick=\"ignored()\">"
        U"**literal** $literal$ <strong>bold <em>*still literal*</em></strong>"
        U"</span> *outside*"};
    const auto tree = parse_inline(source, test_context());
    expect(is_lossless(source));

    const auto* span = first_html_node(tree.nodes, "span");
    expect(fatal(bool(span != nullptr)));
    if (!span) return;
    expect(contains_kind(span->children, InlineCstKind::Strong));
    expect(contains_kind(span->children, InlineCstKind::Emphasis));
    expect(contains_kind(span->children, InlineCstKind::InlineMath));
    expect(first_html_node(span->children, "strong") != nullptr);
    expect(first_html_node(span->children, "em") != nullptr);
    expect(span->semantics().html_attributes.contains("style"));
    expect(!span->semantics().html_attributes.contains("onclick"));
    expect(first_node(tree, InlineCstKind::Strong) != nullptr);
    expect(first_node(tree, InlineCstKind::Emphasis) != nullptr);
};

"html_void_elements_remain_html_cst_nodes"_test = [] {
    const auto source = std::u32string{U"a<br><img src=\"x.svg\" alt=\"logo\">b"};
    const auto tree = parse_inline(source, test_context());
    expect(is_lossless(source));
    const auto* br = first_html_node(tree.nodes, "br");
    const auto* image = first_html_node(tree.nodes, "img");
    expect(fatal(bool(br != nullptr)));
    expect(fatal(bool(image != nullptr)));
    expect(first_node(tree, InlineCstKind::HardBreak) == nullptr);
    expect(first_node(tree, InlineCstKind::Image) == nullptr);
    if (image) {
        expect(image->semantics().href == "x.svg");
        expect(image->semantics().alt == "logo");
    }
};

"percent_escapes_remain_exact_only_inside_link_targets"_test = [] {
    const auto source = std::u32string{
        U"100% [local](docs/My%20File.md#part%202) "
        U"<a href=\"#hello%2Dworld\">anchor</a>"};
    const auto tree = parse_inline(source, test_context());
    expect(fatal(is_lossless(source)));
    const auto* link = first_node(tree, InlineCstKind::Link);
    const auto* anchor = first_html_node(tree.nodes, "a");
    expect(fatal(link != nullptr));
    expect(fatal(anchor != nullptr));
    if (link) expect(fatal(link->semantics().href == "docs/My%20File.md#part%202"));
    if (anchor) expect(fatal(anchor->semantics().href == "#hello%2Dworld"));
    InlineDocument document{source, tree};
    expect(fatal(inline_visible_text(document) == U"100% local anchor"));
};

"html_comments_are_lossless_inert_inline_nodes"_test = [] {
    const auto source = std::u32string{U"before <!-- hidden\ncontent --> after"};
    const auto tree = parse_inline(source, test_context());
    expect(fatal(is_lossless(source)));
    const auto* comment = first_node(tree, InlineCstKind::HtmlComment);
    expect(fatal(comment != nullptr));
    if (comment) {
        expect(fatal(comment->status == ParseStatus::Complete));
        expect(fatal(source.substr(comment->range.start, comment->range.length())
            == U"<!-- hidden\ncontent -->"));
    }

    InlineDocument document{source, tree};
    expect(fatal(inline_visible_text(document) == U"before  after"));
};

"unfinished_html_comments_remain_visible_editing_text"_test = [] {
    const auto source = std::u32string{U"before <!-- unfinished"};
    const auto tree = parse_inline(source, test_context());
    expect(fatal(is_lossless(source)));
    expect(fatal(first_node(tree, InlineCstKind::HtmlComment) == nullptr));
    InlineDocument document{source, tree};
    expect(fatal(inline_visible_text(document) == source));
};

"html_text_islands_hide_nested_comments_without_enabling_markdown"_test = [] {
    const auto source = std::u32string{
        U"<span>*literal*<!-- hidden --><strong>visible</strong></span>"};
    auto context = test_context();
    context.syntax_mode = InlineSyntaxMode::HtmlText;
    const auto tree = parse_inline(source, context);
    expect(fatal(is_lossless(source)));
    const auto* span = first_html_node(tree.nodes, "span");
    expect(fatal(span != nullptr));
    if (span) {
        expect(fatal(contains_kind(span->children, InlineCstKind::HtmlComment)));
        expect(fatal(!contains_kind(span->children, InlineCstKind::Emphasis)));
    }
    InlineDocument document{source, tree};
    expect(fatal(inline_visible_text(document) == U"*literal*visible"));
};

};
