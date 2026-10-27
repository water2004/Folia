#include <cstdint>
#include <type_traits>
#include <utility>
#include "support/folia_test.hpp"
import folia.markdown.inline_cst;
import folia.markdown.ids;
import folia.markdown.text_edit;
using namespace folia;
using namespace boost::ut;
static_assert(std::is_aggregate_v<InlineCstNode>);
static_assert(std::is_nothrow_move_constructible_v<InlineCstNode>);
static_assert(std::is_nothrow_move_assignable_v<InlineCstNode>);

namespace {
InlineCstNodes make_deep_owner(std::size_t depth) {
    InlineCstNodes nodes;
    nodes.push_back(InlineCstNode{NodeId{1}, InlineCstKind::Text, {depth, depth+1}, ParseStatus::Complete});
    for (std::size_t index=0; index<depth; ++index) {
        InlineCstNode parent;
        parent.id = NodeId{index+2};
        parent.kind = InlineCstKind::Emphasis;
        parent.range = {depth-index-1, depth+index+2};
        parent.ensure_delimiter_ranges() = {parent.range,
            {parent.range.start, parent.range.start+1},
            {parent.range.start+1, parent.range.end-1},
            SourceRange{parent.range.end-1, parent.range.end}};
        parent.children = std::move(nodes);
        nodes.push_back(std::move(parent));
    }
    return nodes;
}
std::size_t check_copy_chain(InlineCstNodes const& original, InlineCstNodes const& copy) {
    auto* a=&original; auto* b=&copy;
    std::size_t count=0;
    while (!a->empty()) {
        if (a->size()!=1 || b->size()!=1) return 0;
        auto const& first=a->front(); auto const& second=b->front();
        if (first.id != second.id || first.kind != second.kind
            || first.range.start != second.range.start || first.range.end != second.range.end
            || first.delimited != second.delimited || first.semantic != second.semantic) return 0;
        ++count;
        a=&first.children; b=&second.children;
    }
    return b->empty() ? count : 0;
}
}
suite inline_cst_ownership = [] {
    "inline_cst_deep_owner_copy_move_and_clear"_test = [] {
        auto original = make_deep_owner(130000);
        const auto copied = original;
        expect(check_copy_chain(original, copied) == 130001u);
        InlineCstNodes assigned;
        assigned = original;
        expect(check_copy_chain(original, assigned) == 130001u);
        const auto* allocation = assigned.data();
        InlineCstNodes moved = std::move(assigned);
        expect(moved.data() == allocation);
        auto replaced = make_deep_owner(130000);
        replaced = std::move(moved);
        expect(replaced.data() == allocation);
        expect(check_copy_chain(original, replaced) == 130001u);
        replaced.clear();
        expect(replaced.empty());
    };
};
