// Physical flow fragments do not change the semantic Markdown block tree.
// Boundaries occur only between complete block leaves, never inside a shaping
// paragraph, an inline CST, a table, or an atomic embedded block.
export module folia.core.render_fragments;
import std;
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.instrumentation;

export namespace folia {

inline constexpr std::size_t render_fragment_leaf_limit = 64;

inline bool is_fragment_flow_container(BlockKind kind) {
    switch (kind) {
        case BlockKind::Document:
        case BlockKind::HtmlContainer:
        case BlockKind::List:
        case BlockKind::TaskList:
        case BlockKind::ListItem:
        case BlockKind::TaskListItem:
        case BlockKind::BlockQuote:
        case BlockKind::Callout:
        case BlockKind::FootnoteDefinition:
            return true;
        default:
            return false;
    }
}

struct RenderFragment {
    NodeId id{};              // stable physical identity, never a text coordinate
    NodeId source_root{};     // unchanged semantic top-level block
    BlockPath first_leaf;     // inclusive, relative to source_root
    BlockPath last_leaf;      // inclusive, relative to source_root
    NodeId first_leaf_id{};
    NodeId last_leaf_id{};
    NodeId first_owner{};
    NodeId last_owner{};
    bool first = true;
    bool last = true;
    std::uint64_t structure_key = 0;
    std::size_t leaf_count = 0;

    bool operator==(RenderFragment const&) const = default;
};

// A non-owning cursor into a descriptor. Empty paths mean the complete node.
// Advancing to a child is O(1); materialization never scans excluded siblings.
struct RenderFragmentRange {
    std::span<std::size_t const> first;
    std::span<std::size_t const> last;

    std::size_t begin() const { return first.empty() ? 0 : first.front(); }
    std::size_t end(std::size_t child_count) const {
        return last.empty() ? child_count : (std::min)(child_count, last.front() + 1);
    }
    RenderFragmentRange child(std::size_t index) const {
        return {
            !first.empty() && index == first.front() ? first.subspan(1) : std::span<std::size_t const>{},
            !last.empty() && index == last.front() ? last.subspan(1) : std::span<std::size_t const>{},
        };
    }
    bool starts_node() const {
        return std::ranges::all_of(first, [](auto index) { return index == 0; });
    }
    bool ends_node(BlockNode const& node) const {
        auto const* current = &node;
        for (auto index : last) {
            if (index + 1 != current->children.size()) return false;
            current = &current->children[index];
        }
        return true;
    }
};

inline RenderFragmentRange fragment_range(RenderFragment const& fragment) {
    return {fragment.first_leaf, fragment.last_leaf};
}

namespace render_fragment_detail {

// This is also used for a local structural splice. The cursor starts and ends
// at whole leaves; its ancestor context is computed from the authoritative
// tree, while excluded siblings are never visited.
template <typename ContextKey>
inline std::vector<RenderFragment> plan_interval(
    BlockNode const& root, ContextKey& context_key, RenderFragmentRange range,
    std::unordered_set<std::uint64_t> const& boundaries, bool balance_overflow) {
    struct Leaf {
        NodeId id{};
        BlockPath path;
        std::uint64_t key = 0;
        NodeId first_owner{};
        NodeId last_owner{};
    };
    std::vector<RenderFragment> result;
    std::vector<Leaf> pending;
    if (balance_overflow) pending.reserve(2 * render_fragment_leaf_limit + 1);
    RenderFragment streaming;
    BlockPath path;
    auto flush_streaming = [&] {
        if (streaming.leaf_count == 0) return;
        result.push_back(std::move(streaming));
        streaming = {};
    };
    auto flush = [&](std::size_t count) {
        if (count == 0) return;
        RenderFragment fragment;
        fragment.id = pending.front().id;
        fragment.source_root = root.id;
        fragment.first_leaf = pending.front().path;
        fragment.last_leaf = pending[count - 1].path;
        fragment.first_leaf_id = pending.front().id;
        fragment.last_leaf_id = pending[count - 1].id;
        fragment.first = result.empty() && range.starts_node();
        fragment.last = false;
        fragment.leaf_count = count;
        fragment.structure_key = 1469598103934665603ull;
        for (std::size_t index = 0; index < count; ++index) {
            auto const& leaf = pending[index];
            fragment.structure_key ^= leaf.key;
            fragment.structure_key *= 1099511628211ull;
            if (fragment.first_owner.v == 0) fragment.first_owner = leaf.first_owner;
            if (leaf.last_owner.v != 0) fragment.last_owner = leaf.last_owner;
        }
        result.push_back(std::move(fragment));
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(count));
    };
    auto finish_segment = [&] {
        // A 65-leaf insertion produces 33/32, not 64/1. At most two pages
        // of metadata are buffered, including when a large subtree is pasted.
        while (pending.size() > render_fragment_leaf_limit)
            flush((pending.size() + 1) / 2);
        flush(pending.size());
    };
    auto visit = [&](auto& self, BlockNode const& node, RenderFragmentRange selected,
                     std::uint64_t parent_key, bool first_child, bool last_child,
                     std::optional<std::size_t> ordinal) -> void {
        record_render_fragment_plan_node();
        auto key = context_key(node, parent_key, first_child, last_child, ordinal);
        if (is_fragment_flow_container(node.kind) && !node.children.empty()) {
            for (std::size_t index = selected.begin(); index < selected.end(node.children.size()); ++index) {
                path.push_back(index);
                auto ordered = (node.kind == BlockKind::List || node.kind == BlockKind::TaskList)
                    && node.list_special().ordered;
                self(self, node.children[index], selected.child(index), key, index == 0,
                    index + 1 == node.children.size(),
                    ordered ? std::optional<std::size_t>{index} : std::nullopt);
                path.pop_back();
            }
            return;
        }
        if (!balance_overflow) {
            // Initial planning only needs the first and last path of each
            // page. Avoid one tiny path allocation per document leaf merely
            // to support the bounded lookahead used by a later local split.
            if (streaming.leaf_count != 0 && boundaries.contains(node.id.v)) flush_streaming();
            if (streaming.leaf_count == 0) {
                streaming.id = node.id;
                streaming.source_root = root.id;
                streaming.first_leaf = path;
                streaming.first_leaf_id = node.id;
                streaming.first = result.empty() && range.starts_node();
                streaming.last = false;
                streaming.structure_key = 1469598103934665603ull;
            }
            streaming.last_leaf = path;
            streaming.last_leaf_id = node.id;
            streaming.structure_key ^= key;
            streaming.structure_key *= 1099511628211ull;
            if (streaming.first_owner.v == 0) streaming.first_owner = first_editable_subtree_owner(node);
            if (auto owner = last_editable_subtree_owner(node); owner.v != 0) streaming.last_owner = owner;
            if (++streaming.leaf_count == render_fragment_leaf_limit) flush_streaming();
            return;
        }
        if (!pending.empty() && boundaries.contains(node.id.v)) finish_segment();
        pending.push_back({node.id, path, key,
            first_editable_subtree_owner(node), last_editable_subtree_owner(node)});
        if (pending.size() > 2 * render_fragment_leaf_limit)
            flush(render_fragment_leaf_limit);
    };
    visit(visit, root, range, 0, true, true, std::nullopt);
    if (balance_overflow) finish_segment();
    else flush_streaming();
    if (result.empty()) return result;
    result.back().last = range.ends_node(root);
    if (result.size() == 1 && result.front().first && result.front().last) {
        result.front().id = root.id;
        result.front().first_leaf.clear();
        result.front().last_leaf.clear();
    }
    return result;
}

inline std::optional<BlockPath> leaf_path(
    EditorDocument const& document, NodeId root, NodeId leaf) {
    auto path = cached_document_block_path(document, leaf);
    if (!path || path->empty()) return std::nullopt;
    auto const* node = block_at_path(document.root, *path);
    if (!node || node->id != leaf || document.root.children[path->front()].id != root)
        return std::nullopt;
    path->erase(path->begin());
    return path;
}

inline BlockPath edge_leaf_path(BlockNode const& root, bool last) {
    BlockPath path;
    auto const* node = &root;
    while (is_fragment_flow_container(node->kind) && !node->children.empty()) {
        auto index = last ? node->children.size() - 1 : 0;
        path.push_back(index);
        node = &node->children[index];
    }
    return path;
}

inline std::optional<BlockPath> adjacent_leaf_path(
    BlockNode const& root, BlockPath path, bool next) {
    while (!path.empty()) {
        auto index = path.back();
        path.pop_back();
        auto const* parent = block_at_path(root, path);
        if (!parent) return std::nullopt;
        if ((next && index + 1 < parent->children.size()) || (!next && index > 0)) {
            index = next ? index + 1 : index - 1;
            path.push_back(index);
            auto tail = edge_leaf_path(parent->children[index], !next);
            path.insert(path.end(), tail.begin(), tail.end());
            return path;
        }
    }
    return std::nullopt;
}

} // namespace render_fragment_detail

