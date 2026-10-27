// Uniform structural operations for the Markdown block tree.
export module folia.markdown.block_tree;
import std;
import folia.markdown.ast;
import folia.markdown.ids;

export namespace folia {

using BlockPath = std::vector<std::size_t>;

inline BlockNode* block_at_path(BlockNode& root, const BlockPath& path) {
    auto* node = &root;
    for (const auto index : path) {
        if (index >= node->children.size()) return nullptr;
        node = &node->children[index];
    }
    return node;
}

inline const BlockNode* block_at_path(const BlockNode& root, const BlockPath& path) {
    const auto* node = &root;
    for (const auto index : path) {
        if (index >= node->children.size()) return nullptr;
        node = &node->children[index];
    }
    return node;
}

inline std::optional<BlockPath> block_path(const BlockNode& root, NodeId id) {
    if (root.id == id) return BlockPath{};
    struct Frame { const BlockNode* node; std::size_t next; };
    std::vector<Frame> frames{{&root, 0}};
    BlockPath path;
    while (!frames.empty()) {
        auto& frame = frames.back();
        if (frame.next == frame.node->children.size()) {
            frames.pop_back();
            if (!path.empty()) path.pop_back();
            continue;
        }
        const auto index = frame.next++;
        const auto& child = frame.node->children[index];
        path.push_back(index);
        if (child.id == id) return path;
        if (child.children.empty()) path.pop_back();
        else frames.push_back({&child, 0});
    }
    return std::nullopt;
}

inline BlockNode* find_block(BlockNode& root, NodeId id) {
    const auto path = block_path(root, id);
    return path ? block_at_path(root, *path) : nullptr;
}

inline const BlockNode* find_block(const BlockNode& root, NodeId id) {
    const auto path = block_path(root, id);
    return path ? block_at_path(root, *path) : nullptr;
}

inline BlockNode* find_parent_block(BlockNode& root, NodeId id) {
    auto path = block_path(root, id);
    if (!path || path->empty()) return nullptr;
    path->pop_back();
    return block_at_path(root, *path);
}

inline const BlockNode* find_parent_block(const BlockNode& root, NodeId id) {
    auto path = block_path(root, id);
    if (!path || path->empty()) return nullptr;
    path->pop_back();
    return block_at_path(root, *path);
}

inline BlockNode* callout_title_block(BlockNode& callout) {
    if (callout.kind != BlockKind::Callout || callout.children.empty()
        || callout.children.front().kind != BlockKind::CalloutTitle) return nullptr;
    return &callout.children.front();
}

inline const BlockNode* callout_title_block(const BlockNode& callout) {
    if (callout.kind != BlockKind::Callout || callout.children.empty()
        || callout.children.front().kind != BlockKind::CalloutTitle) return nullptr;
    return &callout.children.front();
}

inline std::size_t callout_body_start(const BlockNode& callout) {
    return callout_title_block(callout) ? 1u : 0u;
}

template <class Visitor>
inline void walk_blocks(BlockNode& root, Visitor&& visitor) {
    struct Frame { BlockNode* node; std::size_t next; };
    visitor(root);
    std::vector<Frame> frames{{&root, 0}};
    while (!frames.empty()) {
        auto& frame = frames.back();
        if (frame.next == frame.node->children.size()) { frames.pop_back(); continue; }
        auto& child = frame.node->children[frame.next++];
        visitor(child);
        if (!child.children.empty()) frames.push_back({&child, 0});
    }
}

template <class Visitor>
inline void walk_blocks(const BlockNode& root, Visitor&& visitor) {
    struct Frame { const BlockNode* node; std::size_t next; };
    visitor(root);
    std::vector<Frame> frames{{&root, 0}};
    while (!frames.empty()) {
        auto& frame = frames.back();
        if (frame.next == frame.node->children.size()) { frames.pop_back(); continue; }
        const auto& child = frame.node->children[frame.next++];
        visitor(child);
        if (!child.children.empty()) frames.push_back({&child, 0});
    }
}

inline bool insert_block(BlockNode& parent, std::size_t index, BlockNode node) {
    if (index > parent.children.size()) return false;
    parent.children.insert(parent.children.begin() + static_cast<std::ptrdiff_t>(index), std::move(node));
    return true;
}

inline std::optional<BlockNode> remove_block(BlockNode& parent, std::size_t index) {
    if (index >= parent.children.size()) return std::nullopt;
    BlockNode removed = std::move(parent.children[index]);
    parent.children.erase(parent.children.begin() + static_cast<std::ptrdiff_t>(index));
    return removed;
}

inline bool replace_block(BlockNode& parent, std::size_t index, BlockNode replacement) {
    if (index >= parent.children.size()) return false;
    parent.children[index] = std::move(replacement);
    return true;
}

inline bool move_block(BlockNode& from_parent, std::size_t from_index, BlockNode& to_parent, std::size_t to_index) {
    if (from_index >= from_parent.children.size() || to_index > to_parent.children.size()) return false;
    auto node = remove_block(from_parent, from_index);
    if (!node) return false;
    if (&from_parent == &to_parent && to_index > from_index) --to_index;
    return insert_block(to_parent, to_index, std::move(*node));
}

} // namespace folia
