#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.serializer;
import folia.markdown.block_source;
import folia.markdown.dialect;
import folia.markdown.utf;

#include "support/commonmark_projection.hpp"
#include "support/gfm_capacity_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

using namespace folia::test::gfm_capacity;

std::string make_small_input(ContainerFixture which, std::size_t depth) {
    std::string input;
    if (which == ContainerFixture::NestedBlockquotes) {
        for (std::size_t i = 0; i < depth; ++i) input += "> ";
        input += 'a';
        return input;
    }
    for (std::size_t i = 0; i < depth; ++i) {
        input.append(i * 2, ' ');
        input += "* a\n";
    }
    return input;
}

BlockNode* first_paragraph(BlockNode& root) {
    std::vector<BlockNode*> pending{&root};
    while (!pending.empty()) {
        auto* block = pending.back();
        pending.pop_back();
        if (block->kind == BlockKind::Paragraph) return block;
        for (auto& child : block->children) pending.push_back(&child);
    }
    return nullptr;
}

std::string normalize_tag_gaps(std::string_view html) {
    std::string normalized;
    normalized.reserve(html.size());
    for (std::size_t i = 0; i < html.size(); ++i) {
        const auto ch = html[i];
        // The narrow list projector validates each paragraph source as exactly
        // "a". cmark emits this generated line break before a nested list;
        // the shared projector joins the two block nodes directly.
        if (ch == '\n' && !normalized.empty() && normalized.back() == 'a'
            && i + 4 < html.size() && html.substr(i, 5) == "\n<ul>") {
            continue;
        }
        if ((ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            && !normalized.empty() && normalized.back() == '>') {
            auto next = i;
            while (next < html.size() && (html[next] == ' ' || html[next] == '\t'
                || html[next] == '\r' || html[next] == '\n')) ++next;
            if (next < html.size() && html[next] == '<') {
                i = next - 1;
                continue;
            }
        }
        normalized.push_back(ch);
    }
    return normalized;
}

std::size_t first_difference(std::string_view left, std::string_view right) {
    const auto common = (std::min)(left.size(), right.size());
    std::size_t i = 0;
    while (i < common && left[i] == right[i]) ++i;
    return i;
}

void compare_at_depth(ContainerFixture which, std::size_t depth) {
    const auto source = make_small_input(which, depth);
    auto parsed = parse_text(1, source, folia::test::commonmark::dialect());
    const auto iterative = folia::test::gfm_capacity::project_pathological_containers(parsed.document.root, which);
    expect(iterative.has_value()) << "iterative projector rejected canonical shape at depth " << depth;
    if (iterative) {
        const auto reference = folia::test::commonmark::project(parsed.document.root).projected_html;
        const auto actual_shape = normalize_tag_gaps(*iterative);
        const auto reference_shape = normalize_tag_gaps(reference);
        const auto difference = first_difference(actual_shape, reference_shape);
        expect(actual_shape == reference_shape)
            << "tag-gap-normalized HTML differs at depth " << depth
            << " at byte " << difference
            << "; iterative=" << actual_shape.substr(difference > 48 ? difference - 48 : 0, 128)
            << "; shared=" << reference_shape.substr(difference > 48 ? difference - 48 : 0, 128);
    }
    std::size_t owners = 0;
    expect(folia::test::gfm_capacity::pathological_inline_sources_lossless(parsed.document.root, owners));
    expect(owners > 0_u);
}

} // namespace

suite pathological_container_streaming_tests = [] {
    "quote_projector_matches_reference_at_bounded_depths"_test = [] {
        for (const auto depth : std::array<std::size_t, 3>{1, 2, 8})
            compare_at_depth(ContainerFixture::NestedBlockquotes, depth);
    };

    "list_projector_matches_reference_at_bounded_depths"_test = [] {
        for (const auto depth : std::array<std::size_t, 3>{1, 2, 8})
            compare_at_depth(ContainerFixture::DeepLists, depth);
    };

    "projector_rejects_unexpected_paragraph_source_and_cst"_test = [] {
        auto source_changed = parse_text(1, "> a", folia::test::commonmark::dialect());
        auto* paragraph = first_paragraph(source_changed.document.root);
        expect(paragraph != nullptr);
        if (paragraph) {
            paragraph->inline_content.source = U"b";
            expect(!folia::test::gfm_capacity::project_pathological_containers(
                source_changed.document.root, ContainerFixture::NestedBlockquotes).has_value());
        }

        auto cst_changed = parse_text(1, "> a", folia::test::commonmark::dialect());
        paragraph = first_paragraph(cst_changed.document.root);
        expect(paragraph != nullptr);
        if (paragraph && !paragraph->inline_content.tree.nodes.empty()) {
            paragraph->inline_content.tree.nodes.front().kind = InlineCstKind::Emphasis;
            expect(!folia::test::gfm_capacity::project_pathological_containers(
                cst_changed.document.root, ContainerFixture::NestedBlockquotes).has_value());
        }
    };

    "projector_rejects_unexpected_list_kind"_test = [] {
        auto parsed = parse_text(1, "* a\n  * a\n", folia::test::commonmark::dialect());
        expect(!parsed.document.root.children.empty());
        if (!parsed.document.root.children.empty()) {
            parsed.document.root.children.front().kind = BlockKind::BlockQuote;
            expect(!folia::test::gfm_capacity::project_pathological_containers(
                parsed.document.root, ContainerFixture::DeepLists).has_value());
        }
    };
};
