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

suite parsing_inline_cst_source_edits_tests = [] {

"text edit applies source replacement and affinity"_test = [] {
    std::u32string source = U"**abc**";
    const TextEdit edit{NodeId{7}, {2, 5}, U"xyz"};
    expect(apply_text_edit(source, edit) == 0_i);
    expect(source == U"**xyz**");

    const TextEdit insertion{NodeId{7}, {2, 2}, U"Q"};
    expect(translate_position(TextPosition{NodeId{7}, 2, TextAffinity::Upstream}, insertion).source_offset == 2_u);
    expect(translate_position(TextPosition{NodeId{7}, 2, TextAffinity::Downstream}, insertion).source_offset == 3_u);
    expect(translate_position(TextPosition{NodeId{8}, 2, TextAffinity::Downstream}, insertion).source_offset == 2_u);

    bool rejected = false;
    try {
        apply_text_edit(source, TextEdit{NodeId{7}, {0, 99}, U""});
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    expect(rejected);
};

"selection_affinity_does_not_create_a_source_range"_test = [] {
    const auto owner = NodeId{7};
    const TextSelection selection{
        {owner, 2, TextAffinity::Upstream},
        {owner, 2, TextAffinity::Downstream},
    };
    expect(selection.is_caret());
    expect(selection.anchor != selection.active);
};

"source edit preserves complete formatting wrappers and untouched identities"_test = [] {
    InlineDocument document{U"left **middle** right", {}};
    auto initial = test_context();
    initial.next_id = NodeId{100};
    reparse_inline_document(document, initial);
    expect(document.tree.nodes.size() == 3_u);
    if (document.tree.nodes.size() != 3) return;

    const auto left_id = document.tree.nodes.front().id;
    const auto styled_id = document.tree.nodes[1].id;
    const auto literal_id = document.tree.nodes[1].children.front().id;
    const auto right_id = document.tree.nodes.back().id;
    std::uint64_t allocated = 10000;
    auto reparsing = test_context();
    reparsing.allocate_id = [&] { return NodeId{allocated++}; };

    const TextEdit edit{NodeId{7}, {8, 8}, U"X"};
    const auto applied = apply_inline_source_edit(NodeId{7}, document, edit, reparsing);
    expect(document.source == U"left **mXiddle** right");
    expect(document.tree.nodes.size() == 3_u);
    if (document.tree.nodes.size() == 3) {
        expect(document.tree.nodes.front().id == left_id);
        expect(document.tree.nodes[1].id == styled_id);
        expect(document.tree.nodes[1].children.front().id == literal_id);
        expect(document.tree.nodes.back().id == right_id);
        expect(document.tree.nodes.back().range == SourceRange{16, 22});
    }
    // Editing a literal descendant does not create a new formatting construct.
    expect(allocated == 10000_u);
    expect(applied.inline_identities && applied.inline_identities->preserved);
    expect(applied.inverse.container_id == NodeId{7});
    expect(applied.inverse.range == SourceRange{8, 9});
    expect(applied.inverse.replacement.empty());
    expect(is_lossless(document.source));
};

"indexed inline identity reconciliation preserves scalar matching semantics"_test = [] {
    auto scalar = [&](auto const& self, InlineCstNodes const& before, InlineCstNodes& after,
        std::u32string_view oldSource, std::u32string_view newSource, TextEdit const& edit) -> void {
        std::vector<bool> claimed(after.size());
        for (auto const& old : before) {
            auto range = inline_source_edit_detail::translated_untouched_range(old.range, edit);
            if (!range) continue;
            for (std::size_t i = 0; i < after.size(); ++i) {
                auto& node = after[i];
                if (claimed[i] || node.kind != old.kind || node.status != old.status || node.range != *range
                    || !inline_source_edit_detail::same_source_slice(oldSource, old.range, newSource, node.range)) continue;
                node.id = old.id; claimed[i] = true;
                self(self, old.children, node.children, oldSource, newSource, edit);
                break;
            }
        }
    };
    std::vector<std::u32string> sources{U"a***b***c\nleft **middle** right", U"[title](<url>) and $x_{1}$",
        U"**abc", U"a\\**b* &amp; `code`", U"*a* _a_ ~~abc~~\r\nend", U"![alt](url) [title]("};
    std::mt19937 random(274913);
    for (int trial = 0; trial < 300; ++trial) {
        auto const& source = sources[random() % sources.size()];
        InlineDocument document{source, {}};
        auto context = test_context(); context.next_id = NodeId{100};
        reparse_inline_document(document, context);
        auto before = document.tree;
        auto start = random() % (source.size() + 1);
        auto end = (std::min)(source.size(), start + random() % 5);
        std::vector<std::u32string> replacements{U"x", U"", U"**", U"\n", U"$", U"\U0001f642"};
        TextEdit edit{NodeId{7}, {start, end}, replacements[random() % replacements.size()]};
        context.next_id = NodeId{10000};
        auto removed = document.source.substr(edit.range.start, edit.range.length());
        apply_text_edit(document.source, edit);
        document.tree = parse_inline(document.source, context);
        auto expected = document.tree;
        inline_source_edit_detail::reconcile_nodes(before.nodes, document.tree.nodes, source, document.source, edit);
        scalar(scalar, before.nodes, expected.nodes, source, document.source, edit);
        expect(std::ranges::equal(inline_source_edit_detail::node_identities(document.tree.nodes),
            inline_source_edit_detail::node_identities(expected.nodes)));
        expect(bool(flatten_tokens(document.tree, document.source) == document.source));
        apply_inline_source_edit(NodeId{7}, document,
            {NodeId{7}, {edit.range.start, edit.range.start + edit.replacement.size()}, removed}, context);
        expect(bool(document.source == source));
        expect(bool(flatten_tokens(document.tree, document.source) == source));
    }
};

"source edit reconciles a long paragraph without changing untouched identities"_test = [] {
    InlineDocument document;
    for (int line = 0; line < 20000; ++line) document.source += U"alpha **beta** gamma\n";
    auto source = document.source;
    auto context = test_context(); context.next_id = NodeId{100};
    reparse_inline_document(document, context);
    auto finalId = document.tree.nodes.back().id;
    context.next_id = NodeId{1000000};
    auto applied = apply_inline_source_edit(NodeId{7}, document, {NodeId{7}, {3, 3}, U"X"}, context);
    expect(document.tree.nodes.back().id == finalId);
    expect(document.source.size() == source.size() + 1);
    expect(bool(flatten_tokens(document.tree, document.source) == document.source));
    apply_inline_source_edit(NodeId{7}, document, applied.inverse, context);
    expect(bool(document.source == source));
    expect(document.tree.nodes.back().id == finalId);
};

"source edit rejects the wrong owner"_test = [] {
    InlineDocument document{U"abc", {}};
    reparse_inline_document(document, test_context());
    bool rejected = false;
    try {
        apply_inline_source_edit(NodeId{1}, document, TextEdit{NodeId{2}, {0, 0}, U"x"}, test_context());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected);
    expect(document.source == U"abc");
};

};
