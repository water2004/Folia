// Physical flow fragments do not change the semantic Markdown block tree.
// Boundaries occur only between complete block leaves, never inside a shaping
// paragraph, an inline CST, a table, or an atomic embedded block.
export module folia.core.render_fragments;
import std;
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.block_tree;
import folia.markdown.document;

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

template <typename ContextKey>
inline std::vector<RenderFragment> plan_render_fragments(
    EditorDocument const& document, ContextKey&& context_key,
    std::span<RenderFragment const> previous = {}) {
    // Existing cuts are anchored to node identities, not "every Nth item".
    // Inserting one list item may split its page, but never shifts all later
    // cuts. Undersized neighbors are deliberately not merged on the hot path.
    std::unordered_set<std::uint64_t> boundaries;
    boundaries.reserve(previous.size());
    for (auto const& fragment : previous)
        if (fragment.id != fragment.source_root) boundaries.insert(fragment.id.v);
    std::vector<RenderFragment> result;
    result.reserve(document.root.children.size());
    for (auto const& root : document.root.children) {
        auto first_fragment = result.size();
        BlockPath path;
        RenderFragment pending;
        std::size_t count = 0;
        auto flush = [&] {
            if (count == 0) return;
            result.push_back(std::move(pending));
            pending = {};
            count = 0;
        };
        auto visit = [&](auto& self, BlockNode const& node, std::uint64_t parent_key,
                         bool first_child, bool last_child, std::optional<std::size_t> ordinal) -> void {
            auto key = context_key(node, parent_key, first_child, last_child, ordinal);
            if (is_fragment_flow_container(node.kind) && !node.children.empty()) {
                for (std::size_t index = 0; index < node.children.size(); ++index) {
                    path.push_back(index);
                    auto ordered = (node.kind == BlockKind::List || node.kind == BlockKind::TaskList)
                        && node.list_special().ordered;
                    self(self, node.children[index], key, index == 0,
                        index + 1 == node.children.size(),
                        ordered ? std::optional<std::size_t>{index} : std::nullopt);
                    path.pop_back();
                }
                return;
            }
            if (count != 0 && boundaries.contains(node.id.v)) flush();
            if (count == 0) {
                pending.structure_key = 1469598103934665603ull;
                pending.id = node.id;
                pending.source_root = root.id;
                pending.first_leaf = path;
                pending.first = result.size() == first_fragment;
            }
            pending.structure_key ^= key;
            pending.structure_key *= 1099511628211ull;
            ++pending.leaf_count;
            pending.last_leaf = path;
            pending.last = false;
            if (++count == render_fragment_leaf_limit) flush();
        };
        visit(visit, root, 0, true, true, std::nullopt);
        flush();
        if (result.size() == first_fragment) continue;
        result.back().last = true;
        if (result.size() == first_fragment + 1) {
            // Small blocks retain their identity and exactly the same flow.
            result.back().id = root.id;
            result.back().first_leaf.clear();
            result.back().last_leaf.clear();
        }
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
