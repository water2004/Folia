#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

// Full source sizes and shapes are ported from the pinned cmark-gfm
// pathological_tests.py (commit 27d942c8b0a62d192f616e5bf3578f4b6a89e180).
// These are isolated from FoliaTests and each is launched under a process
// timeout by run_gfm_pathological.py.
enum class Case {
    NestedStrongEmph, ManyEmphClosers, ManyEmphOpeners, ManyLinkClosers,
    ManyLinkOpeners, MismatchedOpenersClosers, DelimiterRunsMultipleOfThree,
    LinkOpenersEmphClosers, RepeatedBracketParen, RepeatedImageLink,
    HardLinkEmph, NestedBrackets, NestedBlockquotes, DeepLists,
    EmbeddedNul, Backticks, UnclosedLinksAngle, UnclosedLinksParen,
    UnclosedComments, Tables, ReferenceCollisions,
};

std::uint32_t bad_reference_hash(std::string_view reference) {
    std::uint32_t hash = 0;
    for (unsigned char ch : reference) {
        const auto a = (hash << 6) & 0xffffffffu;
        const auto b = (hash << 16) & 0xffffffffu;
        hash = static_cast<std::uint32_t>(ch + a + b - hash);
    }
    return hash;
}

std::string make_input(Case which) {
    std::string input;
    switch (which) {
        case Case::NestedStrongEmph:
            input.reserve(1'820'001);
            for (int i = 0; i < 65000; ++i) input += "*a **a ";
            input += 'b';
            for (int i = 0; i < 65000; ++i) input += " a** a*";
            break;
        case Case::ManyEmphClosers: input.reserve(195000); for (int i=0;i<65000;++i) input += "a_ "; break;
        case Case::ManyEmphOpeners: input.reserve(195000); for (int i=0;i<65000;++i) input += "_a "; break;
        case Case::ManyLinkClosers: input.reserve(130000); for (int i=0;i<65000;++i) input += "a]"; break;
        case Case::ManyLinkOpeners: input.reserve(130000); for (int i=0;i<65000;++i) input += "[a"; break;
        case Case::MismatchedOpenersClosers: input.reserve(200000); for (int i=0;i<50000;++i) input += "*a_ "; break;
        case Case::DelimiterRunsMultipleOfThree:
            input = "a**b"; input.reserve(150004); for (int i=0;i<50000;++i) input += "c* "; break;
        case Case::LinkOpenersEmphClosers: input.reserve(200000); for (int i=0;i<50000;++i) input += "[ a_"; break;
        case Case::RepeatedBracketParen: input.reserve(400000); for (int i=0;i<80000;++i) input += "[ (]("; break;
        case Case::RepeatedImageLink: input.reserve(960000); for (int i=0;i<160000;++i) input += "![[]()"; break;
        case Case::HardLinkEmph: input = "**x [a*b**c*](d)"; break;
        case Case::NestedBrackets: input.assign(50000, '['); input += 'a'; input.append(50000, ']'); break;
        case Case::NestedBlockquotes: input.reserve(150001); for (int i=0;i<50000;++i) input += "> "; input += 'a'; break;
        case Case::DeepLists:
            for (int i=0;i<1000;++i) { input.append(static_cast<std::size_t>(i)*2, ' '); input += "* a\n"; }
            break;
        case Case::EmbeddedNul: input.assign("abc\0de\0", 7); break;
        case Case::Backticks:
            for (int i=1;i<5000;++i) { input += 'e'; input.append(static_cast<std::size_t>(i), '`'); }
            break;
        case Case::UnclosedLinksAngle: input.reserve(180000); for (int i=0;i<30000;++i) input += "[a](<b"; break;
        case Case::UnclosedLinksParen: input.reserve(150000); for (int i=0;i<30000;++i) input += "[a](b"; break;
        case Case::UnclosedComments: input = "</"; input.reserve(1'200'002); for (int i=0;i<300000;++i) input += "<!--"; break;
        case Case::Tables: input.reserve(330000); for (int i=0;i<30000;++i) input += "aaa\rbbb\n-\v\n"; break;
        case Case::ReferenceCollisions: {
            constexpr std::uint32_t refmap_size = 16;
            std::vector<std::string> keys;
            keys.reserve(49999);
            input.reserve(1'150'000);
            std::string collision;
            for (std::uint64_t i=0; collision.empty(); ++i) {
                auto key = "x" + std::to_string(i);
                if (bad_reference_hash(key) % refmap_size == 0) collision = std::move(key);
            }
            for (std::uint64_t i=0; keys.size()<49999; ++i) {
                auto key = "x" + std::to_string(i);
                if (key != collision && bad_reference_hash(key) % refmap_size == 0)
                    keys.push_back(std::move(key));
            }
            for (const auto& key : keys) {
                input += '[' + key + "]: /url\n\n[" + collision + "]\n\n";
            }
            break;
        }
    }
    return input;
}

