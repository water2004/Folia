#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.ast;
import folia.markdown.inline_cst;
import folia.markdown.parser;
import folia.markdown.serializer;

import folia.markdown.block_source;
import folia.markdown.dialect;
import folia.markdown.inline_document;
import folia.markdown.utf;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode const* find_kind(BlockNode const& node, BlockKind kind) {
    if (node.kind == kind) return &node;
    for (auto const& child : node.children) {
        if (auto const* found = find_kind(child, kind)) return found;
    }
    return nullptr;
}

BlockNode const* find_footnote(BlockNode const& node, std::string_view label) {
    if (node.kind == BlockKind::FootnoteDefinition
        && node.container_special().footnote_label == label) return &node;
    for (auto const& child : node.children) {
        if (auto const* found = find_footnote(child, label)) return found;
    }
    return nullptr;
}

} // namespace

suite gfm_footnote_projection_tests = [] {

"footnote_projection_uses_ast_definitions_and_reference_order"_test = [] {
    constexpr std::string_view source =
        "first[^b], second[^a], again[^b], missing[^unknown].\n\n"
        "[^a]: alpha\n\n"
        "[^b]: beta with [^a] nested\n\n"
        "[^unused]: never referenced\n";
    auto dialect = folia::test::commonmark::gfm_dialect();
    dialect.footnotes = true;
    auto parsed = parse_text(1, std::string(source), dialect);
    expect(fatal(serialize_markdown(parsed.document) == source));
    expect(fatal(folia::test::commonmark::inline_lossless(parsed.document.root)));

    const auto* b = find_footnote(parsed.document.root, "b");
    expect(fatal(b != nullptr));
    if (b) {
        expect(fatal(b->container_special().footnote_label == "b"));
        expect(fatal(!b->children.empty()));
    }
    auto const* paragraph = find_kind(parsed.document.root, BlockKind::Paragraph);
    expect(fatal(paragraph != nullptr));
    if (paragraph) {
        std::size_t references = 0;
        for (auto const& item : paragraph->inline_content.tree.nodes) {
            if (item.kind == InlineCstKind::FootnoteRef) ++references;
        }
        expect(fatal(references == 4));
    }

    auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.limitations.empty()));
    expect(fatal(projection.projected_html.find("href=\"#fn-b\"") != std::string::npos));
    expect(fatal(projection.projected_html.find("id=\"fnref-b\"") != std::string::npos));
    expect(fatal(projection.projected_html.find(">1</a>") != std::string::npos));
    expect(fatal(projection.projected_html.find("href=\"#fn-a\"") != std::string::npos));
    expect(fatal(projection.projected_html.find("id=\"fnref-a\"") != std::string::npos));
    expect(fatal(projection.projected_html.find(">2</a>") != std::string::npos));
    expect(fatal(projection.projected_html.find("id=\"fnref-b-2\"") != std::string::npos));
    expect(fatal(projection.projected_html.find("[^unknown]") != std::string::npos));
    expect(fatal(projection.projected_html.find("id=\"fn-unused\"") == std::string::npos));
    expect(fatal(projection.projected_html.find("fnref-a-2") != std::string::npos));
};

"footnote_projection_keeps_recursive_definition_blocks_and_escapes_labels"_test = [] {
    constexpr std::string_view source =
        "note[^\"><script>alert(1)</script>]\n\n"
        "[^\"><script>alert(1)</script>]:\n"
        "    > quoted content\n\n"
        "        code content\n";
    auto dialect = folia::test::commonmark::gfm_dialect();
    dialect.footnotes = true;
    auto parsed = parse_text(2, std::string(source), dialect);
    expect(fatal(serialize_markdown(parsed.document) == source));
    auto const* definition = find_kind(parsed.document.root, BlockKind::FootnoteDefinition);
    expect(fatal(definition != nullptr));
    if (definition) expect(fatal(definition->children.size() >= 2));
    auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.limitations.empty()));
    expect(fatal(projection.projected_html.find("%22%3E%3Cscript%3E") != std::string::npos));
    expect(fatal(projection.projected_html.find("<blockquote>") != std::string::npos));
    expect(fatal(projection.projected_html.find("<pre><code>") != std::string::npos));
    expect(fatal(projection.projected_html.find("footnote-backref") != std::string::npos));
};

};
