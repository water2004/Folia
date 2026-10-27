#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.document;
import folia.markdown.document_symbols;
import folia.markdown.outline;
import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.core.theme;

using namespace folia;
using namespace boost::ut;

namespace {

void expect_quote_roundtrip(std::string_view label, std::string const& source) {
    const auto parsed = parse_text(1, source);
    const auto saved = serialize_markdown(parsed.document);
    expect(saved == source) << label << "; save actual=" << saved << "; expected=" << source;

    const auto reloaded = parse_text(2, saved);
    const auto saved_again = serialize_markdown(reloaded.document);
    expect(saved_again == source)
        << label << "; reload actual=" << saved_again << "; expected=" << source;
}

} // namespace

suite quote_terminal_prefix_tests = [] {

"empty_quote_roundtrips_exactly"_test = [] {
    expect_quote_roundtrip("empty quote", ">\n");
};

"empty_fenced_code_quote_roundtrips_exactly"_test = [] {
    expect_quote_roundtrip("empty fenced code quote", "> ```\n");
};

"fenced_quote_with_one_terminal_lf_roundtrips_exactly"_test = [] {
    expect_quote_roundtrip("fenced quote with one terminal LF", "> ```\n> foo\n");
};

"fenced_quote_with_two_terminal_lfs_keeps_one_real_blank_line"_test = [] {
    expect_quote_roundtrip("fenced quote with two terminal LFs", "> ```\n> foo\n>\n");
};

"unclosed_fence_owns_quoted_terminal_blank_without_outer_paragraph"_test = [] {
    const auto parsed = parse_text(1, "> ```\n> foo\n>\n");
    const auto& root = parsed.document.root.children;
    expect(fatal(root.size() == 1u));
    if (root.size() != 1u) return;
    expect(fatal(root[0].kind == BlockKind::BlockQuote));
    if (root[0].kind != BlockKind::BlockQuote) return;

    const auto& quote_children = root[0].children;
    expect(fatal(quote_children.size() == 1u));
    if (quote_children.size() != 1u) return;
    const auto& code = quote_children[0];
    expect(fatal(code.kind == BlockKind::CodeBlock));
    if (code.kind != BlockKind::CodeBlock) return;

    const auto& source = code.block_source.source();
    const auto& tree = code.block_source.tree();
    expect(tree.kind == BlockSourceKind::FencedCode);
    expect(!tree.complete_closing);
    expect(source == U"```\nfoo\n\n");
    expect(block_source_tokens_partition(code.block_source));
    expect(flatten_block_source_tokens(code.block_source) == source);
    expect(block_source_content(code.block_source) == U"foo\n\n");
    expect(tree.content_to_source.size() == tree.content.size() + 1u);
    if (tree.content_to_source.size() == tree.content.size() + 1u
        && !tree.content_to_source.empty()) {
        expect(tree.content_to_source.front() == 4u);
        expect(tree.content_to_source.back() == source.size());
        expect(std::ranges::is_sorted(tree.content_to_source));
    }

    const auto model = build_render_model(
        parsed.document,
        parsed.outline,
        parsed.symbols,
        default_theme_profile());
    expect(fatal(model.blocks.size() == 1u));
    if (model.blocks.size() != 1u) return;
    const auto& rendered_quote = model.blocks.front();
    expect(fatal(rendered_quote.kind == RenderBlockKind::Quote));
    if (rendered_quote.kind != RenderBlockKind::Quote) return;
    expect(fatal(rendered_quote.child_blocks.size() == 1u));
    if (rendered_quote.child_blocks.size() != 1u) return;
    const auto& rendered_code = rendered_quote.child_blocks.front();
    expect(rendered_code.kind == RenderBlockKind::Code);
    expect(rendered_code.special().code_text == U"foo\n\n");
    expect(rendered_code.special().content_to_source == tree.content_to_source);
    expect(rendered_code.content_span.source_range.start == tree.content_to_source.front());
    expect(rendered_code.content_span.source_range.end == tree.content_to_source.back());
};

"fenced_quote_with_three_terminal_lfs_keeps_two_real_blank_lines"_test = [] {
    expect_quote_roundtrip("fenced quote with three terminal LFs", "> ```\n> foo\n>\n>\n");
};

"quote_blank_before_lazy_following_paragraph_roundtrips_exactly"_test = [] {
    expect_quote_roundtrip("quote blank before lazy following paragraph", "> bar\n>\nbaz\n");
};

"quote_ending_in_real_blank_paragraph_keeps_empty_child"_test = [] {
    constexpr std::string_view source = "> bar\n>\n";
    const auto parsed = parse_text(1, std::string(source));
    const auto& blocks = parsed.document.root.children;
    expect(fatal(blocks.size() == 1u));
    if (blocks.size() != 1u) return;
    expect(fatal(blocks.front().kind == BlockKind::BlockQuote));
    if (blocks.front().kind != BlockKind::BlockQuote) return;

    const auto& children = blocks.front().children;
    expect(fatal(children.size() == 2u));
    if (children.size() != 2u) return;
    expect(children.back().kind == BlockKind::Paragraph);
    expect(children.back().inline_content.source.empty());

    const auto saved = serialize_markdown(parsed.document);
    expect(saved == source) << "quoted real trailing blank actual=" << saved
        << "; expected=" << source;
    const auto reloaded = parse_text(2, saved);
    expect(serialize_markdown(reloaded.document) == source);
};

"indented_code_quote_roundtrips_exactly"_test = [] {
    expect_quote_roundtrip("indented code quote", ">     foo\n");
};

"quote_terminal_prefix_keeps_the_existing_renderer_blank_source_span"_test = [] {
    const auto parsed = parse_text(1, "Hello\n\n");
    const auto model = build_render_model(
        parsed.document,
        parsed.outline,
        parsed.symbols,
        default_theme_profile());

    expect(fatal(model.blocks.size() == 2u));
    if (model.blocks.size() != 2u) return;
    const auto& blank = model.blocks[1];
    expect(fatal(blank.kind == RenderBlockKind::Blank));
    expect(fatal(blank.content_span.container_id == blank.id));
    expect(fatal(blank.content_span.source_range.empty()));
    expect(fatal(blank.content_span.source_range.start == 0u));
};

};