void append_repeat(std::string& output, std::string_view piece, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) output.append(piece);
}

bool contains_repeat(std::string_view output, std::string_view piece, std::size_t count) {
    if (piece.empty()) return false;
    std::string expected;
    expected.reserve(piece.size() * count);
    append_repeat(expected, piece, count);
    return output.find(expected) != std::string_view::npos;
}

bool contains(std::string_view output, std::string_view piece) {
    return output.find(piece) != std::string_view::npos;
}

std::string first_collision_key() {
    for (std::uint64_t index = 0;; ++index) {
        auto key = "x" + std::to_string(index);
        if (bad_reference_hash(key) % 16 == 0) return key;
    }
}

// The exact deep-emphasis fixture needs no intermediate HTML node tree.
// Stream its native CST to the expected HTML vocabulary with explicit frames;
// any unexpected CST kind or delimiter spelling is a test failure.
std::string project_deep_emphasis(const BlockNode& root) {
    const InlineDocument* document = nullptr;
    std::vector<const BlockNode*> blocks{&root};
    while (!blocks.empty()) {
        const auto* block = blocks.back(); blocks.pop_back();
        if (is_editable_block_owner(block->kind)) {
            if (document || block->kind != BlockKind::Paragraph) return {};
            document = &block->inline_content;
        }
        for (auto const& child : block->children) blocks.push_back(&child);
    }
    if (!document) return {};
    struct Frame { const InlineCstNodes* nodes; std::size_t index; InlineCstKind kind; };
    std::vector<Frame> frames{{&document->tree.nodes, 0, InlineCstKind::Text}};
    std::string html = "<p>";
    while (!frames.empty()) {
        auto& frame = frames.back();
        if (frame.index == frame.nodes->size()) {
            const auto kind = frame.kind;
            frames.pop_back();
            if (kind == InlineCstKind::Emphasis) html += "</em>";
            else if (kind == InlineCstKind::Strong) html += "</strong>";
            continue;
        }
        const auto& node = (*frame.nodes)[frame.index++];
        if (node.kind == InlineCstKind::Text) {
            const auto text = inline_source_slice(*document, node.range);
            for (const auto ch : text) if (ch != U'a' && ch != U'b' && ch != U' ') return {};
            html += cps_to_utf8(text);
        } else if (node.kind == InlineCstKind::Emphasis || node.kind == InlineCstKind::Strong) {
            if (node.status != ParseStatus::Complete) return {};
            const auto spelling = node.kind == InlineCstKind::Emphasis ? U"*" : U"**";
            const auto& ranges = node.delimiter_ranges();
            if (!ranges.closing || inline_source_slice(*document, ranges.opening) != spelling
                || inline_source_slice(*document, *ranges.closing) != spelling) return {};
            html += node.kind == InlineCstKind::Emphasis ? "<em>" : "<strong>";
            frames.push_back({&node.children, 0, node.kind});
        } else return {};
    }
    html += "</p>\n";
    return html;
}

