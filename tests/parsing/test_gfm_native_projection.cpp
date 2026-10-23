#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"

import folia.core.ast;
import folia.core.block_source;
import folia.core.dialect;
import folia.core.inline_cst;
import folia.core.inline_document;
import folia.core.parser;
import folia.core.serializer;
import folia.core.utf;
import folia.core.markdown_html;

#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode const* find_kind(BlockNode const& block, BlockKind kind) {
    if (block.kind == kind) return &block;
    for (auto const& child : block.children) {
        if (auto const* found = find_kind(child, kind)) return found;
    }
    return nullptr;
}

bool contains_inline_kind(InlineCstNodes const& nodes, InlineCstKind kind) {
    for (auto const& node : nodes) {
        if (node.kind == kind || contains_inline_kind(node.children, kind)) return true;
    }
    return false;
}

} // namespace

suite gfm_native_projection_tests = [] {

"gfm_table_projection_uses_native_header_alignment_and_cell_cst"_test = [] {
    constexpr std::string_view markdown =
        "| abc | defghi |\n"
        "| :---: | ---: |\n"
        "| bar | **baz** |\n";
    auto parsed = parse_text(1, std::string(markdown), folia::test::commonmark::gfm_dialect());
    expect(fatal(serialize_markdown(parsed.document) == markdown));
    expect(fatal(folia::test::commonmark::inline_lossless(parsed.document.root)));
    auto const* table = find_kind(parsed.document.root, BlockKind::Table);
    expect(fatal(table != nullptr));
    if (!table) return;
    expect(fatal(table->children.size() == 2));
    expect(fatal(table->children[0].kind == BlockKind::TableRow));
    expect(fatal(table->children[0].table_special().table_header_row));
    expect(fatal(table->table_special().table_aligns.size() == 2));
    expect(fatal(table->table_special().table_aligns[0] == TableAlignment::Center));
    expect(fatal(table->table_special().table_aligns[1] == TableAlignment::Right));
    auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.projected_html.find("<th align=\"center\">abc</th>") != std::string::npos));
    expect(fatal(projection.projected_html.find("<th align=\"right\">defghi</th>") != std::string::npos));
    expect(fatal(projection.projected_html.find("<td align=\"center\">bar</td>") != std::string::npos));
    expect(fatal(projection.projected_html.find("<td align=\"right\"><strong>baz</strong></td>") != std::string::npos));
    expect(projection.limitations.empty());
};

"gfm_task_items_project_checkbox_from_native_checked_state_and_keep_inline_content"_test = [] {
    constexpr std::string_view markdown = "- [ ] foo\n- [x] ~~bar~~\n";
    auto parsed = parse_text(2, std::string(markdown), folia::test::commonmark::gfm_dialect());
    expect(fatal(serialize_markdown(parsed.document) == markdown));
    expect(fatal(folia::test::commonmark::inline_lossless(parsed.document.root)));
    auto const* list = find_kind(parsed.document.root, BlockKind::TaskList);
    expect(fatal(list != nullptr));
    if (!list) return;
    expect(fatal(list->children.size() == 2));
    expect(fatal(list->children[0].kind == BlockKind::TaskListItem));
    expect(fatal(!list->children[0].item_special().checked));
    expect(fatal(list->children[1].item_special().checked));
    auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.projected_html.find("<input disabled=\"\" type=\"checkbox\">") != std::string::npos));
    expect(fatal(projection.projected_html.find("<input checked=\"\" disabled=\"\" type=\"checkbox\">") != std::string::npos));
    expect(fatal(projection.projected_html.find("<del>bar</del>") != std::string::npos));
    expect(projection.limitations.empty());
};

"projection_does_not_invent_gfm_links_for_plain_text_cst"_test = [] {
    constexpr std::string_view markdown = "Visit www.commonmark.org/help.";
    auto parsed = parse_text(3, std::string(markdown), folia::test::commonmark::dialect());
    expect(fatal(serialize_markdown(parsed.document) == markdown));
    expect(fatal(folia::test::commonmark::inline_lossless(parsed.document.root)));
    auto const* paragraph = find_kind(parsed.document.root, BlockKind::Paragraph);
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    expect(fatal(!contains_inline_kind(paragraph->inline_content.tree.nodes, InlineCstKind::Autolink)));
    expect(fatal(contains_inline_kind(paragraph->inline_content.tree.nodes, InlineCstKind::Text)));
    auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.projected_html
        == "<p>Visit www.commonmark.org/help.</p>\n"));
    expect(fatal(projection.limitations.empty()));
};

};
