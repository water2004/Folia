#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

namespace {

bool inline_sources_are_lossless(const BlockVec& blocks) {
    for (const auto& block : blocks) {
        if (flatten_tokens(block.inline_content.tree, block.inline_content.source)
            != block.inline_content.source) return false;
        if (!inline_sources_are_lossless(block.children)) return false;
    }
    return true;
}

bool contains_kind(const BlockNode& block, BlockKind kind) {
    if (block.kind == kind) return true;
    for (const auto& child : block.children) {
        if (contains_kind(child, kind)) return true;
    }
    return false;
}

void expect_thematic_boundary(std::string_view label, const std::string& source) {
    const auto parsed = parse_text(1, source);
    const auto& blocks = parsed.document.root.children;
    expect(blocks.size() == 3u) << label << " root block count";
    if (blocks.size() == 3) {
        expect(blocks[0].kind == BlockKind::List) << label << " first list";
        expect(blocks[1].kind == BlockKind::ThematicBreak) << label << " thematic break";
        expect(blocks[2].kind == BlockKind::List) << label << " second list";
    }
    expect(inline_sources_are_lossless(blocks)) << label << " inline CST";
    const auto saved = serialize_markdown(parsed.document);
    expect(saved == source) << label << " exact save";
    const auto reloaded = parse_text(2, saved);
    const auto& reloaded_blocks = reloaded.document.root.children;
    expect(reloaded_blocks.size() == 3u) << label << " reload block count";
    if (reloaded_blocks.size() == 3) {
        expect(reloaded_blocks[0].kind == BlockKind::List) << label << " reload first list";
        expect(reloaded_blocks[1].kind == BlockKind::ThematicBreak) << label << " reload thematic break";
        expect(reloaded_blocks[2].kind == BlockKind::List) << label << " reload second list";
    }
    expect(inline_sources_are_lossless(reloaded_blocks)) << label << " reload inline CST";
    expect(serialize_markdown(reloaded.document) == source) << label << " reload exact save";
}

} // namespace

suite parser_list_thematic_boundary_tests = [] {

"commonmark_60_star_thematic_break_splits_lists"_test = [] {
    expect_thematic_boundary("CommonMark #60", "* Foo\n* * *\n* Bar\n");
};

"thematic_break_has_precedence_for_dash_list_markers"_test = [] {
    expect_thematic_boundary("dash thematic break", "- Foo\n- - -\n- Bar\n");
};

"thematic_break_has_precedence_for_underscore_lines"_test = [] {
    expect_thematic_boundary("underscore thematic break", "- Foo\n_ _ _\n- Bar\n");
};

"thematic_break_has_precedence_with_mixed_spaces"_test = [] {
    expect_thematic_boundary("spaced thematic break", "  * Foo\n *   * *\n  * Bar\n");
};

"nearby_nested_list_marker_remains_a_nested_list"_test = [] {
    const std::string source = "* Foo\n  * nested\n* Bar\n";
    const auto parsed = parse_text(1, source);
    const auto& blocks = parsed.document.root.children;
    expect(blocks.size() == 1u) << "nested list root count";
    if (blocks.size() == 1 && blocks.front().kind == BlockKind::List
        && !blocks.front().children.empty()) {
        expect(contains_kind(blocks.front().children.front(), BlockKind::List))
            << "nested list retained";
    }
    expect(inline_sources_are_lossless(blocks)) << "nested list CST";
    const auto saved = serialize_markdown(parsed.document);
    expect(saved == source) << "nested list exact save";
    const auto reloaded = parse_text(2, saved);
    expect(reloaded.document.root.children.size() == 1u) << "nested list reload root count";
    expect(inline_sources_are_lossless(reloaded.document.root.children)) << "nested list reload CST";
    expect(serialize_markdown(reloaded.document) == source) << "nested list reload exact save";
};

}; // suite parser_list_thematic_boundary_tests
