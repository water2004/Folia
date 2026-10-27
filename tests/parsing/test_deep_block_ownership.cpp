#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>
#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.container_source;
import folia.markdown.ids;
import folia.markdown.inline_cst;
using namespace folia;
using namespace boost::ut;
static_assert(std::is_nothrow_move_constructible_v<BlockNode>);
static_assert(std::is_nothrow_move_assignable_v<BlockNode>);
static_assert(std::is_same_v<BlockVec, std::vector<BlockNode>>);

namespace {
BlockNode make_quote_ownership_tree(std::uint64_t depth) {
    BlockNode node;
    node.id = NodeId{depth+2}; node.kind = BlockKind::Paragraph;
    node.inline_content.source = U"a";
    node.inline_content.tree.nodes.push_back(InlineCstNode{NodeId{depth+3}, InlineCstKind::Text, {0,1}, ParseStatus::Complete});
    node.inline_content.tree.tokens.push_back(InlineToken{TokenKind::Text,1});
    for (std::uint64_t index=depth; index>0; --index) {
        BlockNode parent;
        parent.id = NodeId{index+1}; parent.kind = BlockKind::BlockQuote;
        parent.ensure_container_source().lines.push_back({
            {NodeId{depth+2}, ContainerAnchorPart::Source, 0}, U"> ", false});
        parent.children.push_back(std::move(node));
        node = std::move(parent);
    }
    BlockNode root;
    root.id = NodeId{1}; root.kind = BlockKind::Document;
    root.children.push_back(std::move(node));
    return root;
}
bool same_owned_quote_tree(BlockNode const& first, BlockNode const& second, std::size_t depth) {
    auto* a=&first; auto* b=&second;
    for (std::size_t index=0; index<depth+1; ++index) {
        if (a->id != b->id || a->kind != b->kind || a->children.size()!=1 || b->children.size()!=1) return false;
        if (a->payload) {
            if (!b->payload || a->payload.get()==b->payload.get()) return false;
            const auto& left=a->container_source().lines; const auto& right=b->container_source().lines;
            if (left.size()!=1 || right.size()!=1 || left[0].text!=right[0].text
                || left[0].anchor.owner_id!=right[0].anchor.owner_id) return false;
        }
        a=&a->children[0]; b=&b->children[0];
    }
    return a->id==b->id && a->kind==BlockKind::Paragraph && b->kind==BlockKind::Paragraph
        && a->inline_content.source==U"a" && b->inline_content.source==U"a"
        && a->children.empty() && b->children.empty();
}
}
suite deep_block_ownership = [] {
    "deep_block_ownership_copy_move_clear_and_traversal"_test = [] {
        constexpr std::uint64_t depth=50000;
        auto original=make_quote_ownership_tree(depth);
        auto copy=original;
        expect(same_owned_quote_tree(original,copy,depth));
        auto assigned=make_quote_ownership_tree(depth);
        assigned=original;
        expect(same_owned_quote_tree(original,assigned,depth));
        const auto* allocation=assigned.children.data();
        auto moved=std::move(assigned);
        expect(moved.children.data()==allocation);
        std::uint64_t expected=1;
        bool ordered=true;
        walk_blocks(moved,[&](BlockNode const& node) { ordered = ordered && node.id.v==expected++; });
        expect(ordered && expected==depth+3);
        const auto path=block_path(moved,NodeId{depth+2});
        expect(path && path->size()==depth+1);
        expect(!block_path(moved,NodeId{depth+99}));
        EditorDocument document;
        document.root=std::move(moved);
        rebuild_document_block_index(document);
        expect(document.cached_block_locators.size()==depth+2);
        expect(document.cached_editable_order.size()==1u);
        const auto cached_path=cached_document_block_path(document,NodeId{depth+2});
        expect(cached_path==path);
        document.root.children.clear();
        expect(document.root.children.empty());
    };
};