template <typename ContextKey>
inline std::vector<RenderFragment> plan_render_fragments(
    EditorDocument const& document, ContextKey&& context_key,
    std::span<RenderFragment const> previous = {}) {
    // Existing cuts are anchored to node identities, not "every Nth item".
    std::unordered_set<std::uint64_t> boundaries;
    boundaries.reserve(previous.size());
    for (auto const& fragment : previous)
        if (fragment.id != fragment.source_root) boundaries.insert(fragment.id.v);
    std::vector<RenderFragment> result;
    result.reserve(document.root.children.size());
    for (auto const& root : document.root.children) {
        auto fragments = render_fragment_detail::plan_interval(root, context_key, {}, boundaries, false);
        result.insert(result.end(), std::make_move_iterator(fragments.begin()),
            std::make_move_iterator(fragments.end()));
    }
    return result;
}

template <typename Visitor>
inline void visit_render_fragment(
    BlockNode const& node, RenderFragmentRange range, Visitor&& visitor) {
    visitor(node, range);
    auto begin = range.begin();
    auto end = range.end(node.children.size());
    for (auto index = begin; index < end; ++index)
        visit_render_fragment(node.children[index], range.child(index), visitor);
}

inline std::unordered_map<std::uint64_t, std::size_t> index_render_fragment_owners(
    EditorDocument const& document, std::span<RenderFragment const> fragments) {
    std::unordered_map<std::uint64_t, std::size_t> result;
    result.reserve(document.cached_editable_order.size());
    for (std::size_t index = 0; index < fragments.size(); ++index) {
        auto const& fragment = fragments[index];
        auto const* root = find_document_block(document, fragment.source_root);
        if (!root) continue;
        visit_render_fragment(*root, fragment_range(fragment), [&](auto const& node, auto) {
            if (is_editable_block_owner(node.kind)) result.emplace(node.id.v, index);
        });
    }
    return result;
}

}