bool has_expected_semantics(Case which, std::string_view html) {
    std::string expected;
    switch (which) {
        case Case::NestedStrongEmph:
            expected.clear();
            append_repeat(expected, "<em>a <strong>a ", 65000);
            expected += 'b';
            append_repeat(expected, " a</strong> a</em>", 65000);
            return contains(html, expected);
        case Case::ManyEmphClosers:
            expected.clear(); append_repeat(expected, "a_ ", 64999); expected += "a_";
            return contains(html, expected);
        case Case::ManyEmphOpeners:
            expected.clear(); append_repeat(expected, "_a ", 64999); expected += "_a";
            return contains(html, expected);
        case Case::ManyLinkClosers:
            return contains_repeat(html, "a]", 65000);
        case Case::ManyLinkOpeners:
            return contains_repeat(html, "[a", 65000);
        case Case::MismatchedOpenersClosers:
            expected.clear(); append_repeat(expected, "*a_ ", 49999); expected += "*a_";
            return contains(html, expected);
        case Case::DelimiterRunsMultipleOfThree:
            expected = "a**b"; append_repeat(expected, "c* ", 49999); expected += "c*";
            return contains(html, expected);
        case Case::LinkOpenersEmphClosers:
            expected.clear(); append_repeat(expected, "[ a_", 50000);
            return contains(html, expected);
        case Case::RepeatedBracketParen:
            expected.clear(); append_repeat(expected, "[ (](", 80000);
            return contains(html, expected);
        case Case::RepeatedImageLink:
            expected.clear(); append_repeat(expected, "![<a href=\"\"></a>", 160000);
            return contains(html, expected);
        case Case::HardLinkEmph:
            return contains(html, "**x <a href=\"d\">a<em>b**c</em></a>");
        case Case::NestedBrackets:
            expected.assign(50000, '['); expected += 'a'; expected.append(50000, ']');
            return contains(html, expected);
        case Case::NestedBlockquotes:
            return contains_repeat(html, "<blockquote>\n", 50000);
        case Case::DeepLists:
            expected = "<ul>\n"; append_repeat(expected, "<li>a\n<ul>\n", 999);
            expected += "<li>a</li>\n</ul>\n";
            append_repeat(expected, "</li>\n</ul>\n", 999);
            return contains(html, expected);
        case Case::EmbeddedNul: {
            constexpr std::string_view replacement = "\xef\xbf\xbd";
            return contains(html, "abcde") || contains(html, "abc" + std::string(replacement) + "de")
                || contains(html, "abcde" + std::string(replacement))
                || contains(html, "abc" + std::string(replacement) + "de" + std::string(replacement));
        }
        case Case::Backticks: {
            if (!html.starts_with("<p>") || !html.ends_with("</p>\n")) return false;
            const auto body = std::string_view(html).substr(3, html.size() - 8);
            return std::ranges::all_of(body, [](unsigned char ch) { return ch == 'e' || ch == '`'; });
        }
        case Case::UnclosedLinksAngle:
            expected.clear(); append_repeat(expected, "[a](&lt;b", 30000);
            return contains(html, expected);
        case Case::UnclosedLinksParen:
            expected.clear(); append_repeat(expected, "[a](b", 30000);
            return contains(html, expected);
        case Case::UnclosedComments:
            expected = "&lt;/"; append_repeat(expected, "&lt;!--", 300000);
            return contains(html, expected);
        case Case::Tables:
            expected = "<p>aaa</p>\n<table>\n<thead>\n<tr>\n<th>bbb</th>\n</tr>\n</thead>\n<tbody>\n";
            append_repeat(expected, "<tr>\n<td>aaa</td>\n</tr>\n<tr>\n<td>bbb</td>\n</tr>\n<tr>\n<td>-\v</td>\n</tr>\n", 29999);
            expected += "</tbody>\n</table>\n";
            return html == expected;
        case Case::ReferenceCollisions: {
            const auto row = "<p>[" + first_collision_key() + "]</p>\n";
            expected.clear(); append_repeat(expected, row, 49999);
            return contains(html, expected);
        }
    }
    return false;
}

