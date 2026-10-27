#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.container_source;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_history;
import folia.core.document_operation_apply;
import folia.markdown.document_symbols;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.outline;
import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.core.theme;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/document_edit_test_support.hpp"

namespace {

void expect_first_prefix_roundtrip(
    std::string_view label,
    std::string const& source,
    std::u32string_view expected_prefix) {
    const auto parsed = parse_text(1, source);
    const auto& roots = parsed.document.root.children;
    expect(fatal(roots.size() == 1u)) << label << "; root count=" << roots.size();
    if (roots.size() != 1u || roots.front().kind != BlockKind::BlockQuote) return;

    const auto& quote = roots.front();
    expect(quote.container_source().lines.front().text == expected_prefix)
        << label << "; marker=" << cps_to_utf8(quote.container_source().lines.front().text);
    expect(serialize_markdown(parsed.document) == source)
        << label << "; serialized=" << serialize_markdown(parsed.document)
        << "; expected=" << source;
    const auto reloaded = parse_text(2, serialize_markdown(parsed.document));
    expect(serialize_markdown(reloaded.document) == source)
        << label << "; reload save differs";

    if (!quote.children.empty()
        && quote.children.front().kind == BlockKind::Paragraph) {
        const auto& paragraph = quote.children.front();
        expect(flatten_tokens(
            paragraph.inline_content.tree,
            paragraph.inline_content.source)
            == paragraph.inline_content.source);
        expect(serialize_lossless(
            paragraph.inline_content.tree,
            paragraph.inline_content.source) == paragraph.inline_content.source);

        const auto projection = serialize_markdown_projection(parsed.document);
        expect(projection.text == utf8_to_cps(source));
        const auto mapped_start = serialized_offset_for_source_position(
            projection,
            TextPosition{paragraph.id, 0, TextAffinity::Downstream});
        expect(mapped_start.has_value());
        if (mapped_start) expect(*mapped_start == expected_prefix.size());
    }

    const auto model = build_render_model(
        parsed.document,
        parsed.outline,
        parsed.symbols,
        default_theme_profile());
    expect(fatal(model.blocks.size() == 1u));
    if (model.blocks.size() != 1u) return;
    expect(fatal(model.blocks.front().kind == RenderBlockKind::Quote));
    expect(model.blocks.front().child_blocks.size() == quote.children.size());
    if (!quote.children.empty() && !model.blocks.front().child_blocks.empty()
        && quote.children.front().kind == BlockKind::Paragraph) {
        const auto& source_paragraph = quote.children.front();
        const auto& rendered_paragraph = model.blocks.front().child_blocks.front();
        expect(rendered_paragraph.source_span.container_id == source_paragraph.id);
        expect(rendered_paragraph.source_span.source_range.start == 0u);
        expect(rendered_paragraph.source_span.source_range.end
            == source_paragraph.inline_content.source.size());
    }
}

} // namespace

suite quote_opening_marker_tests = [] {

"first_line_quote_prefix_is_preserved_verbatim"_test = [] {
    const std::vector<std::pair<std::string, std::u32string>> cases{
        {">foo\n", U">"},
        {" >foo\n", U" >"},
        {"  >foo\n", U"  >"},
        {"   >foo\n", U"   >"},
        {"> foo\n", U"> "},
    };
    for (const auto& [source, prefix] : cases) {
        expect_first_prefix_roundtrip("quote first prefix", source, prefix);
    }
};

"empty_quote_and_code_quote_keep_their_first_prefix"_test = [] {
    expect_first_prefix_roundtrip("empty quote", ">\n", U">");
    expect_first_prefix_roundtrip("code quote", "  > ```\n", U"  > ");
};

"nested_quotes_keep_each_first_prefix"_test = [] {
    const auto parsed = parse_text(1, "> >foo\n");
    expect(fatal(parsed.document.root.children.size() == 1u));
    if (parsed.document.root.children.size() != 1u) return;
    const auto& outer = parsed.document.root.children.front();
    expect(fatal(outer.kind == BlockKind::BlockQuote));
    if (outer.kind != BlockKind::BlockQuote) return;
    expect(outer.container_source().lines.front().text == U"> ");
    expect(fatal(outer.children.size() == 1u));
    if (outer.children.size() != 1u) return;
    const auto& inner = outer.children.front();
    expect(fatal(inner.kind == BlockKind::BlockQuote));
    if (inner.kind != BlockKind::BlockQuote) return;
    expect(inner.container_source().lines.front().text == U">");
    expect(serialize_markdown(parsed.document) == "> >foo\n");
};

"enter_split_undo_redo_retain_quote_first_prefix"_test = [] {
    const std::string source = "  >foo\n> bar\n";
    auto document = parse_document(source);
    expect(fatal(document.root.children.size() == 1u));
    if (document.root.children.size() != 1u) return;
    auto& quote = document.root.children.front();
    expect(fatal(quote.kind == BlockKind::BlockQuote));
    if (quote.kind != BlockKind::BlockQuote || quote.children.empty()) return;
    const auto quote_id = quote.id;
    expect(quote.container_source().lines.front().text == U"  >");
    auto& paragraph = quote.children.front();
    expect(fatal(paragraph.kind == BlockKind::Paragraph));
    if (paragraph.kind != BlockKind::Paragraph) return;

    auto split = test_edit::document_enter(document, caret(paragraph, 3));
    expect(fatal(split.has_value()));
    if (!split) return;
    expect(fatal(split->after.root.children.size() == 1u));
    if (split->after.root.children.size() != 1u) return;
    const auto& split_quote = split->after.root.children.front();
    expect(split_quote.id == quote_id);
    expect(split_quote.container_source().lines.front().text == U"  >");
    const auto split_saved = serialize_markdown(split->after);
    expect(std::string_view(split_saved).starts_with("  >")) << split_saved;
    expect(split_quote.children.size() == 2u);
    if (split_quote.children.size() == 2u) {
        for (const auto& child : split_quote.children) {
            expect(flatten_tokens(
                child.inline_content.tree,
                child.inline_content.source) == child.inline_content.source);
            expect(serialize_lossless(child.inline_content.tree, child.inline_content.source)
                == child.inline_content.source);
        }
    }

    auto working = std::move(split->after);
    auto selection = split->selection_after;
    DocumentHistory history;
    history.push(*split);
    expect(fatal(history.undo(working, selection)));
    expect(serialize_markdown(working) == source);
    expect(working.root.children.front().container_source().lines.front().text == U"  >");
    expect(fatal(history.redo(working, selection)));
    expect(serialize_markdown(working) == split_saved);
    expect(working.root.children.front().container_source().lines.front().text == U"  >");
};

};
