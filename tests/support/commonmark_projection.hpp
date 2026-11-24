#pragma once
import folia.markdown.markdown_html;
import folia.markdown.html_entities;

// Semantic HTML-shaped projection for the official CommonMark conformance
// tests. This consumes Folia's parsed tree and lossless inline CST; it is not
// a Markdown parser or a serializer.


#include "support/commonmark/blocks.hpp"
#include "support/commonmark/canonical.hpp"
#include "support/commonmark/html.hpp"
#include "support/commonmark/footnotes.hpp"

namespace folia::test::commonmark {

inline std::string json_string(std::string_view value) {
    return projection_detail::json_string(value);
}

inline bool inline_document_lossless(BlockNode const& block) {
    auto const& doc = block.inline_content();
    if (!(tokens_partition_source(doc.tree, doc.source.size())
        && roots_partition_source(doc.tree, doc.source.size())
        && flatten_tokens(doc.tree, doc.source) == doc.source
        && serialize_lossless(doc.tree, doc.source) == doc.source
        && projection_detail::inline_lossless_nodes(doc.tree.nodes, doc))) return false;
    for (auto const& child : block.children) {
        if (!inline_document_lossless(child)) return false;
    }
    return true;
}

inline bool inline_lossless(BlockNode const& block) {
    return inline_document_lossless(block);
}

inline MarkdownDialect dialect() {
    MarkdownDialect d{};
    d.commonmark = true;
    d.gfm = {false, false, false, false};
    d.math = {false, false, false, false, false, MathRenderBackend::PlainTextFallback};
    d.toc = {false, false, false};
    d.frontmatter = {false, false, false};
    d.footnotes = false;
    d.definition_lists = false;
    d.callouts = false;
    d.wiki_links = false;
    d.highlights = false;
    d.tables = {false, false};
    d.diagrams = {false, false, false};
    return d;
}

inline MarkdownDialect gfm_dialect() {
    auto d = dialect();
    d.gfm = {true, true, true, true};
    d.tables = {true, true};
    return d;
}

inline Projection project(BlockNode const& block, ProjectionOptions const& options = {}) {
    using namespace projection_detail;
    Node strict = element("root");
    Projection result;
    if (block.kind == BlockKind::Document) {
        for (auto const& child : block.children) project_block(strict, child, result.limitations, options);
    } else {
        project_block(strict, block, result.limitations, options);
    }
    finish_footnotes(strict);
    result.projected_html = render_html(strict);
    Node relaxed = clone(strict);
    clean(strict, false);
    clean(relaxed, true);
    encode(strict, result.strict_tree);
    encode(relaxed, result.relaxed_tree);
    std::ranges::sort(result.limitations);
    return result;
}

} // namespace folia::test::commonmark