void run_case(Case which) {
    const auto input = make_input(which);
    auto dialect = folia::test::commonmark::dialect();
    dialect.gfm.tables = true;
    dialect.tables = {true, true};
    const auto parsed = parse_text(1, input, dialect);
    std::string html;
    if (which == Case::NestedBlockquotes || which == Case::DeepLists) {
        const auto fixture = which == Case::NestedBlockquotes
            ? folia::test::gfm_capacity::ContainerFixture::NestedBlockquotes
            : folia::test::gfm_capacity::ContainerFixture::DeepLists;
        const auto projected = folia::test::gfm_capacity::project_pathological_containers(
            parsed.document.root, fixture);
        expect(projected.has_value()) << "unexpected AST/CST/source shape for iterative container projector";
        html = projected.value_or(std::string{});
    } else if (which == Case::NestedStrongEmph) {
        html = project_deep_emphasis(parsed.document.root);
    } else {
        html = folia::test::commonmark::project(parsed.document.root).projected_html;
    }
    const auto semantics_match = has_expected_semantics(which, html);
    expect(semantics_match) << "pinned cmark-gfm semantic output pattern; HTML bytes=" << html.size()
        << "; head=" << html.substr(0, 160)
        << "; tail=" << html.substr(html.size() > 160 ? html.size() - 160 : 0);
    const auto saved = serialize_markdown(parsed.document);
    expect(saved == input) << "character-exact Markdown save, input bytes=" << input.size();

    std::size_t inline_nodes = 0;
    expect(folia::test::gfm_capacity::pathological_inline_sources_lossless(
        parsed.document.root, inline_nodes));
    expect(inline_nodes > 0_u) << "parser produced editable content nodes";
}

#define GFM_PATH_TEST(name, id) name##_test = [] { run_case(Case::id); }

} // namespace

boost::ut::suite gfm_pathological_tests = [] {
    GFM_PATH_TEST("gfm_pathological_nested_strong_emphasis_65000", NestedStrongEmph);
    GFM_PATH_TEST("gfm_pathological_emphasis_closers_65000", ManyEmphClosers);
    GFM_PATH_TEST("gfm_pathological_emphasis_openers_65000", ManyEmphOpeners);
    GFM_PATH_TEST("gfm_pathological_link_closers_65000", ManyLinkClosers);
    GFM_PATH_TEST("gfm_pathological_link_openers_65000", ManyLinkOpeners);
    GFM_PATH_TEST("gfm_pathological_mismatched_markers_50000", MismatchedOpenersClosers);
    GFM_PATH_TEST("gfm_pathological_marker_runs_50000", DelimiterRunsMultipleOfThree);
    GFM_PATH_TEST("gfm_pathological_link_and_emphasis_markers_50000", LinkOpenersEmphClosers);
    GFM_PATH_TEST("gfm_pathological_bracket_paren_80000", RepeatedBracketParen);
    GFM_PATH_TEST("gfm_pathological_image_link_160000", RepeatedImageLink);
    GFM_PATH_TEST("gfm_pathological_hard_link_emphasis", HardLinkEmph);
    GFM_PATH_TEST("gfm_pathological_nested_brackets_50000", NestedBrackets);
    GFM_PATH_TEST("gfm_pathological_nested_blockquotes_50000", NestedBlockquotes);
    GFM_PATH_TEST("gfm_pathological_deep_lists_1000", DeepLists);
    GFM_PATH_TEST("gfm_pathological_embedded_nul", EmbeddedNul);
    GFM_PATH_TEST("gfm_pathological_backticks_4999_runs", Backticks);
    GFM_PATH_TEST("gfm_pathological_unclosed_angle_links_30000", UnclosedLinksAngle);
    GFM_PATH_TEST("gfm_pathological_unclosed_paren_links_30000", UnclosedLinksParen);
    GFM_PATH_TEST("gfm_pathological_unclosed_comments_300000", UnclosedComments);
    GFM_PATH_TEST("gfm_pathological_tables_30000_rows", Tables);
    GFM_PATH_TEST("gfm_pathological_reference_collisions_50000", ReferenceCollisions);
};

#undef GFM_PATH_TEST
