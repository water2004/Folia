#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

import folia.markdown.markdown_html;
import folia.markdown.dialect;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

void collect_lists(BlockNode const& block, std::vector<BlockNode const*>& out) {
    if (block.kind == BlockKind::List) out.push_back(&block);
    for (auto const& child : block.children) collect_lists(child, out);
}

void expect_lossless_parse(std::string_view source) {
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    expect(serialize_markdown(parsed.document) == source) << "list tightness exact save";
    expect(folia::test::commonmark::inline_lossless(parsed.document.root))
        << "list tightness inline CST losslessness";
    const auto reloaded = parse_text(2, serialize_markdown(parsed.document), folia::test::commonmark::dialect());
    expect(serialize_markdown(reloaded.document) == source) << "list tightness reload save";
}

} // namespace

suite parser_list_tightness_tests = [] {

"tight_list_projects_direct_paragraphs_without_p"_test = [] {
    constexpr std::string_view source = "- alpha\n- beta\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 1u) << "one tight list";
    if (lists.size() != 1) return;
    expect(lists.front()->list_special().tight) << "tight list syntax attribute";
    expect(folia::test::commonmark::project(parsed.document.root).strict_tree
        == R"(["root",[],[["ul",[],[["li",[],["alpha"]],["li",[],["beta"]]]]]])")
        << "strict tight-list projection";
    expect_lossless_parse(source);
};

"blank_line_between_items_makes_list_loose"_test = [] {
    constexpr std::string_view source = "- alpha\n\n- beta\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 1u) << "one loose list";
    if (lists.size() != 1) return;
    expect(!lists.front()->list_special().tight) << "blank sibling separator makes list loose";
    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(projection.strict_tree
        == R"(["root",[],[["ul",[],[["li",[],[["p",[],["alpha"]]]],["li",[],[["p",[],["beta"]]]]]]]])")
        << "strict loose-list projection keeps p";
    expect(projection.strict_tree != projection.relaxed_tree)
        << "relaxed diagnostic does not replace strict loose-list semantics";
    expect_lossless_parse(source);
};

"blank_line_between_item_paragraphs_makes_list_loose"_test = [] {
    constexpr std::string_view source = "- alpha\n\n  beta\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 1u) << "one list with separate item paragraphs";
    if (lists.size() != 1) return;
    expect(!lists.front()->list_special().tight) << "blank direct paragraph separator makes list loose";
    expect(lists.front()->children.front().children.size() >= 2u)
        << "both direct paragraphs remain block-tree children";
    expect_lossless_parse(source);
};

"blank_line_between_direct_item_blocks_makes_list_loose"_test = [] {
    constexpr std::string_view source = "- alpha\n\n  > quote\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 1u) << "one list with multiple direct item blocks";
    if (lists.size() != 1) return;
    expect(!lists.front()->list_special().tight) << "direct item block separator makes list loose";
    expect_lossless_parse(source);
};

"blank_after_code_terminal_eol_makes_list_loose"_test = [] {
    constexpr std::string_view source = "-     code\n\n  > quote\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 1u) << "one list with code and quote blocks";
    if (lists.size() != 1) return;
    expect(!lists.front()->list_special().tight)
        << "blank line after code-owned terminal EOL makes list loose";
    expect(lists.front()->children.front().children.size() == 2u)
        << "code and quote remain direct item blocks";
    if (lists.front()->children.front().children.size() == 2u) {
        expect(lists.front()->children.front().children[0].kind == BlockKind::CodeBlock)
            << "first direct block is code";
        expect(lists.front()->children.front().children[1].kind == BlockKind::BlockQuote)
            << "second direct block is quote";
    }
    expect_lossless_parse(source);
};

"editable_blank_paragraph_keeps_loose_list_semantics"_test = [] {
    constexpr std::string_view source = "- alpha\n\n\n  beta\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 1u) << "one list with an editable blank paragraph";
    if (lists.size() != 1) return;
    expect(!lists.front()->list_special().tight) << "blank block gap makes list loose";
    bool has_empty_direct_paragraph = false;
    if (!lists.front()->children.empty()) {
        for (auto const& child : lists.front()->children.front().children) {
            has_empty_direct_paragraph = has_empty_direct_paragraph
                || (child.kind == BlockKind::Paragraph && child.inline_content.source.empty());
        }
    }
    expect(has_empty_direct_paragraph) << "extra blank line remains an editable paragraph";
    expect_lossless_parse(source);
};

"nested_list_internal_blank_does_not_loosen_outer_list"_test = [] {
    constexpr std::string_view source = "- parent\n  - nested a\n\n  - nested b\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 2u) << "outer and nested lists";
    if (lists.size() != 2) return;
    expect(lists[0]->list_special().tight) << "nested blank line does not loosen outer list";
    expect(!lists[1]->list_special().tight) << "nested list tracks its own looseness";
    expect_lossless_parse(source);
};

"trailing_blank_before_outside_block_does_not_loosen_list"_test = [] {
    constexpr std::string_view source = "- alpha\n\noutside\n";
    const auto parsed = parse_text(1, std::string(source), folia::test::commonmark::dialect());
    std::vector<BlockNode const*> lists;
    collect_lists(parsed.document.root, lists);
    expect(lists.size() == 1u) << "one list before outside paragraph";
    if (lists.size() != 1) return;
    expect(lists.front()->list_special().tight) << "external trailing separator does not loosen list";
    expect_lossless_parse(source);
};

}; // suite parser_list_tightness_tests
